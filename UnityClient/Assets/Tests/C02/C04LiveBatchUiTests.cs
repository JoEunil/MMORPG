#if UNITY_EDITOR
using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Reflection;
using C01;
using ClientCore;
using ClientCore.PacketHelper;
using NUnit.Framework;
using UnityEditor;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.LowLevel;
using UnityEngine.UI;

namespace C02.Tests
{
    // Starts after the shared real UI entry has left the authenticated scene live.
    public sealed class C04LiveBatchUiTests
    {
        private static double peerLastFullSeenAt;
        private static uint peerInternalId;
        private static bool peerWasObservedAtReentry;
        private Mouse mouse;
        private IViewModelUI model;
        private readonly Dictionary<uint, FullStateField> players = new Dictionary<uint, FullStateField>();
        private readonly Dictionary<ushort, MonsterFullField> monsters = new Dictionary<ushort, MonsterFullField>();
        private Action<ushort, FullStateField[]> playerFull;
        private Action<ushort, DeltaUpdateField[]> playerDelta;
        private Action<ushort, MonsterFullField[]> monsterFull;
        private Action<ushort, MonsterDeltaField[]> monsterDelta;

        public IEnumerator ObservePeerAndWorld(long expectedPeerId)
        {
            var focusedWindow = EditorWindow.focusedWindow;
            bool oldBackground = Application.runInBackground;
            C01LoginScreen login = null;
            C02WorldScreen world = null;
            C04ObservedWorld observed = null;
            try
            {
                Application.runInBackground = true;
                mouse = InputSystem.AddDevice<Mouse>();
                var gameView = FindGameView();
                Assert.That(gameView != null, Is.True, "Open the Game view before the batched native-input phase.");
                gameView.Focus();
                yield return null;
                login = GameObject.Find("C01Client")?.GetComponent<C01LoginScreen>();
                world = GameObject.Find("C02Client")?.GetComponent<C02WorldScreen>();
                observed = GameObject.Find("C02Client")?.GetComponent<C04ObservedWorld>();
                Assert.That(login != null && world != null && observed != null, Is.True,
                    "The shared C02 live-entry scene must remain open for C04 observation.");
                Assert.That(world.Stage == C02Stage.InWorld && world.HasOwnFull && world.ZoneID == 1 && login.Model != null,
                    Is.True, "C04 observation starts only after the primary client's real server Zone and own Full.");

                model = login.Model;
                playerFull = (count, states) =>
                {
                    for (int i = 0; i < count; i++) players[states[i].zoneInternalID] = states[i];
                };
                playerDelta = (count, updates) => ApplyPlayerDelta(count, updates);
                monsterFull = (count, states) =>
                {
                    for (int i = 0; i < count; i++) monsters[states[i].internalId] = states[i];
                };
                monsterDelta = (count, updates) => ApplyMonsterDelta(count, updates);
                model.OnFullReceived += playerFull;
                model.OnDeltaReceived += playerDelta;
                model.OnMonsterFullReceived += monsterFull;
                model.OnMonsterDeltaReceived += monsterDelta;

                yield return WaitFor(() => GetDictionary(observed, "players").Contains((uint)expectedPeerId) &&
                    players.ContainsKey((uint)expectedPeerId),
                    30f, "the second approved character's real player Full and visual");
                var playerTable = GetDictionary(observed, "players");
                Assert.That(playerTable.Contains(world.InternalID), Is.False, "The local character must never appear as a remote player.");
                Assert.That(players.TryGetValue((uint)expectedPeerId, out var peerState), Is.True,
                    "An independent ClientCore event observer must see the same peer ID.");
                AssertThatObservationMatches(playerTable[(uint)expectedPeerId], peerState.x, peerState.y, peerState.dir);
                peerInternalId = (uint)expectedPeerId;
                peerLastFullSeenAt = LastFullSeen(playerTable[(uint)expectedPeerId]);

                yield return WaitFor(() => GetDictionary(observed, "monsters").Count > 0 && monsters.Count > 0,
                    30f, "real server monster Full and visual");
                var monsterTable = GetDictionary(observed, "monsters");
                bool matchedMonster = false;
                foreach (DictionaryEntry pair in monsterTable)
                {
                    ushort id = (ushort)pair.Key;
                    if (!monsters.TryGetValue(id, out var state)) continue;
                    AssertThatMonsterMatches(pair.Value, state);
                    matchedMonster = true;
                    break;
                }
                Assert.That(matchedMonster, Is.True, "At least one displayed monster must match an independent server Full event.");
                File.AppendAllText(ResultPath, string.Format(CultureInfo.InvariantCulture,
                    "PASS C04 server observations; peer={0}; zone={1}; peers={2}; monsters={3}{4}",
                    expectedPeerId, world.ZoneID, playerTable.Count, monsterTable.Count, Environment.NewLine));

                yield return Capture("c04-server-observations.png");
                yield return ObserveMovement(world, observed, expectedPeerId);
            }
            finally
            {
                if (model != null)
                {
                    model.OnFullReceived -= playerFull;
                    model.OnDeltaReceived -= playerDelta;
                    model.OnMonsterFullReceived -= monsterFull;
                    model.OnMonsterDeltaReceived -= monsterDelta;
                }
                model = null;
                if (mouse != null && mouse.added) InputSystem.RemoveDevice(mouse);
                mouse = null;
                Application.runInBackground = oldBackground;
                if (focusedWindow != null) focusedWindow.Focus();
            }
        }

        private IEnumerator ObserveMovement(C02WorldScreen world, C04ObservedWorld observed, long expectedPeerId)
        {
            var ownCharacter = GameObject.Find("OwnCharacter").transform;
            var camera = GameObject.Find("QuarterViewCamera").GetComponent<Camera>();
            Assert.That(camera != null, Is.True, "The existing quarter-view camera must be active.");
            var nearTarget = world.ServerPosition + new Vector2(0.8f, 0.6f);
            yield return MoveTo(world, camera, ownCharacter, nearTarget, "near point");
            var farTarget = world.ServerPosition + new Vector2(2.5f, 1.5f);
            yield return MoveTo(world, camera, ownCharacter, farTarget, "far diagonal point");

            // Replace an active, still distant target with a visible point in the opposite direction.
            var firstTarget = world.ServerPosition + new Vector2(12f, 0);
            yield return ClickWorldTarget(camera, firstTarget);
            var pendingTarget = GetField(world, "moveTarget") as Vector2?;
            Assert.That(pendingTarget.HasValue, Is.True,
                "The first native click must still have a pending movement target before replacement.");
            Assert.That(Vector2.Distance(pendingTarget.Value, firstTarget), Is.LessThan(0.001f),
                "The pending target must be the first native click target before replacement.");
            Assert.That(Vector2.Distance(world.ServerPosition, firstTarget), Is.GreaterThan(0.2f),
                "The server must not have arrived at the first target before the replacement click.");
            var replacementStart = world.ServerPosition;
            var replacementTarget = replacementStart + new Vector2(-6f, 0);
            File.AppendAllText(ResultPath, string.Format(CultureInfo.InvariantCulture,
                "INFO replacement before click; start=({0:F3},{1:F3}); requested=({2:F3},{3:F3}){4}",
                replacementStart.x, replacementStart.y, replacementTarget.x, replacementTarget.y, Environment.NewLine));
            yield return ClickWorldTarget(camera, replacementTarget);
            var clickedMoveTarget = GetField(world, "moveTarget") as Vector2?;
            File.AppendAllText(ResultPath, string.Format(CultureInfo.InvariantCulture,
                "INFO replacement after click; moveTarget={0}; server=({1:F3},{2:F3}){3}",
                clickedMoveTarget.HasValue
                    ? string.Format(CultureInfo.InvariantCulture, "({0:F3},{1:F3})", clickedMoveTarget.Value.x, clickedMoveTarget.Value.y)
                    : "null",
                world.ServerPosition.x, world.ServerPosition.y, Environment.NewLine));
            yield return WaitFor(() => world.ServerPosition.x < replacementStart.x - 0.2f,
                15f, "server-confirmed movement toward the replacement target");
            yield return WaitFor(() => GetField(world, "moveTarget") == null, 10f,
                "server movement target completion before the HUD-blocking input check");
            var afterMove = world.ServerPosition;
            Assert.That(ownCharacter.position, Is.EqualTo(new Vector3(afterMove.x, 0, afterMove.y)),
                "The local visual must follow the latest server-confirmed movement.");
            AssertOwnFacing(world, ownCharacter);
            File.AppendAllText(ResultPath, string.Format(CultureInfo.InvariantCulture,
                "PASS C03 native near/far/diagonal/replacement clicks; final=({0:F3},{1:F3}){2}",
                afterMove.x, afterMove.y, Environment.NewLine));
            var hudObject = GetField(world, "worldHud") as GameObject;
            var hud = hudObject != null ? hudObject.transform as RectTransform : null;
            Assert.That(hud != null, Is.True, "The existing world HUD rect is required for the UI-raycast check.");
            var hudScreen = RectTransformUtility.WorldToScreenPoint(null, hud.position);
            yield return ClickAt(hudScreen);
            yield return null;
            Assert.That(world.ServerPosition == afterMove, Is.True, "Clicking the visible HUD must not create a new movement target.");
            Assert.That(GetField(world, "moveTarget") == null, Is.True, "HUD input must leave no active movement target.");
            Assert.That(GetDictionary(observed, "players").Contains((uint)expectedPeerId), Is.True,
                "Peer observation must survive the primary move within its Full lifetime.");
            yield return Capture("c04-movement-observations.png");

            var disconnect = GameObject.Find("DisconnectButton")?.GetComponent<Button>();
            Assert.That(disconnect != null && disconnect.interactable, Is.True, "The existing in-world disconnect button must be usable.");
            yield return ClickButton(disconnect);
            yield return WaitFor(() => !world.IsWorldVisible && GetDictionary(observed, "players").Count == 0 &&
                GetDictionary(observed, "monsters").Count == 0, 8f, "C04 visuals and observations to clear on disconnect");
            Assert.That(GetField(world, "moveTarget") == null, Is.True, "Disconnect must clear any remaining movement target.");
            File.AppendAllText(ResultPath, "PASS C04 disconnect cleanup; primary observations cleared.\n");

            // Reuse the reviewed native login/selection/entry path once to check that a new session binds cleanly.
            double reentryStartedAt = Time.realtimeSinceStartupAsDouble;
            yield return new C02EmptyListUiTests().RealCharacterSelectionAndEntry_AndCapture(5011UL);
            var reenteredWorld = GameObject.Find("C02Client")?.GetComponent<C02WorldScreen>();
            var reenteredObserver = GameObject.Find("C02Client")?.GetComponent<C04ObservedWorld>();
            Assert.That(reenteredWorld != null && reenteredWorld.Stage == C02Stage.InWorld &&
                reenteredWorld.HasOwnFull && reenteredObserver != null, Is.True,
                "The existing real UI path must re-enter the world after disconnect.");
            AssertFreshObservations(GetDictionary(reenteredObserver, "players"), reentryStartedAt);
            AssertFreshObservations(GetDictionary(reenteredObserver, "monsters"), reentryStartedAt);
            var reenteredPlayers = GetDictionary(reenteredObserver, "players");
            peerWasObservedAtReentry = peerInternalId != 0 && reenteredPlayers.Contains(peerInternalId);
            if (peerWasObservedAtReentry)
                peerLastFullSeenAt = LastFullSeen(reenteredPlayers[peerInternalId]);
            File.AppendAllText(ResultPath,
                "PASS C01/C02 real UI re-entry after disconnect; retained entities received current-session Full.\n" +
                "NOT OBSERVED C04 monster HP0: no combat/action was generated; fixture covers packet handling.\n" +
                "NOT TESTED C03 text-focus blocking: no active in-world text-entry field exists.\n");
            yield return Capture("c04-reentry.png");
        }

        public IEnumerator PeerTtlAndCenterMonsterTypes()
        {
            var focusedWindow = EditorWindow.focusedWindow;
            bool oldBackground = Application.runInBackground;
            C01LoginScreen login = null;
            C02WorldScreen world = null;
            C04ObservedWorld observed = null;
            try
            {
                Application.runInBackground = true;
                mouse = InputSystem.AddDevice<Mouse>();
                var gameView = FindGameView();
                Assert.That(gameView != null, Is.True, "Open the Game view before the second live observation phase.");
                gameView.Focus();
                yield return null;
                login = GameObject.Find("C01Client")?.GetComponent<C01LoginScreen>();
                world = GameObject.Find("C02Client")?.GetComponent<C02WorldScreen>();
                observed = GameObject.Find("C02Client")?.GetComponent<C04ObservedWorld>();
                Assert.That(login != null && world != null && observed != null && world.Stage == C02Stage.InWorld &&
                    login.Model != null && peerInternalId != 0, Is.True,
                    "The current primary session and previously observed peer must exist.");

                var playersTable = GetDictionary(observed, "players");
                if (peerWasObservedAtReentry && peerLastFullSeenAt > 0)
                {
                    yield return WaitFor(() => !playersTable.Contains(peerInternalId),
                        5f, "the peer's local Full-observation TTL to expire after its separate client exits");
                    Assert.That(Time.realtimeSinceStartupAsDouble - peerLastFullSeenAt >= 3.0, Is.True,
                        "The absent peer must have exceeded the local three-second Full-observation lifetime.");
                    File.AppendAllText(CenterResultPath,
                        "PASS peer observation TTL expired after normal peer-process exit; this is client display policy, not a server despawn message.\n");
                }
                else
                    File.AppendAllText(CenterResultPath,
                        "NOT TESTED peer TTL after shutdown: peer was outside the primary client's Full AOI after re-entry.\n");

                model = login.Model;
                playerFull = (count, states) => { for (int i = 0; i < count; i++) players[states[i].zoneInternalID] = states[i]; };
                playerDelta = (count, updates) => ApplyPlayerDelta(count, updates);
                monsterFull = (count, states) => { for (int i = 0; i < count; i++) monsters[states[i].internalId] = states[i]; };
                monsterDelta = (count, updates) => ApplyMonsterDelta(count, updates);
                model.OnFullReceived += playerFull;
                model.OnDeltaReceived += playerDelta;
                model.OnMonsterFullReceived += monsterFull;
                model.OnMonsterDeltaReceived += monsterDelta;

                var ownCharacter = GameObject.Find("OwnCharacter").transform;
                var camera = GameObject.Find("QuarterViewCamera").GetComponent<Camera>();
                var path = new[] { new Vector2(16, 16), new Vector2(22, 22), new Vector2(28, 28),
                    new Vector2(34, 34), new Vector2(39, 39) };
                foreach (var point in path)
                {
                    Assert.That(world.ZoneID == 1 && point.x >= -22 && point.x <= 42 && point.y >= -22 && point.y <= 42,
                        Is.True, "Center approach stays in the current Zone and the visible V02 ground bounds.");
                    yield return MoveTo(world, camera, ownCharacter, point, "center approach " + point.ToString());
                }

                double centerArrival = Time.realtimeSinceStartupAsDouble;
                var monsterTable = GetDictionary(observed, "monsters");
                yield return WaitForOptional(() => HasFreshMonsterType(monsterTable, 2, centerArrival), 8f);

                var freshTypes = new HashSet<ushort>();
                foreach (DictionaryEntry pair in monsterTable)
                {
                    if (LastFullSeen(pair.Value) < centerArrival) continue;
                    var state = pair.Value.GetType().GetField("State").GetValue(pair.Value);
                    ushort type = (ushort)state.GetType().GetField("monsterId").GetValue(state);
                    ushort id = (ushort)state.GetType().GetField("internalId").GetValue(state);
                    if (!monsters.TryGetValue(id, out var received)) continue;
                    AssertThatMonsterMatches(pair.Value, received);
                    freshTypes.Add(type);
                }
                foreach (ushort type in new ushort[] { 0, 1, 2 })
                {
                    var result = freshTypes.Contains(type) ? "PASS" : "NOT OBSERVED";
                    File.AppendAllText(CenterResultPath, $"C04 server monster type={type}: {result} in final center AOI.\n");
                }
                yield return Capture("c04-center-monster-types.png");

                var disconnect = GameObject.Find("DisconnectButton")?.GetComponent<Button>();
                Assert.That(disconnect != null && disconnect.interactable, Is.True, "The final session must remain cleanly disconnectable.");
                yield return ClickButton(disconnect);
                yield return WaitFor(() => !world.IsWorldVisible && playersTable.Count == 0 &&
                    GetDictionary(observed, "monsters").Count == 0, 8f, "final primary disconnect and observation cleanup");
                Assert.That(GetField(world, "moveTarget") == null, Is.True, "Final disconnect must clear the movement target.");
                File.AppendAllText(CenterResultPath, "PASS final primary disconnect; C04 observations and movement target cleared.\n");
            }
            finally
            {
                if (model != null)
                {
                    model.OnFullReceived -= playerFull;
                    model.OnDeltaReceived -= playerDelta;
                    model.OnMonsterFullReceived -= monsterFull;
                    model.OnMonsterDeltaReceived -= monsterDelta;
                }
                model = null;
                if (mouse != null && mouse.added) InputSystem.RemoveDevice(mouse);
                mouse = null;
                Application.runInBackground = oldBackground;
                if (focusedWindow != null) focusedWindow.Focus();
            }
        }

        private static void AssertFreshObservations(IDictionary entries, double sessionStartedAt)
        {
            foreach (DictionaryEntry pair in entries)
            {
                var lastFull = (double)pair.Value.GetType().GetField("LastFullSeen").GetValue(pair.Value);
                Assert.That(lastFull, Is.GreaterThanOrEqualTo(sessionStartedAt),
                    "An entity retained across disconnect must have received a Full in the new session.");
            }
        }
        private static bool HasFreshMonsterType(IDictionary entries, ushort type, double since)
        {
            foreach (DictionaryEntry pair in entries)
            {
                if (LastFullSeen(pair.Value) < since) continue;
                var state = pair.Value.GetType().GetField("State").GetValue(pair.Value);
                if ((ushort)state.GetType().GetField("monsterId").GetValue(state) == type) return true;
            }
            return false;
        }
        private static double LastFullSeen(object entry) =>
            (double)entry.GetType().GetField("LastFullSeen").GetValue(entry);

        private IEnumerator MoveTo(C02WorldScreen world, Camera camera, Transform ownCharacter, Vector2 target, string label)
        {
            var start = world.ServerPosition;
            yield return ClickWorldTarget(camera, target);
            yield return WaitFor(() => Vector2.Distance(world.ServerPosition, target) <= 0.15f,
                15f, "server-confirmed movement to the " + label);
            Assert.That(ownCharacter.position, Is.EqualTo(new Vector3(world.ServerPosition.x, 0, world.ServerPosition.y)),
                "The local visual must follow server-confirmed coordinates at the " + label + ".");
            AssertOwnFacing(world, ownCharacter);
            File.AppendAllText(ResultPath, string.Format(CultureInfo.InvariantCulture,
                "PASS C03 {0}; start=({1:F3},{2:F3}); confirmed=({3:F3},{4:F3}){5}",
                label, start.x, start.y, world.ServerPosition.x, world.ServerPosition.y, Environment.NewLine));
        }

        private IEnumerator ClickWorldTarget(Camera camera, Vector2 target)
        {
            var screen = camera.WorldToScreenPoint(new Vector3(target.x, 0, target.y));
            Assert.That(screen.z > 0 && screen.x >= 0 && screen.x < Screen.width && screen.y >= 0 && screen.y < Screen.height,
                Is.True, "The selected C03 target must be visible in the current Game view.");
            yield return ClickAt(new Vector2(screen.x, screen.y));
        }

        private IEnumerator ClickButton(Selectable target)
        {
            var rect = target.transform as RectTransform;
            Assert.That(rect != null, Is.True, "The requested UI target must have a RectTransform.");
            var position = RectTransformUtility.WorldToScreenPoint(null, rect.position);
            yield return ClickAt(position);
        }

        private void ApplyPlayerDelta(ushort count, DeltaUpdateField[] updates)
        {
            for (int i = 0; i < count; i++)
            {
                var update = updates[i];
                if (!players.TryGetValue(update.zoneInternalID, out var state)) continue;
                switch (update.fieldID)
                {
                    case 0: state.hp = unchecked((int)update.fieldVal); break;
                    case 2: state.maxHp = unchecked((int)update.fieldVal); break;
                    case 6: if (update.fieldVal <= 3) state.dir = (byte)update.fieldVal; break;
                    case 7: state.x = BitConverter.ToSingle(BitConverter.GetBytes(update.fieldVal), 0); break;
                    case 8: state.y = BitConverter.ToSingle(BitConverter.GetBytes(update.fieldVal), 0); break;
                }
                players[update.zoneInternalID] = state;
            }
        }

        private void ApplyMonsterDelta(ushort count, MonsterDeltaField[] updates)
        {
            for (int i = 0; i < count; i++)
            {
                var update = updates[i];
                if (!monsters.TryGetValue(update.internalId, out var state)) continue;
                switch (update.fieldId)
                {
                    case 0: state.hp = unchecked((int)update.fieldVal); break;
                    case 1: state.x = BitConverter.ToSingle(BitConverter.GetBytes(update.fieldVal), 0); break;
                    case 2: state.y = BitConverter.ToSingle(BitConverter.GetBytes(update.fieldVal), 0); break;
                    case 3: if (update.fieldVal <= 3) state.dir = (byte)update.fieldVal; break;
                }
                monsters[update.internalId] = state;
            }
        }

        private IEnumerator ClickAt(Vector2 point)
        {
            InputSystem.QueueStateEvent(mouse, new MouseState { position = point });
            yield return NextPlayerFrame();
            InputSystem.QueueStateEvent(mouse, new MouseState { position = point }.WithButton(MouseButton.Left));
            yield return NextPlayerFrame();
            InputSystem.QueueStateEvent(mouse, new MouseState { position = point });
            yield return NextPlayerFrame();
        }

        private static IEnumerator NextPlayerFrame()
        {
            int frame = Time.frameCount;
            double deadline = EditorApplication.timeSinceStartup + 3;
            EditorApplication.QueuePlayerLoopUpdate();
            while (Time.frameCount == frame && EditorApplication.timeSinceStartup < deadline) yield return null;
            Assert.That(Time.frameCount > frame, Is.True, "The queued native input requires a real Player frame.");
        }

        private static IEnumerator WaitFor(Func<bool> condition, float seconds, string step)
        {
            float deadline = Time.realtimeSinceStartup + seconds;
            while (!condition() && Time.realtimeSinceStartup < deadline) yield return null;
            Assert.That(condition(), Is.True, "Timed out waiting for " + step + ".");
        }
        private static IEnumerator WaitSeconds(float seconds)
        {
            float deadline = Time.realtimeSinceStartup + seconds;
            while (Time.realtimeSinceStartup < deadline) yield return null;
        }
        private static IEnumerator WaitForOptional(Func<bool> condition, float seconds)
        {
            float deadline = Time.realtimeSinceStartup + seconds;
            while (!condition() && Time.realtimeSinceStartup < deadline) yield return null;
        }

        private static IEnumerator Capture(string name)
        {
            var path = Path.Combine(Path.GetDirectoryName(Application.dataPath), "Captures", name);
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            ScreenCapture.CaptureScreenshot(path);
            float deadline = Time.realtimeSinceStartup + 5;
            while ((!File.Exists(path) || new FileInfo(path).Length == 0) && Time.realtimeSinceStartup < deadline) yield return null;
            Assert.That(File.Exists(path) && new FileInfo(path).Length > 0, Is.True, "The requested C04 capture was not created.");
        }

        private static IDictionary GetDictionary(object target, string name) =>
            (IDictionary)GetField(target, name);
        private static object GetField(object target, string name) =>
            target.GetType().GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).GetValue(target);
        private static void AssertThatObservationMatches(object entry, float x, float y, byte dir)
        {
            var state = entry.GetType().GetField("State").GetValue(entry);
            Assert.That((float)state.GetType().GetField("x").GetValue(state), Is.EqualTo(x).Within(0.001));
            Assert.That((float)state.GetType().GetField("y").GetValue(state), Is.EqualTo(y).Within(0.001));
            Assert.That((byte)state.GetType().GetField("dir").GetValue(state), Is.EqualTo(dir));
            var visual = (GameObject)entry.GetType().GetField("Visual").GetValue(entry);
            Assert.That(visual.transform.position, Is.EqualTo(new Vector3(x, 0, y)));
            var expectedRotation = Quaternion.Euler(0, dir == 0 ? 180 : dir == 1 ? 0 : dir == 2 ? 90 : 270, 0);
            Assert.That(Quaternion.Angle(visual.transform.rotation, expectedRotation), Is.LessThan(0.01f));
        }
        private static void AssertThatMonsterMatches(object entry, MonsterFullField state)
        {
            var stored = entry.GetType().GetField("State").GetValue(entry);
            Assert.That((ushort)stored.GetType().GetField("internalId").GetValue(stored), Is.EqualTo(state.internalId));
            Assert.That((ushort)stored.GetType().GetField("monsterId").GetValue(stored), Is.EqualTo(state.monsterId));
            Assert.That((float)stored.GetType().GetField("x").GetValue(stored), Is.EqualTo(state.x).Within(0.001));
            Assert.That((float)stored.GetType().GetField("y").GetValue(stored), Is.EqualTo(state.y).Within(0.001));
            var visual = (GameObject)entry.GetType().GetField("Visual").GetValue(entry);
            Assert.That(visual.transform.position, Is.EqualTo(new Vector3(state.x, 0, state.y)));
            var expectedRotation = Quaternion.Euler(0, state.dir == 0 ? 180 : state.dir == 1 ? 0 : state.dir == 2 ? 90 : 270, 0);
            Assert.That(Quaternion.Angle(visual.transform.rotation, expectedRotation), Is.LessThan(0.01f));
        }
        private static void AssertOwnFacing(C02WorldScreen world, Transform ownCharacter)
        {
            var state = (FullStateField)GetField(world, "ownFull");
            var expectedRotation = Quaternion.Euler(0, state.dir == 0 ? 180 : state.dir == 1 ? 0 : state.dir == 2 ? 90 : 270, 0);
            Assert.That(Quaternion.Angle(ownCharacter.rotation, expectedRotation), Is.LessThan(0.01f),
                "The visible character's facing must match the latest server-confirmed direction.");
        }
        private static EditorWindow FindGameView()
        {
            foreach (var window in Resources.FindObjectsOfTypeAll<EditorWindow>())
                if (window != null && window.GetType().Name == "GameView") return window;
            return null;
        }
        private static string ResultPath => Path.GetFullPath(Path.Combine(Application.dataPath, "../Captures/c04-live-batch.txt"));
        private static string CenterResultPath => Path.GetFullPath(Path.Combine(Application.dataPath, "../Captures/c04-center-types.txt"));

        public static void StartAfterPeerObservation(long expectedPeerId)
        {
            if (!EditorApplication.isPlaying || expectedPeerId <= 0)
                throw new InvalidOperationException("A live C02 entry and approved peer Zone ID are required.");
            var instance = new C04LiveBatchUiTests();
            var runner = FindLoadedType("C01.Tests.C01EditorInputRun");
            var start = runner.GetMethod("Start", new[] { typeof(IEnumerator), typeof(string), typeof(string) });
            start.Invoke(null, new object[] { instance.ObservePeerAndWorld(expectedPeerId), ResultPath,
                "PASS: C04 real server observations and C03 ground-input assertions completed." });
        }
        public static void StartAfterPeerExit()
        {
            if (!EditorApplication.isPlaying || peerInternalId == 0)
                throw new InvalidOperationException("A previously observed peer and active Play session are required.");
            var instance = new C04LiveBatchUiTests();
            var runner = FindLoadedType("C01.Tests.C01EditorInputRun");
            var start = runner.GetMethod("Start", new[] { typeof(IEnumerator), typeof(string), typeof(string) });
            start.Invoke(null, new object[] { instance.PeerTtlAndCenterMonsterTypes(), CenterResultPath,
                "PASS: C04 TTL, center-cell server monster observation, and final cleanup phase completed." });
        }
        private static Type FindLoadedType(string fullName)
        {
            foreach (var assembly in AppDomain.CurrentDomain.GetAssemblies())
            {
                var type = assembly.GetType(fullName, false);
                if (type != null) return type;
            }
            throw new InvalidOperationException("The shared C01 Editor-update driver is not loaded.");
        }
    }
}
#endif
