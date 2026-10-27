using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
#if UNITY_EDITOR
using UnityEditor;
#endif
using Mmorpg.Client;
using ClientCore;
using ClientCore.PacketHelper;
using ClientCore.Services;
using NUnit.Framework;
using TMPro;
using UnityEngine;
using UnityEngine.UI;

namespace Mmorpg.Client.Tests
{
    // Detached fixtures: not actual keyboard/button input, a server cast, or a live death.
    public sealed class CombatTests
    {
        public static string RunPureChecks()
        {
            var tests = new CombatTests();
            tests.AtomicCastKeepsWholeSnapshot_ValidatesBeforeMutation_ThenResetsSkill();
            tests.ConcurrentActionReadersNeverObserveMixedCastCoordinatesOrSlot();
            tests.ConcurrentCancellationCannotTurnPendingCastIntoMovement_AndNewInputStillWorks();
            tests.QueuedActionResultsCaptureSubscribers_AndDisposeSuppressesLateDelivery();
            tests.ActionResultWireIs21Bytes_AndMalformedCountsAreRejected();
            tests.OwnHpMpDeltaStopsPendingActionsAtZero_AndKeepsServerVitals();
            tests.AimTextUsesMovementDirections_AndPhasesAreOwnPlayerOnly_AndOldSessionOrDisabledCallbacksCannotReappear();
            return "C05 detached fixtures: 7/7 passed; no Play, real input, socket, server cast, or live HP0.";
        }

        [Test]
        public void AtomicCastKeepsWholeSnapshot_ValidatesBeforeMutation_ThenResetsSkill()
        {
            var data = NewData();
            Assert.That(((IViewDataModel)data).GetActionState().Item5, Is.EqualTo(255));
            data.UpdateMove(0, 11, 12);
            data.UpdateAction(3, 15, 16, 2);
            Assert.Throws<ArgumentException>(() => data.UpdateAction(4, 1, 2, 0));
            Assert.Throws<ArgumentException>(() => data.UpdateAction(0, float.NaN, 2, 0));
            Assert.Throws<ArgumentException>(() => data.UpdateAction(0, 1, float.PositiveInfinity, 0));
            Assert.Throws<ArgumentException>(() => data.UpdateAction(0, 1, 2, 3));
            data.UpdateMove(1, 99, 100);
            var state = ((IViewDataModel)data).GetActionState();
            Assert.That(state, Is.EqualTo((true, (byte)3, 15f, 16f, (byte)2, 0UL)));
            state = ((IViewDataModel)data).GetActionState();
            Assert.That(state.Item1, Is.False);
            Assert.That(state.Item5, Is.EqualTo(255));
            data.UpdateMove(2, 17, 18);
            Assert.That(((IViewDataModel)data).GetActionState(), Is.EqualTo((true, (byte)2, 17f, 18f, (byte)255, 0UL)));
        }

        [Test]
        public void ConcurrentActionReadersNeverObserveMixedCastCoordinatesOrSlot()
        {
            var data = NewData();
            var writer = Task.Run(() =>
            {
                for (int i = 0; i < 15000; i++)
                {
                    byte slot = (byte)(i % 3);
                    data.UpdateAction(slot, slot + 100, slot + 200, slot);
                }
            });
            for (int i = 0; i < 15000; i++)
            {
                var state = ((IViewDataModel)data).GetActionState();
                if (!state.Item1) continue;
                Assert.That(state.Item2, Is.EqualTo(state.Item5));
                Assert.That(state.Item3, Is.EqualTo(state.Item5 + 100));
                Assert.That(state.Item4, Is.EqualTo(state.Item5 + 200));
            }
            Assert.That(writer.Wait(2000), Is.True);
        }

        [Test]
        public void ConcurrentCancellationCannotTurnPendingCastIntoMovement_AndNewInputStillWorks()
        {
            var data = NewData();
            const int rounds = 2000;
            int leakedMovement = 0;
            using (var rendezvous = new Barrier(3))
            {
                var cancellation = Task.Factory.StartNew(() =>
                {
                    for (int i = 0; i < rounds; i++)
                    {
                        rendezvous.SignalAndWait();
                        data.CancelAction();
                        rendezvous.SignalAndWait();
                    }
                }, CancellationToken.None, TaskCreationOptions.LongRunning, TaskScheduler.Default);
                var clientTick = Task.Factory.StartNew(() =>
                {
                    for (int i = 0; i < rounds; i++)
                    {
                        rendezvous.SignalAndWait();
                        var state = ((IViewDataModel)data).GetActionState();
                        // A tick can win before cancellation and consume the original cast.
                        // Cancellation must never manufacture a dirty movement-only action.
                        if (state.Item1 && state.Item5 == 255) leakedMovement++;
                        rendezvous.SignalAndWait();
                    }
                }, CancellationToken.None, TaskCreationOptions.LongRunning, TaskScheduler.Default);
                for (int i = 0; i < rounds; i++)
                {
                    data.UpdateAction(3, 15, 16, 2);
                    rendezvous.SignalAndWait();
                    rendezvous.SignalAndWait();
                }
                Assert.That(Task.WaitAll(new[] { cancellation, clientTick }, 2000), Is.True);
            }
            Assert.That(leakedMovement, Is.Zero, "The two-lock UpdateSkill(255)/CancelMove path can expose dirty NONE_SKILL between calls.");
            var cancelled = ((IViewDataModel)data).GetActionState();
            Assert.That(cancelled.Item1, Is.False); Assert.That(cancelled.Item5, Is.EqualTo(255));
            data.UpdateMove(0, 15, 17);
            Assert.That(((IViewDataModel)data).GetActionState(), Is.EqualTo((true, (byte)0, 15f, 17f, (byte)255, 0UL)));
            data.UpdateAction(1, 15, 17, 1);
            data.CancelMove();
            Assert.That(((IViewDataModel)data).GetActionState(), Is.EqualTo((true, (byte)1, 15f, 17f, (byte)1, 0UL)),
                "Movement-only cancellation still preserves an independently queued cast.");
            data.UpdateAction(2, 15, 17, 0);
            data.CancelAction();
            data.UpdateAction(3, 15, 17, 2);
            Assert.That(((IViewDataModel)data).GetActionState(), Is.EqualTo((true, (byte)3, 15f, 17f, (byte)2, 0UL)));
        }

        [Test]
        public void QueuedActionResultsCaptureSubscribers_AndDisposeSuppressesLateDelivery()
        {
            var dispatcher = new QueuedDispatcher();
            var model = NewModel(NewData(), dispatcher);
            int old = 0, replacement = 0;
            Action<ushort, ActionResultField[]> handler = (_, __) => old++;
            model.OnActionResultReceived += handler;
            model.ActionResultReceived(1, 1, 0, Array.Empty<ActionResultField>());
            model.OnActionResultReceived -= handler;
            model.OnActionResultReceived += (_, __) => replacement++;
            dispatcher.Drain();
            Assert.That(old, Is.EqualTo(1));
            Assert.That(replacement, Is.Zero, "New subscriptions cannot receive previously queued results.");
            model.ActionResultReceived(1, 1, 0, Array.Empty<ActionResultField>());
            model.Dispose();
            dispatcher.Drain();
            model.ActionResultReceived(1, 1, 0, Array.Empty<ActionResultField>());
            Assert.That(dispatcher.Jobs.Count, Is.Zero, "Disposed model cannot enqueue more results.");
            Assert.That(replacement, Is.Zero);
        }

        [Test]
        public void ActionResultWireIs21Bytes_AndMalformedCountsAreRejected()
        {
            Assert.That(Marshal.SizeOf<ActionResultField>(), Is.EqualTo(21));
            var frame = new byte[41];
            BitConverter.GetBytes((ushort)0xABCD).CopyTo(frame, 0);
            BitConverter.GetBytes((uint)frame.Length).CopyTo(frame, 2);
            frame[6] = 12; frame[7] = 1; frame[8] = 1;
            BitConverter.GetBytes((ushort)1).CopyTo(frame, 10); BitConverter.GetBytes(1UL).CopyTo(frame, 12);
            frame[20] = 0; BitConverter.GetBytes(73U).CopyTo(frame, 21);
            frame[25] = 2; frame[26] = 3;
            BitConverter.GetBytes(10f).CopyTo(frame, 27); BitConverter.GetBytes(11f).CopyTo(frame, 31);
            BitConverter.GetBytes(2U).CopyTo(frame, 35); frame[39] = 1;
            var result = Packet.DeserializeVariablePacket<ActionResultField>(Packet.ExtractHeader(frame), frame);
            Assert.That(result.count, Is.EqualTo(1)); Assert.That(result.items[0].casterId, Is.EqualTo(73));
            Assert.That(result.items[0].skillPhase, Is.EqualTo(1)); Assert.That(result.items[0].y, Is.EqualTo(11));
            frame[8] = 2;
            Assert.Throws<ArgumentException>(() => Packet.DeserializeVariablePacket<ActionResultField>(Packet.ExtractHeader(frame), frame));
            var truncated = new byte[40]; Array.Copy(frame, truncated, 40); truncated[8] = 1;
            BitConverter.GetBytes((uint)truncated.Length).CopyTo(truncated, 2);
            Assert.Throws<ArgumentException>(() => Packet.DeserializeVariablePacket<ActionResultField>(Packet.ExtractHeader(truncated), truncated));
        }

        [Test]
        public void OwnHpMpDeltaStopsPendingActionsAtZero_AndKeepsServerVitals()
        {
            using (var f = new CombatFixture())
            {
                Assert.That(f.World.CanAct, Is.True);
                f.Data.UpdateAction(3, 10, 10, 2);
                Set(f.World, "visualPosition", f.World.ServerPosition + new Vector2(0.4f, -0.25f));
                f.Delta(new DeltaUpdateField { zoneInternalID = 73, fieldID = 1, fieldVal = 31 },
                    new DeltaUpdateField { zoneInternalID = 73, fieldID = 0, fieldVal = 0 });
                Assert.That(f.World.OwnFull.mp, Is.EqualTo(31)); Assert.That(f.World.OwnFull.hp, Is.Zero);
                Assert.That(f.Vitals.text, Does.Contain("HP 0 / 100").And.Contain("MP 31 / 50"));
                Assert.That(f.World.CanAct, Is.False);
                Assert.That((Vector2)Get(f.World, "visualPosition"), Is.EqualTo(f.World.ServerPosition),
                    "HP 0 stops local visual prediction at the latest server-confirmed position.");
                Assert.That(((IViewDataModel)f.Data).GetActionState().Item1, Is.False);
                Invoke(f.Combat, "RequestSkill", (byte)1);
                Assert.That(((IViewDataModel)f.Data).GetActionState().Item1, Is.False);
                foreach (var button in f.Buttons) Assert.That(button.interactable, Is.False);
                var defeatPanel = (GameObject)Get(f.Combat, "defeatPanel");
                Assert.That(defeatPanel != null && defeatPanel.activeSelf, Is.True);
                Assert.That(defeatPanel.transform.Find("Defeat Message/Title").GetComponent<TMP_Text>().text, Is.EqualTo("DEFEATED"));
                Assert.That(defeatPanel.transform.Find("Defeat Message/Message").GetComponent<TMP_Text>().text,
                    Does.Contain("Your character has fallen.").And.Contain("Return to town"));

                var respawn = defeatPanel.transform.Find("Defeat Message/Return to Town").GetComponent<UnityEngine.UI.Button>();
                Assert.That(respawn.GetComponentInChildren<TMP_Text>(true).text, Is.EqualTo("RETURN TO TOWN"));
                respawn.onClick.Invoke();
                Assert.That(f.World.OwnFull.hp, Is.Zero, "Only the server may restore HP, not the button.");
                Assert.That(f.World.CanAct, Is.False);
                f.Full();
                Assert.That(f.World.OwnFull.hp, Is.EqualTo(100));
                Assert.That(f.World.CanAct, Is.True, "A later actual Full owns the confirmed state; there is no local resurrection.");
                Assert.That(defeatPanel.activeSelf, Is.False, "A server-confirmed living state hides the defeat panel.");
                Set(f.Combat, "respawnDeadline", 999999d);
                respawn.interactable = false;
                Invoke(f.Combat, "HideDefeatPanel");

                f.Delta(new DeltaUpdateField { zoneInternalID = 73, fieldID = 0, fieldVal = 0 });
                Assert.That(f.World.OwnFull.hp, Is.Zero);
                Assert.That(defeatPanel.activeSelf, Is.True);
                Assert.That(respawn.interactable, Is.True, "A subsequent death must allow another recovery request.");
                var logout = defeatPanel.GetComponentInChildren<UnityEngine.UI.Button>(true);
                Assert.That(logout.GetComponentInChildren<TMP_Text>(true).text, Is.EqualTo("LOG OUT"));
                logout.onClick.Invoke();
                Assert.That(((TMP_Text)Get(Get(f.Combat, "loginScreen"), "statusText")).text, Does.Contain("Session closed."));
                Assert.That(f.World.Stage, Is.EqualTo(WorldStage.Ready));
                Assert.That(Get(f.Combat, "model"), Is.Null, "ConnectionFailed releases the bound model through ClientChanged(null).");
                Assert.That(defeatPanel.activeSelf, Is.False, "Leaving through the existing connection-close path hides the defeat panel.");
            }
        }

        [Test]
        [Category("PlayFeedbackRevision")]
        public void AimTextUsesMovementDirections_AndPhasesAreOwnPlayerOnly_AndOldSessionOrDisabledCallbacksCannotReappear()
        {
            using (var f = new CombatFixture())
            {
                Invoke(f.Combat, "ShowSkillTooltip", 1);
                var tooltip = (TMP_Text)Get(f.Combat, "skillTooltipText");
                Assert.That(tooltip.text, Does.Contain("X · Ring").And.Contain("Circle · Radius 3.25 · MP 17"),
                    "The tooltip reads the serialized C14 skill asset, including edited mana and range values.");
                Assert.That(((GameObject)Get(f.Combat, "skillTooltip")).activeSelf, Is.True);
                Invoke(f.Combat, "HideSkillTooltip");
                Assert.That(((GameObject)Get(f.Combat, "skillTooltip")).activeSelf, Is.False);

                for (byte dir = 0; dir < 4; dir++)
                {
                    f.Full(dir);
                    Assert.That(f.Aim.text, Is.EqualTo("Lv 0  EXP 0"), "Facing changes do not add internal aiming details to the growth HUD.");
                }
                f.World.transform.position = new Vector3(60f, 0f, 70f);
                f.World.transform.localScale = new Vector3(2f, 1f, 2f);
                var action = new ActionResultField { casterType = 0, casterId = 73, skillSlot = 2, skillId = 2, dir = 3, x = 10, y = 10, skillPhase = 1 };
                var current = (Action<ushort, ActionResultField[]>)Get(f.Combat, "actionResults");
                current(1, new[] { action });
                Assert.That(f.Status.text, Is.EqualTo("Line released"));
                action.skillPhase = 0;
                action.dir = 0;
                current(1, new[] { action });
                Assert.That(f.Status.text, Is.EqualTo("Line charging"));
                var telegraph = (MeshRenderer)Get(f.Combat, "chargeTelegraphRenderer");
                var telegraphMesh = (Mesh)Get(f.Combat, "chargeTelegraphMesh");
                Assert.That(telegraph.enabled && telegraphMesh.vertexCount >= 8 && telegraphMesh.triangles.Length >= 12, Is.True,
                    "The accepted Line charging phase displays a translucent filled area and progress shape.");
                Assert.That((bool)Get(f.Combat, "chargeTelegraphActive"), Is.True);
                Assert.That(telegraph.transform.parent, Is.Null);
                var telegraphVertices = (Vector3[])Get(f.Combat, "chargeVertices");
                Assert.That(telegraphVertices[0], Is.EqualTo(new Vector3(8.5f, 0.055f, 10f)));
                Assert.That(telegraphVertices[1], Is.EqualTo(new Vector3(11.5f, 0.055f, 10f)));
                Assert.That(telegraphVertices[2], Is.EqualTo(new Vector3(11.5f, 0.055f, 22f)));
                Assert.That(telegraphVertices[3], Is.EqualTo(new Vector3(8.5f, 0.055f, 22f)));
                float confirmedCooldown = (float)((float[])Get(f.Combat, "cooldownDeadlines"))[2];
                Assert.That(confirmedCooldown - Time.unscaledTime, Is.EqualTo(f.serverContent.otherSkills[1].cooldownTicks * 0.05f).Within(0.1f),
                    "The estimate starts from the server-confirmed cast and reads the existing skill data ticks.");
                var cooldownOverlay = f.Buttons[2].transform.Find("SkillIcon/C05CooldownOverlay");
                Assert.That(cooldownOverlay.gameObject.activeSelf, Is.True);
                Assert.That(cooldownOverlay.GetComponent<Image>().sprite, Is.Not.Null, "The radial Image uses the fixture skill icon sprite.");
                Assert.That(cooldownOverlay.GetComponent<Image>().type, Is.EqualTo(Image.Type.Filled));
                Assert.That(cooldownOverlay.GetComponent<Image>().fillMethod, Is.EqualTo(Image.FillMethod.Radial360));
                Assert.That(cooldownOverlay.GetComponent<Image>().fillAmount, Is.GreaterThan(0.98f));
                Assert.That(cooldownOverlay.GetComponentInChildren<TMP_Text>().text, Does.StartWith("~"));
                Assert.That(cooldownOverlay.Find("Time").GetComponent<TMP_Text>().font,
                    Is.SameAs(f.Buttons[2].transform.Find("SkillName").GetComponent<TMP_Text>().font));
                Assert.That(f.Buttons[2].transform.Find("SkillName").GetComponent<TMP_Text>().text, Is.EqualTo("LINE"));
                Assert.That(f.Buttons[2].transform.Find("SkillKey").GetComponent<TMP_Text>().text, Is.EqualTo("C"));
                Set(f.Combat, "chargeTelegraphDeadline", Time.unscaledTime - 0.001f);
                Invoke(f.Combat, "Update");
                Assert.That(telegraph.enabled, Is.False, "The authored charge wait clears the telegraph if no release arrives.");
                current(1, new[] { action });
                Assert.That(telegraph.enabled && (bool)Get(f.Combat, "chargeTelegraphActive"), Is.True);
                var otherCaster = action; otherCaster.casterType = 1;
                current(1, new[] { otherCaster });
                Assert.That(telegraph.enabled, Is.True, "Monster actions do not replace the player's charge area.");
                action.skillPhase = 1;
                current(1, new[] { action });
                var releaseEffect = (MeshRenderer)Get(f.Combat, "releaseEffectRenderer");
                var releaseMesh = (Mesh)Get(f.Combat, "releaseEffectMesh");
                Assert.That(releaseEffect, Is.Not.Null);
                Assert.That(releaseMesh, Is.Not.Null);
                Assert.That((bool)Get(f.Combat, "chargeTelegraphActive"), Is.False,
                    "The caster's own C release hides its charge area before drawing the hit effect.");
                Assert.That(releaseEffect.enabled && releaseMesh.vertexCount > 0 && releaseMesh.triangles.Length > 0, Is.True,
                    "The C release uses a visible mesh ribbon rather than the charging fill.");
                Assert.That(releaseEffect.transform.parent, Is.Null, "The world-space effect must not inherit the transformed world root.");
                Assert.That(releaseEffect.transform.position, Is.EqualTo(Vector3.zero));
                Assert.That((float)Get(f.Combat, "releaseEffectDeadline") - Time.unscaledTime,
                    Is.EqualTo(0.35f).Within(0.1f), "The hit effect uses its short visual lifetime.");
                Assert.That(((float[])Get(f.Combat, "cooldownDeadlines"))[2], Is.EqualTo(confirmedCooldown),
                    "The release phase does not restart the cooldown estimate.");
                action.skillSlot = 2; action.skillId = 2; action.skillPhase = 0;
                current(1, new[] { action });
                Assert.That((bool)Get(f.Combat, "chargeTelegraphActive"), Is.True);
                Assert.That(telegraph.enabled, Is.True);
                action.skillSlot = 1; action.skillId = 1; action.skillPhase = 1;
                current(1, new[] { action });
                Assert.That(f.Status.text, Is.EqualTo("Ring released"));
                Assert.That((bool)Get(f.Combat, "chargeTelegraphActive"), Is.True,
                    "Releasing Ring must preserve the unrelated Line charge area.");
                Assert.That(telegraph.enabled, Is.True,
                    "The Line charge renderer remains visible while Ring uses its independent release renderer.");
                Assert.That(releaseEffect.enabled && releaseMesh.vertexCount > 0, Is.True,
                    "Ring release uses a broad sweeping mesh ribbon.");
                var baseVertices = (Vector3[])Get(f.Combat, "releaseBaseVertices");
                var animatedVertices = (Vector3[])Get(f.Combat, "releaseVertices");
                var origin = (Vector3)Get(f.Combat, "releaseEffectOrigin");
                Vector3 baseSweepOffset = baseVertices[1] - origin;
                Assert.That(new Vector2(baseSweepOffset.x, baseSweepOffset.z).magnitude, Is.EqualTo(.96f * 3.25f).Within(0.01f),
                    "The authored sweep layer stays just inside the server-provided planar ring boundary.");
                Vector3 initialSweepPoint = animatedVertices[1];
                Set(f.Combat, "releaseEffectDeadline", Time.unscaledTime + 0.175f);
                Invoke(f.Combat, "Update");
                Vector3 animatedSweepOffset = animatedVertices[1] - origin;
                Assert.That(new Vector2(animatedSweepOffset.x, animatedSweepOffset.z).magnitude, Is.EqualTo(.63f * .96f * 3.25f).Within(0.04f),
                    "The mesh sweep scales with its remaining lifetime.");
                Assert.That(animatedVertices[1], Is.Not.EqualTo(initialSweepPoint));
                Set(f.Combat, "releaseEffectDeadline", Time.unscaledTime - 0.001f);
                Invoke(f.Combat, "Update");
                Assert.That(releaseEffect.enabled, Is.False, "The mesh renderer is disabled when its deadline expires.");
                action.skillSlot = 2; action.skillId = 2; action.skillPhase = 0;
                current(1, new[] { action });
                Assert.That((bool)Get(f.Combat, "chargeTelegraphActive"), Is.True,
                    "A new Line charge rebuilds its rectangle after the Ring release effect.");
                ((float[])Get(f.Combat, "cooldownDeadlines"))[2] = Time.unscaledTime - 1f;
                Invoke(f.Combat, "Update");
                Assert.That(cooldownOverlay.gameObject.activeSelf, Is.False, "The radial overlay hides when its estimate expires.");
                Assert.That(f.Buttons[2].transform.Find("SkillName").GetComponent<TMP_Text>().text, Is.EqualTo("LINE"));
                Assert.That(f.Buttons[2].transform.Find("SkillKey").GetComponent<TMP_Text>().text, Is.EqualTo("C"));
                string confirmed = f.Status.text;
                var bad = action; bad.casterType = 1; current(1, new[] { bad });
                bad = action; bad.casterId = 99; current(1, new[] { bad });
                bad = action; bad.skillPhase = 2; current(1, new[] { bad });
                bad = action; bad.x = float.NaN; current(1, new[] { bad });
                current(2, new[] { action });
                Assert.That(f.Status.text, Is.EqualTo(confirmed));
                Invoke(f.Combat, "ChangeClient", f.Model);
                current(1, new[] { action });
                Assert.That(f.Status.text, Is.Empty, "Old session callbacks cannot repaint the new observation.");
                var active = (Action<ushort, ActionResultField[]>)Get(f.Combat, "actionResults");
                action.skillSlot = 2; action.skillId = 2; action.skillPhase = 0;
                active(1, new[] { action });
                Assert.That((bool)Get(f.Combat, "chargeTelegraphActive"), Is.True);
                Assert.That(telegraph.enabled, Is.True);
                f.Delta(new DeltaUpdateField { zoneInternalID = 73, fieldID = 0, fieldVal = 0 });
                Assert.That(telegraph.enabled, Is.False, "An authoritative HP 0 update removes the charge area.");
                f.Full();
                active(1, new[] { action });
                Assert.That(telegraph.enabled, Is.True);
                Invoke(f.Combat, "ShowSkillTooltip", 1);
                Assert.That(((GameObject)Get(f.Combat, "skillTooltip")).activeSelf, Is.True);
                float beforeTransition = ((float[])Get(f.Combat, "cooldownDeadlines"))[2];
                Set(f.World, "transitionTargetZone", (ushort)2);
                typeof(WorldScreen).GetProperty("Stage").GetSetMethod(true).Invoke(f.World, new object[] { WorldStage.TransitionPending });
                Invoke(f.World, "ZoneReceived", (ushort)2, 0UL, 74U, 210f, 10f);
                Assert.That(telegraph.enabled, Is.False, "Changing zone/own character clears the previous charge area.");
                Assert.That(((GameObject)Get(f.Combat, "skillTooltip")).activeSelf, Is.False,
                    "Changing zone hides the transient skill help.");
                Invoke(f.World, "FullReceived", (ushort)1, new[]
                {
                    new FullStateField { zoneInternalID = 74, hp = 100, mp = 50, maxHp = 100, maxMp = 50, x = 210, y = 10, dir = 3 }
                });
                Assert.That(f.World.Stage, Is.EqualTo(WorldStage.InWorld), "The fixture must actually finish a valid zone transfer.");
                action.casterId = 74; action.skillPhase = 0; action.x = 210;
                active = (Action<ushort, ActionResultField[]>)Get(f.Combat, "actionResults");
                Assert.That(((float[])Get(f.Combat, "cooldownDeadlines"))[2], Is.EqualTo(beforeTransition),
                    "Zone transfer preserves the current character's estimate without a new cast.");
                Assert.That(cooldownOverlay.gameObject.activeSelf, Is.True, "A new zone shows the same character's remaining estimate again.");
                active(1, new[] { action });
                action.skillPhase = 1;
                active(1, new[] { action });
                var currentRelease = (MeshRenderer)Get(f.Combat, "releaseEffectRenderer");
                Assert.That(currentRelease.enabled, Is.True);
                f.Data.UpdateAction(3, 10, 10, 1);
                f.DisableCombat();
                active(1, new[] { action });
                Assert.That(f.Status.text, Is.Empty);
                Assert.That(telegraph.enabled, Is.False);
                Assert.That(currentRelease.enabled, Is.False, "Disabling combat clears the release mesh.");
                Assert.That(cooldownOverlay.gameObject.activeSelf, Is.False, "Disabling combat hides its cooldown overlay.");
                Assert.That(((IViewDataModel)f.Data).GetActionState().Item1, Is.False);
                int iconChildren = f.Buttons[2].transform.Find("SkillIcon").childCount;
                f.EnableCombat();
                Assert.That(f.Buttons[2].transform.Find("SkillIcon").childCount, Is.EqualTo(iconChildren),
                    "Re-enabling reuses the existing overlay instead of adding another UI object.");
                Assert.That(cooldownOverlay.gameObject.activeSelf, Is.False);
                f.DisableCombat();
            }
        }

        [Test]
        public void ChargeTelegraphsFillAllSkillShapesShowProgressAndReleaseAtEachServerPhasePosition()
        {
            using (var f = new CombatFixture())
            {
                f.World.transform.position = new Vector3(60f, 0f, 70f);
                f.World.transform.localScale = new Vector3(2f, 1f, 2f);
                var received = (Action<ushort, ActionResultField[]>)Get(f.Combat, "actionResults");
                var expectedForward = new[] { Vector3.forward, Vector3.back, Vector3.left, Vector3.right };

                for (byte slot = 0; slot < 3; slot++)
                {
                    var charge = new ActionResultField { casterType = 0, casterId = 73, skillSlot = slot,
                        skillId = slot, dir = 0, x = 20, y = 30, skillPhase = 0 };
                    received(1, new[] { charge });
                    var renderer = (MeshRenderer)Get(f.Combat, "chargeTelegraphRenderer");
                    var mesh = (Mesh)Get(f.Combat, "chargeTelegraphMesh");
                    var vertices = (Vector3[])Get(f.Combat, "chargeVertices");
                    Assert.That((bool)Get(f.Combat, "chargeTelegraphActive"), Is.True);
                    Assert.That(renderer.enabled && mesh.triangles.Length > 0, Is.True);
                    Assert.That(renderer.transform.parent, Is.Null);
                    Assert.That((Vector3)Get(f.Combat, "chargeTelegraphOrigin"), Is.EqualTo(new Vector3(20f, .07f, 30f)));
                    Assert.That(((Color[])Get(f.Combat, "chargeColors"))[0].a, Is.LessThan(.4f),
                        "Charging fills are translucent and remain quieter than the release effect.");

                    float duration = (float)Get(f.Combat, "chargeTelegraphDuration");
                    Set(f.Combat, "chargeTelegraphStarted", Time.unscaledTime - duration * .5f);
                    Invoke(f.Combat, "Update");
                    if (slot < 2)
                    {
                        float expectedRadius = slot == 0 ? 2f : 3.25f;
                        Vector3 edge = vertices[1] - new Vector3(20f, .07f, 30f);
                        Assert.That(new Vector2(edge.x, edge.z).magnitude, Is.EqualTo(expectedRadius).Within(.01f),
                            "Circle fill dimensions come from the server skill hit phase.");
                        Vector3 progressEdge = vertices[43] - new Vector3(20f, .07f, 30f);
                        Assert.That(new Vector2(progressEdge.x, progressEdge.z).magnitude, Is.EqualTo(expectedRadius * .5f).Within(.03f),
                            "The brighter inner fill visibly advances during the authored wait.");
                    }
                    else
                    {
                        Assert.That(vertices[0], Is.EqualTo(new Vector3(18.5f, .055f, 30f)));
                        Assert.That(vertices[1], Is.EqualTo(new Vector3(21.5f, .055f, 30f)));
                        Assert.That(vertices[2], Is.EqualTo(new Vector3(21.5f, .055f, 42f)));
                        Vector3 progressEnd = (vertices[6] + vertices[7]) * .5f;
                        Assert.That(Vector3.Distance(new Vector3(progressEnd.x, 0f, progressEnd.z),
                            new Vector3(20f, 0f, 30f)), Is.EqualTo(6f).Within(.03f),
                            "The brighter rectangle fill advances toward the skill's far edge.");
                    }
                    Set(f.Combat, "chargeTelegraphDeadline", Time.unscaledTime - .001f);
                    Invoke(f.Combat, "Update");
                    Assert.That(renderer.enabled, Is.False, "Each telegraph clears at its server-authored wait deadline.");
                }

                for (byte dir = 0; dir < 4; dir++)
                {
                    var phase = new ActionResultField { casterType = 0, casterId = 73, skillSlot = 2,
                        skillId = 2, dir = dir, x = 20, y = 30, skillPhase = 0 };
                    received(1, new[] { phase });
                    var chargeRenderer = (MeshRenderer)Get(f.Combat, "chargeTelegraphRenderer");
                    Assert.That(chargeRenderer.enabled && (bool)Get(f.Combat, "chargeTelegraphActive"), Is.True);
                    var chargeVertices = (Vector3[])Get(f.Combat, "chargeVertices");
                    Vector3 chargeFarCenter = (chargeVertices[2] + chargeVertices[3]) * .5f;
                    Vector3 chargeForward = Vector3.ProjectOnPlane(chargeFarCenter - new Vector3(20f, .055f, 30f), Vector3.up).normalized;
                    Assert.That(Vector3.Dot(chargeForward, expectedForward[dir]), Is.GreaterThan(.999f),
                        "The filled rectangle follows all four server directions.");
                    phase.skillPhase = 1; phase.x = 22; phase.y = 33;
                    received(1, new[] { phase });
                    Assert.That((bool)Get(f.Combat, "chargeTelegraphActive"), Is.False);
                    Assert.That(chargeRenderer.enabled, Is.False);
                    var effect = (MeshRenderer)Get(f.Combat, "releaseEffectRenderer");
                    Assert.That(effect.enabled, Is.True);
                    Assert.That(effect.transform.parent, Is.Null);
                    Assert.That(effect.transform.position, Is.EqualTo(Vector3.zero));
                    var mesh = (Mesh)Get(f.Combat, "releaseEffectMesh");
                    var vertices = (Vector3[])Get(f.Combat, "releaseBaseVertices");
                    var origin = new Vector3(22f, .07f, 33f);
                    Assert.That((Vector3)Get(f.Combat, "releaseEffectOrigin"), Is.EqualTo(origin),
                        "The release mesh starts at this phase's server position, not the cast position.");
                    Vector3 endCenter = (vertices[48] + vertices[49]) * .5f;
                    Vector3 forward = Vector3.ProjectOnPlane(endCenter - origin, Vector3.up).normalized;
                    Assert.That(Vector3.Dot(forward, expectedForward[dir]), Is.GreaterThan(.999f),
                        "The mesh ribbon follows the authoritative phase direction in all four wire directions.");
                    Assert.That(Vector3.Distance(endCenter, origin), Is.EqualTo(12f).Within(.01f),
                        "The base mesh keeps the server rectangle's authored range.");
                    var animated = (Vector3[])Get(f.Combat, "releaseVertices");
                    Vector3 animatedEnd = (animated[48] + animated[49]) * .5f;
                    Assert.That(Vector3.Dot(Vector3.ProjectOnPlane(animatedEnd - origin, Vector3.up).normalized,
                        expectedForward[dir]), Is.GreaterThan(.999f),
                        "The animated impact keeps its server direction.");
                    Assert.That(Vector3.Distance(animatedEnd, origin), Is.EqualTo(2.16f).Within(.06f),
                        "The mesh animation begins from the server phase origin.");
                    Set(f.Combat, "releaseEffectDeadline", Time.unscaledTime + 0.175f);
                    Invoke(f.Combat, "Update");
                    animatedEnd = (animated[48] + animated[49]) * .5f;
                    Assert.That(Vector3.Dot(Vector3.ProjectOnPlane(animatedEnd - origin, Vector3.up).normalized,
                        expectedForward[dir]), Is.GreaterThan(.999f));
                    Assert.That(Vector3.Distance(animatedEnd, origin), Is.EqualTo(7.56f).Within(.08f),
                        "The same server-anchored ribbon advances outward during its visible lifetime.");
                    Assert.That(mesh.vertexCount, Is.GreaterThan(48));
                }
            }
        }

        [Test]
        public void FirstHuntFeedback_UnconfirmedRequestExpires_AndAdvisoriesDoNotClaimRejection()
        {
            using (var f = new CombatFixture())
            {
                f.Delta(new DeltaUpdateField { zoneInternalID = 73, fieldID = 1, fieldVal = 5 });
                Invoke(f.Combat, "ShowSkillRequest", (byte)1);
                Assert.That(f.Status.gameObject.activeSelf, Is.True, "The existing HUD feedback label is visible.");
                Assert.That(f.Status.text, Does.Contain("Ring requesting").And.Contain("low MP (5/17)"));
                Assert.That(f.World.CanAct && f.Buttons[1].interactable, Is.True,
                    "The advisory leaves server authority and input availability intact.");
                float deadline = (float)Get(f.Combat, "combatStatusDeadline");
                Invoke(f.Combat, "UpdateCombatStatus", deadline - .01f);
                Assert.That(f.Status.text, Does.Contain("requesting"));
                Invoke(f.Combat, "UpdateCombatStatus", deadline);
                Assert.That(f.Status.text, Is.EqualTo("Ring: no confirmation received"));
                Assert.That((int)Get(f.Combat, "pendingSkillSlot"), Is.EqualTo(-1));
                Invoke(f.Combat, "UpdateCombatStatus", deadline + 3);
                Assert.That(f.Status.text, Is.Empty);

                f.Full();
                ((float[])Get(f.Combat, "cooldownDeadlines"))[1] = Time.unscaledTime + 2;
                Invoke(f.Combat, "ShowSkillRequest", (byte)1);
                Assert.That(f.Status.text, Does.Contain("requesting").And.Contain("cooldown ~"));
                Assert.That(f.Buttons[1].interactable, Is.True, "Estimated cooldown never disables the skill.");
                f.DisableCombat();
                Assert.That(f.Status.text, Is.Empty);
                Assert.That((int)Get(f.Combat, "pendingSkillSlot"), Is.EqualTo(-1));
                Assert.That((float)Get(f.Combat, "combatStatusDeadline"), Is.Zero);
            }
        }

        [Test]
        public void FirstHuntFeedback_OnlyMatchingOwnPhasesReplaceRequest_AndReleaseExpires()
        {
            using (var f = new CombatFixture())
            {
                Invoke(f.Combat, "ShowSkillRequest", (byte)1);
                var received = (Action<ushort, ActionResultField[]>)Get(f.Combat, "actionResults");
                var phase = new ActionResultField { casterType = 0, casterId = 99, skillSlot = 1,
                    skillId = 1, dir = 3, x = 10, y = 10, skillPhase = 0 };
                received(1, new[] { phase });
                Assert.That(f.Status.text, Is.EqualTo("Ring requesting"));
                phase.casterId = 73; phase.skillSlot = 0; phase.skillId = 0;
                received(1, new[] { phase });
                Assert.That(f.Status.text, Is.EqualTo("Ring requesting"), "Another slot cannot confirm this display's request.");
                phase.skillSlot = 1; phase.skillId = 1;
                received(1, new[] { phase });
                Assert.That(f.Status.text, Is.EqualTo("Ring charging"));
                Assert.That((int)Get(f.Combat, "pendingSkillSlot"), Is.EqualTo(-1));
                phase.skillPhase = 1;
                received(1, new[] { phase });
                Assert.That(f.Status.text, Is.EqualTo("Ring released"));
                Assert.That(f.World.OwnFull.exp, Is.Zero, "A skill phase cannot invent a hit, kill, or reward.");
                Invoke(f.Combat, "UpdateCombatStatus", (float)Get(f.Combat, "combatStatusDeadline"));
                Assert.That(f.Status.text, Is.Empty);
            }
        }

        private sealed class CombatFixture : IDisposable
        {
            private readonly GameObject root;
            private readonly Sprite sprite;
            public readonly ServerContent serverContent;
            public readonly WorldScreen World;
            public readonly CombatScreen Combat;
            public readonly IViewDataUI Data;
            public readonly ViewModel Model;
            public readonly TMP_Text Vitals, Status, Aim;
            public readonly UnityEngine.UI.Button[] Buttons;
            public CombatFixture()
            {
                root = new GameObject("C05 detached fixture"); root.SetActive(false);
                root.AddComponent<Canvas>(); root.AddComponent<UnityEngine.UI.GraphicRaycaster>();
                var login = root.AddComponent<LoginScreen>(); World = root.AddComponent<WorldScreen>(); Combat = root.AddComponent<CombatScreen>();
                Set(login, "loginIdInput", Child("id").AddComponent<TMP_InputField>());
                Set(login, "passwordInput", Child("password").AddComponent<TMP_InputField>());
                Set(login, "loginButton", Button("login")); Set(login, "statusText", Text("login status")); Set(login, "characterListText", Text("list"));
                Set(World, "loginScreen", login); Set(World, "characterChoices", Array.Empty<UnityEngine.UI.Button>());
                Set(World, "enterButton", Button("enter")); Set(World, "disconnectButton", Button("disconnect"));
                Set(World, "entryStatusText", Text("entry")); Set(World, "worldStatusText", Text("world status"));
                foreach (var name in new[] { "selectionPanel", "background", "worldRoot", "worldHud" }) Set(World, name, Child(name));
                foreach (var name in new[] { "ownCharacter", "fieldPlane", "quarterViewCamera" }) Set(World, name, Child(name).transform);
                Set(Combat, "loginScreen", login); Set(Combat, "worldScreen", World);
                serverContent = ScriptableObject.CreateInstance<ServerContent>();
                serverContent.ring.mana = 17;
                serverContent.ring.phases[1].range1 = 3.25f;
                Set(Combat, "serverContent", serverContent);
#if UNITY_EDITOR
                var effectMaterial = AssetDatabase.LoadAssetAtPath<Material>("Assets/Client/Art/KnightRefinement/SteelSkillGlow.mat");
                if (effectMaterial != null) Set(Combat, "skillEffectMaterial", effectMaterial);
#endif
                Set(Combat, "combatPanel", Child("combat panel"));
                Buttons = new[] { Button("strike"), Button("ring"), Button("line") }; Set(Combat, "skillButtons", Buttons);
                sprite = Sprite.Create(Texture2D.whiteTexture, new Rect(0, 0, 1, 1), new Vector2(0.5f, 0.5f));
                for (int i = 0; i < Buttons.Length; i++)
                {
                    var icon = new GameObject("SkillIcon", typeof(RectTransform), typeof(CanvasRenderer), typeof(Image));
                    icon.transform.SetParent(Buttons[i].transform, false);
                    icon.GetComponent<Image>().sprite = sprite;
                    var label = Text("SkillName"); label.text = new[] { "STRIKE", "RING", "LINE" }[i];
                    label.transform.SetParent(Buttons[i].transform, false);
                    var key = Text("SkillKey"); key.text = new[] { "Z", "X", "C" }[i];
                    key.transform.SetParent(Buttons[i].transform, false);
                }
                Vitals = Text("vitals"); Status = Text("phase"); Aim = Text("aim");
                Set(Combat, "vitalsText", Vitals); Set(Combat, "combatStatusText", Status); Set(Combat, "aimText", Aim);
                Data = NewData(); Model = NewModel(Data, new QueuedDispatcher());
                var initializer = new Initializer(); Set(initializer, "_viewModel", Model); Set(initializer, "_data", Data); Set(login, "client", initializer);
                root.SetActive(true);
                // Edit Mode direct fixtures do not receive MonoBehaviour lifecycle callbacks.
                if (Get(World, "model") == null) Invoke(World, "OnEnable");
                if (Get(Combat, "model") == null) Invoke(Combat, "OnEnable");
                typeof(WorldScreen).GetProperty("Stage").GetSetMethod(true).Invoke(World, new object[] { WorldStage.Entering });
                Invoke(World, "ZoneReceived", (ushort)1, 0UL, 73U, 10f, 10f); Full();
            }
            public void Full(byte dir = 3) => Invoke(World, "FullReceived", (ushort)1, new[] { new FullStateField { zoneInternalID = 73, hp = 100, mp = 50, maxHp = 100, maxMp = 50, x = 10, y = 10, dir = dir } });
            public void Delta(params DeltaUpdateField[] updates) => Invoke(World, "DeltaReceived", (ushort)updates.Length, updates);
            private GameObject Child(string name) { var child = new GameObject(name, typeof(RectTransform)); child.transform.SetParent(root.transform); return child; }
            private TMP_Text Text(string name) => Child(name).AddComponent<TextMeshProUGUI>();
            private UnityEngine.UI.Button Button(string name) => Child(name).AddComponent<UnityEngine.UI.Button>();
            public void DisableCombat()
            {
                Combat.enabled = false;
                if (Get(Combat, "model") != null) Invoke(Combat, "OnDisable");
            }
            public void EnableCombat()
            {
                Combat.enabled = true;
                if (Get(Combat, "model") == null) Invoke(Combat, "OnEnable");
            }
            public void Dispose()
            {
                DisableCombat();
                Invoke(Combat, "OnDestroy"); // Detached Edit Mode fixtures do not receive destruction callbacks.
                if (Get(World, "model") != null) Invoke(World, "OnDisable");
                UnityEngine.Object.DestroyImmediate(root);
                UnityEngine.Object.DestroyImmediate(serverContent);
                UnityEngine.Object.DestroyImmediate(sprite);
            }
        }
        private static IViewDataUI NewData() => (IViewDataUI)Activator.CreateInstance(typeof(ViewModel).Assembly.GetType("ClientCore.ViewData", true), true);
        private static ViewModel NewModel(IViewDataUI data, IMainThreadDispatcher dispatcher)
        {
            var model = (ViewModel)Activator.CreateInstance(typeof(ViewModel), BindingFlags.Instance | BindingFlags.NonPublic, null,
                new object[] { new FakeNetwork(), (IViewDataModel)data, new NullLogger(), dispatcher }, null);
            WorldTestSession.Ready(model, ((QueuedDispatcher)dispatcher).Drain);
            return model;
        }
        private static object Get(object target, string name) => target.GetType().GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).GetValue(target);
        private static void Set(object target, string name, object value) => target.GetType().GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).SetValue(target, value);
        private static void Invoke(object target, string name, params object[] args) => target.GetType().GetMethod(name, BindingFlags.Instance | BindingFlags.NonPublic).Invoke(target, args);
        private sealed class QueuedDispatcher : IMainThreadDispatcher { public readonly List<Action> Jobs = new List<Action>(); public void Post(Action job) => Jobs.Add(job); public void Drain() { foreach (var job in Jobs.ToArray()) job(); Jobs.Clear(); } }
        private sealed class NullLogger : ClientCore.ILogger { public void Log(string message) { } }
        private sealed class FakeNetwork : INetworkService
        {
            public void RequestBazaarBuy(uint requestID, ulong listingID) { }
            public void RequestBazaarCheckOutbox(uint requestID) { } public void RequestBazaarCancelListing(uint requestID, ulong listingID) { }
            public void RequestBazaarClaim(uint requestID, ulong listingID) { }
            public void RequestBazaarMyList(uint requestID) { }
            public void RequestBazaar(uint requestID, ushort itemType, ushort page) { } public void RequestInventory() { } public void Profiles(uint[] profileIds) { }
            public void RequestBazaarRegister(uint requestID, byte inventorySlot, ushort quantity, ulong totalDiamondPrice) { }
            public void RequestWallet(uint requestID) { }
            public Task Connect(string address, int port) => Task.CompletedTask;
            public void CharacterList() { } public void Enter(ulong id) { } public void Chat(string msg, byte scope, ulong target) { }
            public void Action(byte dir, float x, float y, byte slot, ulong ownershipEpoch) { } public void Pong(ulong time) { } public void ZoneChange(byte op) { } public void ZoneResync(ulong ownershipEpoch) { } public void RespawnToTown(ulong ownershipEpoch) { } public void Log(string msg) { }
        }
    }
}
