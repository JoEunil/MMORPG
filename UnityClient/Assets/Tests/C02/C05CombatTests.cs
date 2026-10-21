using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using C01;
using ClientCore;
using ClientCore.PacketHelper;
using ClientCore.Services;
using NUnit.Framework;
using TMPro;
using UnityEngine;

namespace C02.Tests
{
    // Detached fixtures: not actual keyboard/button input, a server cast, or a live death.
    public sealed class C05CombatTests
    {
        public static string RunPureChecks()
        {
            var tests = new C05CombatTests();
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
            var state = ((IViewDataModel)data).GetActionState();
            Assert.That(state, Is.EqualTo((true, (byte)3, 15f, 16f, (byte)2, 0UL)));
            state = ((IViewDataModel)data).GetActionState();
            Assert.That(state.Item1, Is.False);
            Assert.That(state.Item5, Is.EqualTo(255));
            data.UpdateMove(0, 15, 17);
            Assert.That(((IViewDataModel)data).GetActionState().Item5, Is.EqualTo(255));
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
                    Does.Contain("Your character has fallen.").And.Contain("return to the login screen."));

                f.Full();
                Assert.That(f.World.OwnFull.hp, Is.EqualTo(100));
                Assert.That(f.World.CanAct, Is.True, "A later actual Full owns the confirmed state; there is no local resurrection.");
                Assert.That(defeatPanel.activeSelf, Is.False, "A server-confirmed living state hides the defeat panel.");

                f.Delta(new DeltaUpdateField { zoneInternalID = 73, fieldID = 0, fieldVal = 0 });
                Assert.That(f.World.OwnFull.hp, Is.Zero);
                Assert.That(defeatPanel.activeSelf, Is.True);
                var logout = defeatPanel.GetComponentInChildren<UnityEngine.UI.Button>(true);
                Assert.That(logout.GetComponentInChildren<TMP_Text>(true).text, Is.EqualTo("LOG OUT"));
                logout.onClick.Invoke();
                Assert.That(((TMP_Text)Get(Get(f.Combat, "loginScreen"), "statusText")).text, Does.Contain("Session closed."));
                Assert.That(f.World.Stage, Is.EqualTo(C02Stage.Ready));
                Assert.That(Get(f.Combat, "model"), Is.Null, "ConnectionFailed releases the bound model through ClientChanged(null).");
                Assert.That(defeatPanel.activeSelf, Is.False, "Leaving through the existing connection-close path hides the defeat panel.");
            }
        }

        [Test]
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

                var directions = new[] { "North", "South", "West", "East" };
                for (byte dir = 0; dir < directions.Length; dir++)
                {
                    f.Full(dir);
                    Assert.That(f.Aim.text, Does.EndWith("Line aim: " + directions[dir]));
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
                Assert.That(f.Status.text, Is.EqualTo("Line cast"));
                var preview = (LineRenderer)Get(f.Combat, "linePreview");
                Assert.That(preview != null && preview.enabled && preview.useWorldSpace && preview.loop, Is.True);
                Assert.That(preview.transform.parent, Is.SameAs(f.World.transform));
                Assert.That(preview.positionCount, Is.EqualTo(4));
                Assert.That(preview.GetPosition(0), Is.EqualTo(new Vector3(8.5f, 0.05f, 10f)));
                Assert.That(preview.GetPosition(1), Is.EqualTo(new Vector3(11.5f, 0.05f, 10f)));
                Assert.That(preview.GetPosition(2), Is.EqualTo(new Vector3(11.5f, 0.05f, 22f)));
                Assert.That(preview.GetPosition(3), Is.EqualTo(new Vector3(8.5f, 0.05f, 22f)));
                Set(f.Combat, "linePreviewDeadline", Time.unscaledTime - 1f);
                Invoke(f.Combat, "Update");
                Assert.That(preview.enabled, Is.False, "The unscaled two-second lifetime clears a lost release.");
                current(1, new[] { action });
                Assert.That(preview.enabled, Is.True);
                var otherCaster = action; otherCaster.casterType = 1;
                current(1, new[] { otherCaster });
                Assert.That(preview.enabled, Is.True, "Monster actions do not replace the player's range preview.");
                action.skillPhase = 1;
                current(1, new[] { action });
                Assert.That(preview.enabled, Is.False, "The server's matching release hides the outline.");
                action.skillSlot = 1; action.skillId = 1; action.skillPhase = 1;
                current(1, new[] { action });
                Assert.That(f.Status.text, Is.EqualTo("Ring released"));
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
                preview = (LineRenderer)Get(f.Combat, "linePreview");
                Assert.That(preview.enabled, Is.True);
                f.Delta(new DeltaUpdateField { zoneInternalID = 73, fieldID = 0, fieldVal = 0 });
                Assert.That(preview.enabled, Is.False, "An authoritative HP 0 update removes the preview.");
                f.Full();
                active(1, new[] { action });
                Assert.That(preview.enabled, Is.True);
                Invoke(f.Combat, "ShowSkillTooltip", 1);
                Assert.That(((GameObject)Get(f.Combat, "skillTooltip")).activeSelf, Is.True);
                Set(f.World, "transitionTargetZone", (ushort)2);
                typeof(C02WorldScreen).GetProperty("Stage").GetSetMethod(true).Invoke(f.World, new object[] { C02Stage.TransitionPending });
                Invoke(f.World, "ZoneReceived", (ushort)2, 0UL, 74U, 10f, 10f);
                Assert.That(preview.enabled, Is.False, "Changing zone/own character clears the previous cast outline.");
                Assert.That(((GameObject)Get(f.Combat, "skillTooltip")).activeSelf, Is.False,
                    "Changing zone hides the transient skill help.");
                f.Data.UpdateAction(3, 10, 10, 1);
                f.DisableCombat();
                active(1, new[] { action });
                Assert.That(f.Status.text, Is.Empty);
                Assert.That(preview.enabled, Is.False);
                Assert.That(((IViewDataModel)f.Data).GetActionState().Item1, Is.False);
            }
        }

        private sealed class CombatFixture : IDisposable
        {
            private readonly GameObject root;
            private readonly C14ServerContent serverContent;
            public readonly C02WorldScreen World;
            public readonly C05CombatScreen Combat;
            public readonly IViewDataUI Data;
            public readonly ViewModel Model;
            public readonly TMP_Text Vitals, Status, Aim;
            public readonly UnityEngine.UI.Button[] Buttons;
            public CombatFixture()
            {
                root = new GameObject("C05 detached fixture"); root.SetActive(false);
                root.AddComponent<Canvas>(); root.AddComponent<UnityEngine.UI.GraphicRaycaster>();
                var login = root.AddComponent<C01LoginScreen>(); World = root.AddComponent<C02WorldScreen>(); Combat = root.AddComponent<C05CombatScreen>();
                Set(login, "loginIdInput", Child("id").AddComponent<TMP_InputField>());
                Set(login, "passwordInput", Child("password").AddComponent<TMP_InputField>());
                Set(login, "loginButton", Button("login")); Set(login, "statusText", Text("login status")); Set(login, "characterListText", Text("list"));
                Set(World, "loginScreen", login); Set(World, "characterChoices", Array.Empty<UnityEngine.UI.Button>());
                Set(World, "enterButton", Button("enter")); Set(World, "disconnectButton", Button("disconnect"));
                Set(World, "entryStatusText", Text("entry")); Set(World, "worldStatusText", Text("world status"));
                foreach (var name in new[] { "selectionPanel", "background", "worldRoot", "worldHud" }) Set(World, name, Child(name));
                foreach (var name in new[] { "ownCharacter", "fieldPlane", "quarterViewCamera" }) Set(World, name, Child(name).transform);
                Set(Combat, "loginScreen", login); Set(Combat, "worldScreen", World);
                serverContent = ScriptableObject.CreateInstance<C14ServerContent>();
                serverContent.ring.mana = 17;
                serverContent.ring.phases[1].range1 = 3.25f;
                Set(Combat, "serverContent", serverContent);
                Set(Combat, "combatPanel", Child("combat panel"));
                Buttons = new[] { Button("strike"), Button("ring"), Button("line") }; Set(Combat, "skillButtons", Buttons);
                Vitals = Text("vitals"); Status = Text("phase"); Aim = Text("aim");
                Set(Combat, "vitalsText", Vitals); Set(Combat, "combatStatusText", Status); Set(Combat, "aimText", Aim);
                Data = NewData(); Model = NewModel(Data, new QueuedDispatcher());
                var initializer = new Initializer(); Set(initializer, "_viewModel", Model); Set(initializer, "_data", Data); Set(login, "client", initializer);
                root.SetActive(true);
                // Edit Mode direct fixtures do not receive MonoBehaviour lifecycle callbacks.
                if (Get(World, "model") == null) Invoke(World, "OnEnable");
                if (Get(Combat, "model") == null) Invoke(Combat, "OnEnable");
                typeof(C02WorldScreen).GetProperty("Stage").GetSetMethod(true).Invoke(World, new object[] { C02Stage.Entering });
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
            public void Dispose()
            {
                DisableCombat();
                if (Get(World, "model") != null) Invoke(World, "OnDisable");
                UnityEngine.Object.DestroyImmediate(root);
                UnityEngine.Object.DestroyImmediate(serverContent);
            }
        }
        private static IViewDataUI NewData() => (IViewDataUI)Activator.CreateInstance(typeof(ViewModel).Assembly.GetType("ClientCore.ViewData", true), true);
        private static ViewModel NewModel(IViewDataUI data, IMainThreadDispatcher dispatcher)
        {
            var model = (ViewModel)Activator.CreateInstance(typeof(ViewModel), BindingFlags.Instance | BindingFlags.NonPublic, null,
                new object[] { new FakeNetwork(), (IViewDataModel)data, new NullLogger(), dispatcher }, null);
            C08TestSession.Ready(model, ((QueuedDispatcher)dispatcher).Drain);
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
            public Task Connect(string address, int port) => Task.CompletedTask;
            public void CharacterList() { } public void Enter(ulong id) { } public void Chat(string msg, byte scope, ulong target) { }
            public void Action(byte dir, float x, float y, byte slot, ulong ownershipEpoch) { } public void Pong(ulong time) { } public void ZoneChange(byte op) { } public void ZoneResync(ulong ownershipEpoch) { } public void Log(string msg) { }
        }
    }
}
