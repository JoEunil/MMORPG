#if UNITY_EDITOR
using System;
using System.Collections;
using System.Globalization;
using System.IO;
using System.Reflection;
using System.Collections.Generic;
using System.Text.RegularExpressions;
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
    // Phase one ends with no game client so the coordinator can inspect persistence before phase two.
    public sealed class B05LiveBatchUiTests
    {
        private static readonly CultureInfo Invariant = CultureInfo.InvariantCulture;
        private Mouse mouse;
        private string runId;
        private string resultPath;
        private C01.C01LoginScreen login;
        private C02WorldScreen world;
        private IViewModelUI entryModel;
        private Action<IViewModelUI> clientChanged;
        private Action<ushort, ulong, uint, float, float> zoneHandler;
        private Action<ushort, FullStateField[]> fullHandler;
        private uint zoneInternalId;
        private bool hpZeroObserved;
        private readonly Dictionary<uint, FullStateField> firstFullByInternalId = new Dictionary<uint, FullStateField>();

        public static void StartPhaseOne()
        {
            string id = DateTime.UtcNow.ToString("yyyyMMdd'T'HHmmssfff'Z'", Invariant);
            var instance = new B05LiveBatchUiTests
            {
                runId = id,
                resultPath = CapturePath("b05-persistence-" + id + "-phase1.txt")
            };
            instance.Start(instance.PhaseOne(), instance.resultPath,
                "COMPLETED: B05 phase one; inspect final Full and disconnected observation, then take READ ONLY snapshot before phase two.");
        }

        public static void StartPhaseTwo(string id)
        {
            if (string.IsNullOrEmpty(id) || !Regex.IsMatch(id, @"^\d{8}T\d{9}Z$"))
                throw new ArgumentException("Use the run ID printed by B05 phase one.", nameof(id));
            var instance = new B05LiveBatchUiTests
            {
                runId = id,
                resultPath = CapturePath("b05-persistence-" + id + "-phase2.txt")
            };
            instance.Start(instance.PhaseTwo(), instance.resultPath,
                "COMPLETED: B05 phase two; inspect reconnect Full comparison.");
        }

        private IEnumerator PhaseOne()
        {
            bool previousBackground = Application.runInBackground;
            try
            {
                AssertCaptureAvailable("b05-persistence-" + runId + "-outer.png");
                Application.runInBackground = true;
                string loginId = CurrentLoginId();
                ulong characterId = CharacterId(loginId);
                Append("PHASE1 native UI login/selection/ENTER. Dedicated character=" + characterId + ".");
                PreserveC02EntryEvidence("phase1");
                yield return EntryAndObserveFirstFull(characterId);
                BindWorld();
                mouse = InputSystem.AddDevice<Mouse>();
                var gameView = FindGameView();
                Assert.That(gameView != null, Is.True, "Open the Game view for native B05 input.");
                gameView.Focus();
                yield return null;
                Assert.That(Application.isFocused, Is.True, "Unity Editor must have OS focus for native B05 input.");

                var fresh = FirstRawOwnFull();
                AppendState("FRESH", characterId, fresh);
                if (fresh.hp <= 0)
                {
                    Append("BLOCKED_HP0: fresh server Full is HP=0; no field movement or attack was sent.");
                    yield return DisconnectNormally();
                    yield return WaitDisconnectedTenSeconds();
                    Assert.Fail("Fresh Full has HP=0. Stopped without movement.");
                }

                yield return MoveToOuterPoint(new Vector2(-4f, -4f));

                var final = world.OwnFull;
                var finalPosition = world.ServerPosition;
                if (final.hp <= 0)
                {
                    Append("BLOCKED_HP0: server Full reached HP=0 during field movement; no further input was sent.");
                    yield return DisconnectNormally();
                    yield return WaitDisconnectedTenSeconds();
                    Assert.Fail("HP reached zero during movement. Stopped without further input.");
                }
                Assert.That(Vector2.Distance(finalPosition, new Vector2(-4f, -4f)), Is.LessThanOrEqualTo(1.8f),
                    "Server position must reach the visible Zone 1 outer point.");
                AppendState("PRE_DISCONNECT_FINAL", characterId, final);
                yield return CapturePng("b05-persistence-" + runId + "-outer.png");
                var baseline = new Baseline(characterId, world.ZoneID, finalPosition.x, finalPosition.y, final.hp, final.mp);

                yield return DisconnectNormally();
                yield return WaitDisconnectedTenSeconds();
                WriteBaseline(baseline);
                Append("PASS: disconnected for 10 seconds with no game model. No reconnect was opened in phase one.");
                Append("NEXT: coordinator performs the approved READ ONLY snapshot before phase two.");
            }
            finally
            {
                UnhookEntryModel();
                if (login != null && login.Model != null) login.ReleaseClient();
                if (mouse != null && mouse.added) InputSystem.RemoveDevice(mouse);
                mouse = null;
                Application.runInBackground = previousBackground;
            }
        }

        private IEnumerator PhaseTwo()
        {
            bool previousBackground = Application.runInBackground;
            try
            {
                AssertCaptureAvailable("b05-persistence-" + runId + "-reconnect.png");
                var baseline = ReadBaseline();
                string loginId = CurrentLoginId();
                ulong characterId = CharacterId(loginId);
                Assert.That(characterId, Is.EqualTo(baseline.CharacterId),
                    "Reconnect must use the same dedicated character as phase one.");
                Application.runInBackground = true;
                Append("PHASE2 native UI reconnect/selection/ENTER. Comparing the first entered own Full with the phase-one final Full.");
                PreserveC02EntryEvidence("phase2");
                yield return EntryAndObserveFirstFull(characterId);
                BindWorld();
                mouse = InputSystem.AddDevice<Mouse>();

                var reconnected = FirstRawOwnFull();
                AppendState("RECONNECT_FULL", characterId, reconnected);
                bool same = world.ZoneID == baseline.Zone && reconnected.hp == baseline.Hp && reconnected.mp == baseline.Mp &&
                    Mathf.Abs(reconnected.x - baseline.X) <= 0.02f && Mathf.Abs(reconnected.y - baseline.Y) <= 0.02f;
                Append("COMPARE phase1FinalVsReconnectFull: zone=" + baseline.Zone + "->" + world.ZoneID +
                    "; hp=" + baseline.Hp + "->" + reconnected.hp + "; mp=" + baseline.Mp + "->" + reconnected.mp +
                    "; x=" + baseline.X.ToString("F3", Invariant) + "->" + reconnected.x.ToString("F3", Invariant) +
                    "; y=" + baseline.Y.ToString("F3", Invariant) + "->" + reconnected.y.ToString("F3", Invariant) +
                    "; result=" + (same ? "PASS" : "FAIL") + ". Compare discrepancies with the coordinator READ ONLY row.");
                yield return CapturePng("b05-persistence-" + runId + "-reconnect.png");
                yield return DisconnectNormally();
                Assert.That(same, Is.True, "Reconnect Full differs from the phase-one final Full; compare with READ ONLY DB evidence.");
            }
            finally
            {
                UnhookEntryModel();
                if (login != null && login.Model != null) login.ReleaseClient();
                if (mouse != null && mouse.added) InputSystem.RemoveDevice(mouse);
                mouse = null;
                Application.runInBackground = previousBackground;
            }
        }

        private IEnumerator MoveToOuterPoint(Vector2 destination)
        {
            var movement = new C05LiveBatchUiTests();
            SetPrivateField(movement, "resultPath", resultPath);
            SetPrivateField(movement, "mouse", mouse);
            InvokePrivate(movement, "BindLiveWorld");
            InvokePrivate(movement, "Subscribe");
            Action worldChanged = () => { if (!world.CanAct || world.OwnFull.hp <= 0) hpZeroObserved = true; };
            world.WorldStateChanged += worldChanged;
            try
            {
                var camera = GameObject.Find("QuarterViewCamera")?.GetComponent<Camera>();
                var own = GameObject.Find("OwnCharacter")?.transform;
                Assert.That(camera != null && own != null, Is.True, "Quarter-view camera/player visual is missing.");
                yield return (IEnumerator)InvokePrivate(movement, "WaitFor", new Func<bool>(() => GetPrivateField(world, "moveTarget") == null), 12f,
                    "existing native movement to finish before the approach");
                double deadline = Time.realtimeSinceStartupAsDouble + 75;
                for (int step = 0; step < 12 && Vector2.Distance(world.ServerPosition, destination) > 1.8f; step++)
                {
                    if (hpZeroObserved || !world.CanAct)
                    {
                        Append("BLOCKED_HP0: C05 safe visible-movement routine stopped; normal UI disconnect follows.");
                        yield return DisconnectNormally();
                        yield return WaitDisconnectedTenSeconds();
                        Assert.Fail("HP reached zero during movement. Stopped without further input.");
                    }
                    yield return NextPlayerFrame();
                    yield return NextPlayerFrame();
                    Assert.That(Time.realtimeSinceStartupAsDouble < deadline, Is.True, "Outer movement exceeded 75 seconds.");
                    Vector2 start = world.ServerPosition;
                    float startDistance = Vector2.Distance(start, destination);
                    Vector2 point = Vector2.MoveTowards(start, destination, Mathf.Min(5f, Vector2.Distance(start, destination)));
                    Vector3 screen = camera.WorldToScreenPoint(new Vector3(point.x, 0, point.y));
                    Assert.That(screen.z > 0 && screen.x >= 0 && screen.x < Screen.width && screen.y >= 0 && screen.y < Screen.height,
                        Is.True, "Outer target must be visible before clicking.");
                    InvokePrivate(movement, "RecordApproachClick", world, screen, point, Vector2.Distance(start, destination));
                    yield return (IEnumerator)InvokePrivate(movement, "ClickAt", new Vector2(screen.x, screen.y));
                    if (hpZeroObserved || !world.CanAct)
                    {
                        Append("BLOCKED_HP0: server HP reached zero during the current click; no additional destination click was queued.");
                        yield return DisconnectNormally();
                        yield return WaitDisconnectedTenSeconds();
                        Assert.Fail("HP reached zero during movement. Stopped without further input.");
                    }
                    double waitDeadline = Time.realtimeSinceStartupAsDouble + 12;
                    while (Vector2.Distance(world.ServerPosition, destination) >= startDistance - 0.75f &&
                        Vector2.Distance(world.ServerPosition, destination) > 1.8f && Time.realtimeSinceStartupAsDouble < waitDeadline)
                    {
                        if (hpZeroObserved || !world.CanAct) break;
                        yield return null;
                    }
                    if (hpZeroObserved || !world.CanAct)
                    {
                        Append("BLOCKED_HP0: server HP reached zero while observing movement; no additional destination click was queued.");
                        yield return DisconnectNormally();
                        yield return WaitDisconnectedTenSeconds();
                        Assert.Fail("HP reached zero during movement. Stopped without further input.");
                    }
                    Assert.That(Vector2.Distance(world.ServerPosition, destination) < startDistance - 0.75f ||
                        Vector2.Distance(world.ServerPosition, destination) <= 1.8f, Is.True,
                        "No server-confirmed movement progress toward the outer point.");
                    yield return (IEnumerator)InvokePrivate(movement, "WaitFor", new Func<bool>(() => GetPrivateField(world, "moveTarget") == null), 12f,
                        "native outer move target to finish before the next projection");
                    Append("MOVE server=" + world.ServerPosition.x.ToString("F3", Invariant) + "," + world.ServerPosition.y.ToString("F3", Invariant) +
                        "; hp=" + world.OwnFull.hp + "; mp=" + world.OwnFull.mp);
                }
                Assert.That(hpZeroObserved || world.OwnFull.hp > 0, Is.True, "Character must remain alive during outer movement.");
                Assert.That(Vector2.Distance(world.ServerPosition, destination), Is.LessThanOrEqualTo(1.8f),
                    "Server position did not reach the outer point.");
                yield return (IEnumerator)InvokePrivate(movement, "WaitFor", new Func<bool>(() => GetPrivateField(world, "moveTarget") == null), 12f,
                    "final native outer movement to settle");
                Assert.That(Vector3.Distance(own.position, new Vector3(world.ServerPosition.x, 0, world.ServerPosition.y)), Is.LessThanOrEqualTo(0.02f),
                    "Character visual must settle near the server-confirmed outer position.");
                Append("PASS: C05 native visible-point route reached the server-confirmed outer point; see click/HUD diagnostics above.");
            }
            finally
            {
                world.WorldStateChanged -= worldChanged;
                InvokePrivate(movement, "Unsubscribe");
            }
        }

        private IEnumerator EntryAndObserveFirstFull(ulong expectedCharacterId)
        {
            IEnumerator entry = new C02EmptyListUiTests().RealCharacterSelectionAndEntry_AndCapture(expectedCharacterId);
            try
            {
                bool sceneLoaded = false;
                while (entry.MoveNext())
                {
                    if (!sceneLoaded)
                    {
                        sceneLoaded = true;
                        yield return entry.Current; // C02's existing scene-load routine.
                        login = GameObject.Find("C01Client")?.GetComponent<C01.C01LoginScreen>();
                        Assert.That(login != null, Is.True, "C02 login component was not created after scene load.");
                        clientChanged = OnClientChanged;
                        login.ClientChanged += clientChanged;
                        Append("INFO subscribed to raw ClientCore Zone/Full immediately after C02 scene load and before login/ENTER.");
                        continue;
                    }
                    yield return entry.Current;
                }
            }
            finally
            {
                (entry as IDisposable)?.Dispose();
                UnhookEntryModel();
            }
        }

        private void OnClientChanged(IViewModelUI model)
        {
            if (entryModel != null)
            {
                entryModel.OnZoneChageReceived -= zoneHandler;
                entryModel.OnFullReceived -= fullHandler;
            }
            entryModel = model;
            if (entryModel == null) return;
            zoneHandler = (zone, chatId, internalId, x, y) => zoneInternalId = internalId;
            fullHandler = (count, states) =>
            {
                for (int i = 0; i < count; i++)
                    if (!firstFullByInternalId.ContainsKey(states[i].zoneInternalID))
                        firstFullByInternalId.Add(states[i].zoneInternalID, states[i]);
            };
            entryModel.OnZoneChageReceived += zoneHandler;
            entryModel.OnFullReceived += fullHandler;
        }

        private void UnhookEntryModel()
        {
            if (entryModel != null)
            {
                if (zoneHandler != null) entryModel.OnZoneChageReceived -= zoneHandler;
                if (fullHandler != null) entryModel.OnFullReceived -= fullHandler;
            }
            if (login != null && clientChanged != null) login.ClientChanged -= clientChanged;
            entryModel = null; clientChanged = null; zoneHandler = null; fullHandler = null;
        }

        private FullStateField FirstRawOwnFull()
        {
            Assert.That(world != null && world.InternalID != 0 && zoneInternalId == world.InternalID, Is.True,
                "The observed Zone internal ID must match the entered own character.");
            Assert.That(firstFullByInternalId.TryGetValue(world.InternalID, out var first), Is.True,
                "No raw Full was observed for the entered own character.");
            return first;
        }

        private static object InvokePrivate(object target, string name, params object[] args) =>
            target.GetType().GetMethod(name, BindingFlags.Instance | BindingFlags.NonPublic).Invoke(target, args);

        private static void SetPrivateField(object target, string name, object value) =>
            target.GetType().GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).SetValue(target, value);

        private void PreserveC02EntryEvidence(string phase)
        {
            string captures = CapturePath("");
            string[] names = { "c02-live-character-list.png", "c02-live-entry.png", "c02-live-entry-ui.txt" };
            foreach (string name in names)
            {
                string source = Path.Combine(captures, name);
                if (!File.Exists(source)) continue;
                string preserved = Path.Combine(captures, "b05-persistence-" + runId + "-" + phase + "-preserved-" + name);
                using (var input = new FileStream(source, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
                using (var output = new FileStream(preserved, FileMode.CreateNew, FileAccess.Write, FileShare.None))
                    input.CopyTo(output);
                Append("PRESERVED existing " + name + " before C02 helper reuse.");
            }
        }

        private IEnumerator DisconnectNormally()
        {
            var button = GameObject.Find("DisconnectButton")?.GetComponent<Button>();
            Assert.That(button != null && button.interactable, Is.True, "Normal disconnect button is unavailable.");
            yield return Click(button);
            yield return WaitFor(() => !world.IsWorldVisible && login.Model == null, 8f,
                "normal disconnect and client release");
            Append("PASS: normal UI disconnect; ClientCore released.");
        }

        private IEnumerator WaitDisconnectedTenSeconds()
        {
            Assert.That(login != null && login.Model == null, Is.True, "Game model must be released before save observation.");
            double deadline = Time.realtimeSinceStartupAsDouble + 10;
            while (Time.realtimeSinceStartupAsDouble < deadline)
            {
                Assert.That(login.Model == null, Is.True, "No game connection may be opened during save observation.");
                yield return null;
            }
        }

        private IEnumerator Click(Selectable target)
        {
            Assert.That(target != null && mouse != null, Is.True, "Native UI click target/device is missing.");
            var rect = target.transform as RectTransform;
            Assert.That(rect != null, Is.True, "Native UI button requires RectTransform.");
            yield return ClickAt(RectTransformUtility.WorldToScreenPoint(null, rect.position));
        }

        private IEnumerator ClickAt(Vector2 position)
        {
            InputSystem.QueueStateEvent(mouse, new MouseState { position = position });
            yield return NextPlayerFrame();
            InputSystem.QueueStateEvent(mouse, new MouseState { position = position }.WithButton(MouseButton.Left));
            yield return NextPlayerFrame();
            InputSystem.QueueStateEvent(mouse, new MouseState { position = position });
            yield return NextPlayerFrame();
        }

        private IEnumerator CapturePng(string name)
        {
            string path = CapturePath(name);
            Assert.That(!File.Exists(path), Is.True, "B05 does not overwrite earlier captures.");
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            ScreenCapture.CaptureScreenshot(path);
            double deadline = Time.realtimeSinceStartupAsDouble + 5;
            while ((!File.Exists(path) || new FileInfo(path).Length == 0) && Time.realtimeSinceStartupAsDouble < deadline)
                yield return null;
            Assert.That(File.Exists(path) && new FileInfo(path).Length > 0, Is.True, "B05 Game View capture was not written.");
        }

        private static IEnumerator NextPlayerFrame()
        {
            int frame = Time.frameCount;
            double deadline = EditorApplication.timeSinceStartup + 3;
            EditorApplication.QueuePlayerLoopUpdate();
            while (Time.frameCount == frame && EditorApplication.timeSinceStartup < deadline) yield return null;
            Assert.That(Time.frameCount > frame, Is.True, "Native input requires a real Player frame.");
        }

        private IEnumerator WaitFor(Func<bool> condition, float seconds, string stage)
        {
            double deadline = Time.realtimeSinceStartupAsDouble + seconds;
            while (!condition() && Time.realtimeSinceStartupAsDouble < deadline) yield return null;
            Assert.That(condition(), Is.True, "Timed out at B05 stage: " + stage + ".");
        }

        private void BindWorld()
        {
            login = GameObject.Find("C01Client")?.GetComponent<C01.C01LoginScreen>();
            world = GameObject.Find("C02Client")?.GetComponent<C02WorldScreen>();
            Assert.That(login != null && world != null && login.Model != null && world.Stage == C02Stage.InWorld &&
                world.HasOwnFull && world.InternalID != 0, Is.True, "B05 requires the real entered C02 world and own Full.");
            Assert.That(world.ZoneID == 1 && world.SelectedCharacterID == CharacterId(CurrentLoginId()), Is.True,
                "Only the selected dedicated character in Zone 1 is allowed.");
        }

        private static string CurrentLoginId()
        {
            string loginId = Environment.GetEnvironmentVariable("C01_TEST_LOGIN_ID");
            if (string.IsNullOrEmpty(loginId)) loginId = "unity_ai_01";
            if (loginId != "unity_ai_01" && loginId != "unity_ai_02")
                throw new InvalidOperationException("Only unity_ai_01 or unity_ai_02 may run B05.");
            return loginId;
        }

        private static ulong CharacterId(string loginId) => loginId == "unity_ai_01" ? 5011UL : 5012UL;

        private void AppendState(string label, ulong characterId, ClientCore.PacketHelper.FullStateField state) =>
            Append(label + " characterId=" + characterId + "; zone=" + world.ZoneID + "; x=" + state.x.ToString("R", Invariant) +
                "; y=" + state.y.ToString("R", Invariant) + "; hp=" + state.hp + "; mp=" + state.mp +
                "; maxHp=" + state.maxHp + "; maxMp=" + state.maxMp);

        private void WriteBaseline(Baseline baseline)
        {
            string path = CapturePath("b05-persistence-" + runId + "-baseline.txt");
            Assert.That(!File.Exists(path), Is.True, "B05 never overwrites an earlier baseline.");
            File.WriteAllText(path, string.Format(Invariant,
                "run={0}\ncharacterId={1}\nzone={2}\nx={3:R}\ny={4:R}\nhp={5}\nmp={6}\n",
                runId, baseline.CharacterId, baseline.Zone, baseline.X, baseline.Y, baseline.Hp, baseline.Mp));
        }

        private Baseline ReadBaseline()
        {
            string path = CapturePath("b05-persistence-" + runId + "-baseline.txt");
            Assert.That(File.Exists(path), Is.True, "Run phase one and inspect its READ ONLY window before phase two.");
            var values = new System.Collections.Generic.Dictionary<string, string>(StringComparer.Ordinal);
            foreach (string line in File.ReadAllLines(path))
            {
                int separator = line.IndexOf('=');
                if (separator > 0) values[line.Substring(0, separator)] = line.Substring(separator + 1);
            }
            Assert.That(values.TryGetValue("run", out var savedRun) && savedRun == runId, Is.True, "B05 baseline run ID mismatch.");
            return new Baseline(ulong.Parse(values["characterId"], Invariant), ushort.Parse(values["zone"], Invariant),
                float.Parse(values["x"], Invariant), float.Parse(values["y"], Invariant),
                int.Parse(values["hp"], Invariant), int.Parse(values["mp"], Invariant));
        }

        private void Start(IEnumerator scenario, string path, string passMessage)
        {
            if (!EditorApplication.isPlaying) throw new InvalidOperationException("B05 live input requires Play mode.");
            if (File.Exists(path)) throw new IOException("B05 result already exists; earlier evidence is preserved.");
            var runner = FindLoadedType("C01.Tests.C01EditorInputRun");
            var method = runner.GetMethod("Start",
                new[] { typeof(IEnumerator), typeof(string), typeof(string) });
            Assert.That(method != null, Is.True, "Shared C01 Editor-update runner is unavailable.");
            method.Invoke(null, new object[] { scenario, path, passMessage });
        }

        private static Type FindLoadedType(string name)
        {
            foreach (var assembly in AppDomain.CurrentDomain.GetAssemblies())
            {
                var type = assembly.GetType(name, false);
                if (type != null) return type;
            }
            throw new InvalidOperationException("The shared C01 Editor-update runner is not loaded.");
        }

        private static string CapturePath(string name) => Path.GetFullPath(Path.Combine(Application.dataPath, "../Captures", name));
        private static object GetPrivateField(object target, string name) =>
            target.GetType().GetField(name, System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic).GetValue(target);
        private static EditorWindow FindGameView()
        {
            foreach (var window in Resources.FindObjectsOfTypeAll<EditorWindow>())
                if (window != null && window.GetType().Name == "GameView") return window;
            return null;
        }
        private void Append(string line) => File.AppendAllText(resultPath, line + Environment.NewLine);

        private static void AssertCaptureAvailable(string name)
        {
            if (File.Exists(CapturePath(name))) throw new IOException("B05 capture already exists; earlier evidence is preserved.");
        }

        private readonly struct Baseline
        {
            public readonly ulong CharacterId;
            public readonly ushort Zone;
            public readonly float X, Y;
            public readonly int Hp, Mp;
            public Baseline(ulong characterId, ushort zone, float x, float y, int hp, int mp)
            { CharacterId = characterId; Zone = zone; X = x; Y = y; Hp = hp; Mp = mp; }
        }
    }
}
#endif
