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
using TMPro;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.LowLevel;
using UnityEngine.SceneManagement;
using UnityEngine.UI;

namespace C02.Tests
{
    // This scenario verifies only the real empty-list UI path. Fixture preview is labeled separately.
    public sealed class C02EmptyListUiTests
    {
        private Mouse testMouse;
        private string inputDiagnosticResultPath;

        public IEnumerator EmptyListDisablesEntry_AndCapturesLabeledStates()
        {
            var account = ReadAccount();
            var loginId = ReadString(account, "LoginId");
            var password = ReadString(account, "Password");
            var originalRunInBackground = Application.runInBackground;
            var focusedWindow = EditorWindow.focusedWindow;
            TMP_InputField passwordInput = null;

            try
            {
                Application.runInBackground = true;
                inputDiagnosticResultPath = Path.GetFullPath(Path.Combine(Application.dataPath, "../Captures/c02-empty-list-ui.txt"));
                testMouse = InputSystem.AddDevice<Mouse>();
                yield return LoadC02Scene();

                var loginRoot = GameObject.Find("C01Client");
                var worldRoot = GameObject.Find("C02Client");
                Assert.That(loginRoot != null && worldRoot != null, Is.True, "C02 scene roots were not found.");
                var login = loginRoot.GetComponent<C01LoginScreen>();
                var world = worldRoot.GetComponent<C02WorldScreen>();
                var loginIdInput = Find<TMP_InputField>(login, "Canvas/C01Panel/SignIn/LoginIdInput");
                passwordInput = Find<TMP_InputField>(login, "Canvas/C01Panel/SignIn/PasswordInput");
                var loginButton = Find<Button>(login, "Canvas/C01Panel/SignIn/LoginButton");
                var loginStatus = Find<TMP_Text>(login, "Canvas/C01Panel/Results/StatusText");
                var listStatus = Find<TMP_Text>(login, "Canvas/C01Panel/Results/CharacterListText");
                var entryStatus = GameObject.Find("EntryStatusText").GetComponent<TMP_Text>();

                Assert.That(login != null && world != null && loginIdInput != null && passwordInput != null &&
                    loginButton != null && loginStatus != null && listStatus != null && entryStatus != null,
                    Is.True, "C02 scene is missing a required login or character-entry UI element.");

                yield return TypeText(loginIdInput, loginId);
                yield return TypeText(passwordInput, password);
                yield return Click(loginButton);
                yield return WaitFor(() => login.ListResponseCount > 0, 25f, "C02 server character-list response");
                yield return WaitFor(() => !login.IsBusy, 10f, "C02 login completion");

                bool emptyList = login.Authenticated && loginStatus.text.Contains("status=0") &&
                    loginStatus.text.Contains("count=0") && listStatus.text.Contains("No character rows returned.");
                Assert.That(emptyList, Is.True, "The configured account must retain the server's empty-list result.");
                Assert.That(login.ListResponseCount == 1, Is.True, "One login must produce exactly one list response.");
                Assert.That(world.Stage == C02Stage.Choosing && !world.IsWorldVisible, Is.True,
                    "An empty list must leave the world hidden and entry unavailable.");

                var choices = (Button[])typeof(C02WorldScreen)
                    .GetField("characterChoices", BindingFlags.Instance | BindingFlags.NonPublic).GetValue(world);
                bool allRowsDisabled = choices != null && choices.Length == 10;
                if (choices != null)
                    foreach (var choice in choices)
                        allRowsDisabled &= choice != null && !choice.gameObject.activeSelf && !choice.interactable;
                var enterButton = GameObject.Find("EnterButton").GetComponent<Button>();
                Assert.That(allRowsDisabled && !enterButton.interactable, Is.True,
                    "An empty server list must expose no selectable row and keep Enter disabled.");
                Assert.That(entryStatus.text.Contains("No selectable characters") &&
                    entryStatus.text.Contains("unavailable"), Is.True,
                    "The entry status must explain that world entry is unavailable.");

                bool noCredentialDisclosure = !loginStatus.text.Contains(loginId) && !loginStatus.text.Contains(password) &&
                    !listStatus.text.Contains(loginId) && !listStatus.text.Contains(password) &&
                    string.IsNullOrEmpty(passwordInput.text);
                Assert.That(noCredentialDisclosure, Is.True, "Login fields and status UI must not disclose credentials.");

                yield return CapturePng("c02-empty-list-ui.png");
                yield return CaptureFixturePreview(world, login, entryStatus);
            }
            finally
            {
                if (passwordInput != null) passwordInput.text = string.Empty;
                Application.runInBackground = originalRunInBackground;
                if (focusedWindow != null) focusedWindow.Focus();
                if (testMouse != null && testMouse.added) InputSystem.RemoveDevice(testMouse);
                testMouse = null;
                ClearAccount(account);
                loginId = null;
                password = null;
                inputDiagnosticResultPath = null;
            }
        }

        public IEnumerator RealCharacterSelectionAndEntry_AndCapture(ulong expectedCharacterID)
        {
            var account = ReadAccount();
            var loginId = ReadString(account, "LoginId");
            var password = ReadString(account, "Password");
            var expectedLoginId = expectedCharacterID == 5011UL ? "unity_ai_01" :
                expectedCharacterID == 5012UL ? "unity_ai_02" : null;
            var expectedCharacterName = expectedCharacterID == 5011UL ? "unity_ai_01_c02" :
                expectedCharacterID == 5012UL ? "unity_ai_02_c02" : null;
            var originalRunInBackground = Application.runInBackground;
            var focusedWindow = EditorWindow.focusedWindow;
            TMP_InputField passwordInput = null;
            C01LoginScreen login = null;
            C02WorldScreen world = null;
            IViewModelUI observedModel = null;
            bool zoneObserved = false;
            ushort receivedZone = 0;
            uint receivedInternalID = 0;
            var fullByInternalID = new Dictionary<uint, FullStateField>();
            int matchingFullStateCount = 0;
            Action<ushort, ulong, uint, float, float> zoneObserver = (zone, chat, internalID, x, y) =>
            {
                zoneObserved = true;
                receivedZone = zone;
                receivedInternalID = internalID;
                if (fullByInternalID.ContainsKey(internalID)) matchingFullStateCount++;
            };
            Action<ushort, FullStateField[]> fullObserver = (count, states) =>
            {
                for (int i = 0; i < count; i++)
                {
                    var state = states[i];
                    fullByInternalID[state.zoneInternalID] = state;
                    if (zoneObserved && state.zoneInternalID == receivedInternalID) matchingFullStateCount++;
                }
            };

            try
            {
                Application.runInBackground = true;
                inputDiagnosticResultPath = Path.GetFullPath(Path.Combine(Application.dataPath, "../Captures/c02-live-entry-ui.txt"));
                testMouse = InputSystem.AddDevice<Mouse>();
                yield return LoadC02Scene();

                var loginRoot = GameObject.Find("C01Client");
                var worldRoot = GameObject.Find("C02Client");
                Assert.That(loginRoot != null && worldRoot != null, Is.True, "C02 scene roots were not found.");
                login = loginRoot.GetComponent<C01LoginScreen>();
                world = worldRoot.GetComponent<C02WorldScreen>();
                var loginIdInput = Find<TMP_InputField>(login, "Canvas/C01Panel/SignIn/LoginIdInput");
                passwordInput = Find<TMP_InputField>(login, "Canvas/C01Panel/SignIn/PasswordInput");
                var loginButton = Find<Button>(login, "Canvas/C01Panel/SignIn/LoginButton");
                var loginStatus = Find<TMP_Text>(login, "Canvas/C01Panel/Results/StatusText");
                var listStatus = Find<TMP_Text>(login, "Canvas/C01Panel/Results/CharacterListText");
                var entryStatus = GameObject.Find("EntryStatusText").GetComponent<TMP_Text>();
                var choices = (Button[])typeof(C02WorldScreen)
                    .GetField("characterChoices", BindingFlags.Instance | BindingFlags.NonPublic).GetValue(world);
                var choice = choices != null && choices.Length > 0 ? choices[0] : null;
                var enterButton = GameObject.Find("EnterButton").GetComponent<Button>();

                Assert.That(login != null && world != null && loginIdInput != null && passwordInput != null &&
                    loginButton != null && loginStatus != null && listStatus != null && entryStatus != null &&
                    choice != null && enterButton != null,
                    Is.True, "C02 live-entry scene is missing a required UI or world element.");

                yield return TypeText(loginIdInput, loginId);
                yield return TypeText(passwordInput, password);
                yield return Click(loginButton);
                yield return WaitFor(() => login.ListResponseCount > 0, 25f, "C02 live character-list response");
                yield return WaitFor(() => !login.IsBusy, 10f, "C02 live login completion");

                var characters = login.Characters;
                bool expectedServerRow = expectedLoginId != null && expectedCharacterName != null &&
                    loginId == expectedLoginId && login.Authenticated && loginStatus.text.Contains("status=1") &&
                    loginStatus.text.Contains("count=1") && characters.Item1 == 1 && characters.Item2.Count == 1 &&
                    characters.Item2[0].Name.TrimEnd('\0') == expectedCharacterName &&
                    characters.Item2[0].CharacterID == expectedCharacterID &&
                    listStatus.text.Contains(expectedCharacterName) &&
                    listStatus.text.Contains(expectedCharacterID.ToString(CultureInfo.InvariantCulture));
                Assert.That(expectedServerRow, Is.True,
                    "The server list must contain the single approved C02 test character and expected ID.");
                Assert.That(login.ListResponseCount == 1, Is.True, "One login must produce exactly one list response.");
                Assert.That(choice.gameObject.activeSelf && choice.interactable && !enterButton.interactable,
                    Is.True, "The real server row must be selectable before selection, with Enter initially disabled.");
                bool noCredentialDisclosure = !loginStatus.text.Contains(loginId) && !loginStatus.text.Contains(password) &&
                    !listStatus.text.Contains(password) && string.IsNullOrEmpty(passwordInput.text);
                Assert.That(noCredentialDisclosure, Is.True, "Login fields and status UI must not disclose credentials.");
                yield return CapturePng("c02-live-character-list.png");

                yield return Click(choice);
                Assert.That(world.SelectedCharacterID == expectedCharacterID && enterButton.interactable,
                    Is.True, "Selecting the actual server row must select its ID and enable Enter.");

                observedModel = login.Model;
                Assert.That(observedModel != null, Is.True, "Authenticated C02 session must expose its live model.");
                observedModel.OnZoneChageReceived += zoneObserver;
                observedModel.OnFullReceived += fullObserver;

                yield return Click(enterButton);
                Assert.That(world.EnterRequestCount == 1, Is.True,
                    "The actual selected row must issue exactly one Enter request through the UI.");
                yield return WaitFor(() => world.Stage == C02Stage.InWorld, 20f, "server-confirmed C02 own Full and Zone");

                var worldStatus = GameObject.Find("WorldStatusText").GetComponent<TMP_Text>();
                var ownCharacter = GameObject.Find("OwnCharacter").transform;
                bool hasMatchingFull = fullByInternalID.TryGetValue(receivedInternalID, out var ownFull);
                Assert.That(zoneObserved && receivedInternalID != 0 && receivedZone == 1 && receivedZone == world.ZoneID &&
                    receivedInternalID == world.InternalID && matchingFullStateCount > 0 && hasMatchingFull,
                    Is.True, "Actual Zone and matching own Full events must confirm the same internal ID.");
                Assert.That(world.HasOwnFull && world.IsWorldVisible, Is.True,
                    "The matching server Full must show the world.");
                Assert.That(world.ServerPosition == new Vector2(ownFull.x, ownFull.y) &&
                    ownCharacter.position == new Vector3(ownFull.x, 0, ownFull.y),
                    Is.True, "Server Full coordinates must match the displayed position and own-character transform.");
                var invariant = CultureInfo.InvariantCulture;
                var expectedWorldStatus = string.Format(invariant,
                    "Character ID {0}    Internal ID {1}\nZone {2}    Server (x, y): ({3:F3}, {4:F3})",
                    expectedCharacterID, receivedInternalID, receivedZone, ownFull.x, ownFull.y);
                Assert.That(worldStatus.text.Contains(expectedWorldStatus), Is.True,
                    "The world HUD must show the selected ID, actual Zone/internal ID, and server coordinates.");
                Assert.That(!worldStatus.text.Contains(password) && !loginStatus.text.Contains(password) &&
                    !entryStatus.text.Contains(password) && !worldStatus.text.Contains(loginId) &&
                    !entryStatus.text.Contains(loginId) && string.IsNullOrEmpty(passwordInput.text), Is.True,
                    "Credentials must remain absent from live status UI and the password field must stay cleared.");

                yield return null;
                yield return null;
                var shaderDeadline = Time.realtimeSinceStartup + 10f;
                while (ShaderUtil.anythingCompiling && Time.realtimeSinceStartup < shaderDeadline)
                    yield return null;
                Assert.That(!ShaderUtil.anythingCompiling, Is.True,
                    "Live-entry preview shaders did not finish compiling before capture.");
                yield return null;
                yield return CapturePng("c02-live-entry.png");
                // On PASS, deliberately keep the live C02 scene and authenticated model connected for Recorder capture.
            }
            finally
            {
                if (observedModel != null)
                {
                    observedModel.OnZoneChageReceived -= zoneObserver;
                    observedModel.OnFullReceived -= fullObserver;
                }
                if (passwordInput != null) passwordInput.text = string.Empty;
                Application.runInBackground = originalRunInBackground;
                if (focusedWindow != null) focusedWindow.Focus();
                if (testMouse != null && testMouse.added) InputSystem.RemoveDevice(testMouse);
                testMouse = null;
                ClearAccount(account);
                loginId = null;
                password = null;
                inputDiagnosticResultPath = null;
            }
        }

        private IEnumerator CaptureFixturePreview(C02WorldScreen world, C01LoginScreen login, TMP_Text entryStatus)
        {
            var ownState = new FullStateField
            {
                zoneInternalID = 73,
                profileId = 2,
                profileVersion = 4,
                hp = 88,
                mp = 41,
                maxHp = 100,
                maxMp = 50,
                exp = 1234,
                level = 9,
                dir = 1,
                x = 17.25f,
                y = -6.5f
            };
            try
            {
                SetPrivateProperty(world, "Stage", C02Stage.Entering);
                SetPrivateProperty(world, "SelectedCharacterID", 42UL);
                InvokePrivate(world, "FullReceived", (ushort)1, new[] { ownState });
                InvokePrivate(world, "ZoneReceived", (ushort)5, (ulong)500, 73U, 15f, -5f);
                Assert.That(world.HasOwnFull && world.IsWorldVisible && world.InternalID == 73 && world.ZoneID == 5,
                    Is.True, "Fixture preview requires matching own Full and Zone state.");
                Assert.That(world.ServerPosition == new Vector2(17.25f, -6.5f), Is.True,
                    "Fixture preview must use the fixture server coordinates.");

                // Let the first scene render settle before capturing the labeled fixture state.
                yield return null;
                yield return null;
                var shaderDeadline = Time.realtimeSinceStartup + 10f;
                while (ShaderUtil.anythingCompiling && Time.realtimeSinceStartup < shaderDeadline)
                    yield return null;
                Assert.That(!ShaderUtil.anythingCompiling, Is.True,
                    "Fixture preview shaders did not finish compiling before capture.");
                yield return null;

                var status = GameObject.Find("WorldStatusText").GetComponent<TMP_Text>();
                var originalStatus = status.text;
                try
                {
                    status.text = "FIXTURE PREVIEW — NOT A SERVER ENTRY\n" + originalStatus;
                    yield return CapturePng("c02-fixture-preview.png");
                }
                finally
                {
                    status.text = originalStatus;
                }
            }
            finally
            {
                // The fixture result never sends Enter to the server. Restore the real empty-list screen.
                InvokePrivate(world, "ChangeClient", new object[] { null });
                InvokePrivate(world, "ChangeClient", login.Model);
            }

            Assert.That(!world.IsWorldVisible && !world.HasOwnFull && world.EnterRequestCount == 0, Is.True,
                "The fixture preview must be cleared and no server Enter request may have been issued.");
            Assert.That(entryStatus.text.Contains("Sign in") || entryStatus.text.Contains("No selectable characters"),
                Is.True, "The restored screen must not retain fixture preview state.");
        }

        private static IEnumerator LoadC02Scene()
        {
            var operation = EditorSceneManager.LoadSceneAsyncInPlayMode("Assets/C02/C02.unity",
                new LoadSceneParameters(LoadSceneMode.Single));
            Assert.That(operation != null, Is.True, "C02 scene load could not be started.");
            var deadline = Time.realtimeSinceStartup + 10f;
            while (!operation.isDone && Time.realtimeSinceStartup < deadline) yield return null;
            Assert.That(operation.isDone, Is.True, "C02 scene load timed out.");
            yield return null;
        }

        private IEnumerator TypeText(TMP_InputField input, string value)
        {
            var gameView = FindGameView();
            Assert.That(gameView != null, Is.True, "An open Unity Game view is required for C02 text input.");
            gameView.Focus();
            yield return null;
            var rect = input.transform as RectTransform;
            var position = RectTransformUtility.WorldToScreenPoint(null, rect.position);
            yield return Click(input);
            var eventSystem = UnityEngine.EventSystems.EventSystem.current;
            bool focused = input.isFocused && eventSystem != null && eventSystem.currentSelectedGameObject == input.gameObject;
            if (!focused) AppendInputDiagnostic(input, value.Length, position, "focus");
            Assert.That(focused, Is.True, "Native UI click did not focus the requested input field.");

            for (int i = 0; i < value.Length; i++)
            {
                EditorGUIUtility.QueueGameViewInputEvent(new Event
                {
                    type = EventType.KeyDown,
                    character = value[i],
                    keyCode = KeyCode.None
                });
                var prefix = value.Substring(0, i + 1);
                var deadline = Time.realtimeSinceStartup + 1f;
                while (input.text != prefix && Time.realtimeSinceStartup < deadline) yield return null;
                if (input.text != prefix) AppendInputDiagnostic(input, prefix.Length, position, "text");
                Assert.That(input.text == prefix, Is.True, "Native Game View text event was not acknowledged.");
                EditorGUIUtility.QueueGameViewInputEvent(new Event
                {
                    type = EventType.KeyUp,
                    character = value[i],
                    keyCode = KeyCode.None
                });
                yield return null;
            }
            Assert.That(input.text == value, Is.True, "Native Game View events did not populate the selected input.");
        }

        private void AppendInputDiagnostic(TMP_InputField input, int expectedLength, Vector2 position, string phase)
        {
            var eventSystem = UnityEngine.EventSystems.EventSystem.current;
            var selected = eventSystem == null ? null : eventSystem.currentSelectedGameObject;
            var module = eventSystem == null ? null : eventSystem.currentInputModule as UnityEngine.InputSystem.UI.InputSystemUIInputModule;
            bool pointEnabled = module != null && module.point != null && module.point.action.enabled;
            bool clickEnabled = module != null && module.leftClick != null && module.leftClick.action.enabled;
            var line = $"INPUT_DIAGNOSTIC phase={phase}; target={input.gameObject.name}; actualLength={input.text.Length}; " +
                $"expectedLength={expectedLength}; focused={input.isFocused}; selected={(selected == null ? "none" : selected.name)}; " +
                $"selectedTarget={(selected == input.gameObject)}; pointEnabled={pointEnabled}; clickEnabled={clickEnabled}; " +
                $"nativeMousePresent={Mouse.current != null}; testMousePresent={testMouse != null && testMouse.added}; " +
                $"screen={Screen.width}x{Screen.height}; position=({position.x:F1},{position.y:F1}){Environment.NewLine}";
            File.AppendAllText(inputDiagnosticResultPath, line);
        }

        private IEnumerator Click(Selectable target)
        {
            Assert.That(target != null, Is.True, "The requested C02 UI button was not found.");
            var rect = target.transform as RectTransform;
            Assert.That(rect != null, Is.True, "C02 UI target must have a RectTransform.");
            var position = RectTransformUtility.WorldToScreenPoint(null, rect.position);
            InputSystem.QueueStateEvent(testMouse, new MouseState { position = position });
            yield return NextPlayerFrame();
            InputSystem.QueueStateEvent(testMouse, new MouseState { position = position }.WithButton(MouseButton.Left));
            yield return NextPlayerFrame();
            InputSystem.QueueStateEvent(testMouse, new MouseState { position = position });
            yield return NextPlayerFrame();
            if (!string.IsNullOrEmpty(inputDiagnosticResultPath))
            {
                var selected = UnityEngine.EventSystems.EventSystem.current.currentSelectedGameObject;
                File.AppendAllText(inputDiagnosticResultPath,
                    $"CLICK target={target.name}; frame={Time.frameCount}; selected={(selected == null ? "none" : selected.name)}; mouseAtTarget={Vector2.Distance(testMouse.position.ReadValue(), position) < 1f}\n");
            }
        }

        private static IEnumerator NextPlayerFrame()
        {
            var frame = Time.frameCount;
            var deadline = Time.realtimeSinceStartup + 3f;
            EditorApplication.QueuePlayerLoopUpdate();
            while (Time.frameCount == frame && Time.realtimeSinceStartup < deadline) yield return null;
            Assert.That(Time.frameCount > frame, Is.True, "The queued mouse event requires an actual player frame.");
        }

        private static IEnumerator WaitFor(Func<bool> condition, float timeout, string step)
        {
            var deadline = Time.realtimeSinceStartup + timeout;
            while (!condition() && Time.realtimeSinceStartup < deadline) yield return null;
            Assert.That(condition(), Is.True, "Timed out waiting for " + step + ".");
        }

        private static IEnumerator CapturePng(string fileName)
        {
            var path = Path.GetFullPath(Path.Combine(Application.dataPath, "../Captures/" + fileName));
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            if (File.Exists(path)) File.Delete(path);
            ScreenCapture.CaptureScreenshot(path);
            var deadline = Time.realtimeSinceStartup + 5f;
            while ((!File.Exists(path) || new FileInfo(path).Length == 0) && Time.realtimeSinceStartup < deadline)
                yield return null;
            Assert.That(File.Exists(path) && new FileInfo(path).Length > 0, Is.True,
                "The requested C02 capture was not written.");
        }

        private static T Find<T>(Component root, string path) where T : Component
        {
            var target = root == null ? null : root.transform.Find(path);
            return target == null ? null : target.GetComponent<T>();
        }

        private static TestAccount ReadAccount()
        {
            var type = FindLoadedType("C01.Tests.C01LoginFlowTests+TestAccount");
            var method = type.GetMethod("ReadForCurrentWindowsUser", BindingFlags.Public | BindingFlags.Static);
            return new TestAccount(type, method.Invoke(null, null));
        }

        private static void ClearAccount(TestAccount account)
        {
            if (account != null) account.Type.GetMethod("Clear").Invoke(account.Value, null);
        }

        private static string ReadString(TestAccount account, string name) =>
            (string)account.Type.GetField(name, BindingFlags.Instance | BindingFlags.Public).GetValue(account.Value);

        private static Type FindLoadedType(string fullName)
        {
            foreach (var assembly in AppDomain.CurrentDomain.GetAssemblies())
            {
                var type = assembly.GetType(fullName, false);
                if (type != null) return type;
            }
            throw new InvalidOperationException("C01 test account loader is not loaded.");
        }

        private static void SetPrivateProperty(C02WorldScreen target, string name, object value)
        {
            var property = typeof(C02WorldScreen).GetProperty(name);
            property.GetSetMethod(true).Invoke(target, new[] { value });
        }

        private static void InvokePrivate(C02WorldScreen target, string name, params object[] args) =>
            typeof(C02WorldScreen).GetMethod(name, BindingFlags.Instance | BindingFlags.NonPublic).Invoke(target, args);

        private static EditorWindow FindGameView()
        {
            foreach (var window in Resources.FindObjectsOfTypeAll<EditorWindow>())
                if (window != null && window.GetType().Name == "GameView") return window;
            return null;
        }

        private sealed class TestAccount
        {
            public readonly Type Type;
            public readonly object Value;
            public TestAccount(Type type, object value) { Type = type; Value = value; }
        }
    }

    public static class C02EditorInputRun
    {
        public static void StartEmptyListUi()
        {
            if (!EditorApplication.isPlaying) throw new InvalidOperationException("C02 UI run requires Play mode.");
            var driverType = FindLoadedType("C01.Tests.C01EditorInputRun");
            var start = driverType.GetMethod("Start", new[] { typeof(IEnumerator), typeof(string), typeof(string) });
            var scenario = new C02EmptyListUiTests().EmptyListDisablesEntry_AndCapturesLabeledStates();
            var resultPath = Path.GetFullPath(Path.Combine(Application.dataPath, "../Captures/c02-empty-list-ui.txt"));
            start.Invoke(null, new object[] { scenario, resultPath, "PASS: C02 empty-list UI and labeled fixture preview assertions completed." });
        }

        public static void StartLiveEntryUi(ulong expectedCharacterID)
        {
            if (!EditorApplication.isPlaying) throw new InvalidOperationException("C02 live-entry UI run requires Play mode.");
            if (expectedCharacterID == 0) throw new ArgumentOutOfRangeException(nameof(expectedCharacterID));
            var driverType = FindLoadedType("C01.Tests.C01EditorInputRun");
            var start = driverType.GetMethod("Start", new[] { typeof(IEnumerator), typeof(string), typeof(string) });
            var scenario = new C02EmptyListUiTests().RealCharacterSelectionAndEntry_AndCapture(expectedCharacterID);
            var resultPath = Path.GetFullPath(Path.Combine(Application.dataPath, "../Captures/c02-live-entry-ui.txt"));
            start.Invoke(null, new object[] { scenario, resultPath, "PASS: C02 real character selection and server-confirmed entry assertions completed." });
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
