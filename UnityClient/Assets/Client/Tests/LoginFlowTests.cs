using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Text;
using TMPro;
#if UNITY_EDITOR
using UnityEditor;
using UnityEditor.SceneManagement;
#endif
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.Controls;
using UnityEngine.InputSystem.LowLevel;
using UnityEngine.SceneManagement;
using UnityEngine.TestTools;
using NUnit.Framework;
using Mmorpg.Client;

namespace Mmorpg.Client.Tests
{
    public sealed class LoginFlowTests
    {
        private Mouse testMouse;
        private const string ScenePath = "Assets/Client/Scenes/Login.unity";
        private const string SceneName = "Login";
        private const string ScreenRoot = "C01Client";
        private const string LoginIdPath = "Canvas/C01Panel/SignIn/LoginIdInput";
        private const string PasswordPath = "Canvas/C01Panel/SignIn/PasswordInput";
        private const string LoginButtonPath = "Canvas/C01Panel/SignIn/LoginButton";
        private const string StatusPath = "Canvas/C01Panel/Results/StatusText";
        private const string CharacterListPath = "Canvas/C01Panel/Results/CharacterListText";

        [Test]
        public void PasswordSubmitUsesExistingValidationAndBusyGuardWithoutDuplicatingListeners()
        {
            var root = new GameObject("C01 password submit fixture");
            root.SetActive(false);
            try
            {
                var screen = root.AddComponent<LoginScreen>();
                var idInput = Add<TMP_InputField>("login id");
                var passwordInput = Add<TMP_InputField>("password");
                var button = Add<UnityEngine.UI.Button>("login button");
                var status = Add<TextMeshProUGUI>("status");
                Set("loginIdInput", idInput);
                Set("passwordInput", passwordInput);
                Set("loginButton", button);
                Set("statusText", status);
                Set("characterListText", Add<TextMeshProUGUI>("characters"));
                root.SetActive(true);

                Assert.That(SubmitListenerCount(), Is.EqualTo(1));
                passwordInput.onSubmit.Invoke("event argument is not a password source");
                Assert.That(status.text, Is.EqualTo("Enter your login ID and password."));
                Assert.That(screen.Model, Is.Null, "Empty input must not initialize a client.");
                status.text = "busy status retained";
                screen.SetBusy(true);
                passwordInput.onSubmit.Invoke(string.Empty);
                button.onClick.Invoke();
                Assert.That(screen.IsBusy, Is.True);
                Assert.That(status.text, Is.EqualTo("busy status retained"));
                Assert.That(screen.Model, Is.Null);

                root.SetActive(false);
                Assert.That(SubmitListenerCount(), Is.Zero);
                status.text = "disabled status retained";
                passwordInput.onSubmit.Invoke(string.Empty);
                Assert.That(status.text, Is.EqualTo("disabled status retained"));
                root.SetActive(true);
                Assert.That(SubmitListenerCount(), Is.EqualTo(1), "Re-enabling must restore exactly one submit listener.");
                passwordInput.onSubmit.Invoke(string.Empty);
                Assert.That(status.text, Is.EqualTo("Enter your login ID and password."));
                Assert.That(screen.Model, Is.Null);

                T Add<T>(string name) where T : Component
                {
                    var child = new GameObject(name, typeof(RectTransform));
                    child.transform.SetParent(root.transform, false);
                    return child.AddComponent<T>();
                }

                void Set(string name, object value) => typeof(LoginScreen)
                    .GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).SetValue(screen, value);

                int SubmitListenerCount() => ((IList)typeof(UnityEngine.Events.UnityEventBase)
                    .GetMethod("PrepareInvoke", BindingFlags.Instance | BindingFlags.NonPublic)
                    .Invoke(passwordInput.onSubmit, null)).Count;
            }
            finally
            {
                UnityEngine.Object.DestroyImmediate(root);
            }
        }

        [UnityTest, Timeout(120000)]
        public IEnumerator RealUiInput_FailedLoginRetry_AndSceneReload_DoesNotDuplicateListEvents()
        {
            var account = TestAccount.ReadForCurrentWindowsUser();
            var observedLogs = new List<string>();
#if UNITY_EDITOR
            var originalFocusedWindow = EditorWindow.focusedWindow;
#endif
            var originalRunInBackground = Application.runInBackground;
            Application.LogCallback logCallback = (condition, stackTrace, type) => observedLogs.Add(condition);
            Application.logMessageReceived += logCallback;

            try
            {
                Application.runInBackground = true;
                testMouse = InputSystem.AddDevice<Mouse>();
                // Rebuild UI actions with the test device while retaining the native text runtime.
                yield return LoadLoginScene();
                var screen = FindScreen();

                Assert.That(screen, Is.Not.Null, "C01LoginScreen was not present in the C01 scene.");
                var idInput = FindComponent<TMP_InputField>(screen, LoginIdPath);
                var passwordInput = FindComponent<TMP_InputField>(screen, PasswordPath);
                var statusText = FindComponent<TMP_Text>(screen, StatusPath);
                var characterListText = FindComponent<TMP_Text>(screen, CharacterListPath);
                var loginButton = FindComponent<UnityEngine.UI.Button>(screen, LoginButtonPath);
                Assert.That(idInput, Is.Not.Null);
                Assert.That(passwordInput, Is.Not.Null);
                Assert.That(statusText, Is.Not.Null);
                Assert.That(characterListText, Is.Not.Null);
                Assert.That(loginButton, Is.Not.Null);

                yield return TypeText(idInput, account.LoginId);
                yield return TypeText(passwordInput, account.Password + "-invalid");
                yield return Click(loginButton);
                yield return WaitFor(screen, statusText, account, "invalid-login failure display",
                    () => !screen.IsBusy && statusText.text.Contains("Retry when ready."), 20f);

                Assert.That(screen.Authenticated, Is.False, "Invalid credentials must remain unauthenticated.");
                Assert.That(screen.ListResponseCount, Is.Zero, "An HTTP login failure must not produce a TCP list response.");

                yield return TypeText(passwordInput, account.Password);
                yield return Click(loginButton);
                yield return WaitFor(screen, statusText, account, "valid-login character-list response",
                    () => screen.ListResponseCount > 0, 20f);
                yield return WaitFor(screen, statusText, account, "valid-login completion",
                    () => !screen.IsBusy, 16f);

                Assert.That(screen.Authenticated, Is.True, "The valid retry must complete TCP authentication.");
                Assert.That(screen.ListResponseCount, Is.EqualTo(1), "One login attempt must deliver exactly one character-list response.");
                Assert.That(statusText.text.Contains("status=0") && statusText.text.Contains("count=0"), Is.True,
                    "The current test account has no characters; the server status=0 response must remain a failure.");
                Assert.That(characterListText.text.Contains("No character rows returned."), Is.True,
                    "The character panel must explain why no rows are displayed.");
                Assert.That(characterListText.text.Contains("does not distinguish an empty list from a failed query"), Is.True,
                    "The character panel must preserve the server's empty-versus-failure ambiguity.");

                AssertNoSecretDisclosure(account, passwordInput, statusText, characterListText, observedLogs);

                yield return ReloadLoginScene();
                screen = FindScreen();
                Assert.That(screen, Is.Not.Null, "C01 scene did not reload.");
                idInput = FindComponent<TMP_InputField>(screen, LoginIdPath);
                passwordInput = FindComponent<TMP_InputField>(screen, PasswordPath);
                statusText = FindComponent<TMP_Text>(screen, StatusPath);
                characterListText = FindComponent<TMP_Text>(screen, CharacterListPath);
                loginButton = FindComponent<UnityEngine.UI.Button>(screen, LoginButtonPath);

                yield return TypeText(idInput, account.LoginId);
                yield return TypeText(passwordInput, account.Password);
                yield return Click(loginButton);
                yield return WaitFor(screen, statusText, account, "reloaded-scene character-list response",
                    () => screen.ListResponseCount > 0, 20f);
                yield return WaitFor(screen, statusText, account, "reloaded-scene login completion",
                    () => !screen.IsBusy, 16f);

                Assert.That(screen.Authenticated, Is.True);
                Assert.That(screen.ListResponseCount, Is.EqualTo(1),
                    "Reloaded scene must subscribe once and receive one list response.");
                Assert.That(statusText.text.Contains("status=0") && statusText.text.Contains("count=0"), Is.True,
                    "Reloaded scene must preserve the server's status=0/count=0 failure result.");
                Assert.That(characterListText.text.Contains("No character rows returned."), Is.True,
                    "Reloaded scene must explain why no rows are displayed.");
                Assert.That(characterListText.text.Contains("does not distinguish an empty list from a failed query"), Is.True,
                    "Reloaded scene must preserve the server's empty-versus-failure ambiguity.");
                AssertNoSecretDisclosure(account, passwordInput, statusText, characterListText, observedLogs);
            }
            finally
            {
                Application.runInBackground = originalRunInBackground;
#if UNITY_EDITOR
                if (originalFocusedWindow != null) originalFocusedWindow.Focus();
#endif
                Application.logMessageReceived -= logCallback;
                if (testMouse != null && testMouse.added) InputSystem.RemoveDevice(testMouse);
                testMouse = null;
                account.Clear();
                observedLogs.Clear();
            }
        }

        private IEnumerator LoadLoginScene()
        {
#if UNITY_EDITOR
            var operation = EditorSceneManager.LoadSceneAsyncInPlayMode(ScenePath,
                new LoadSceneParameters(LoadSceneMode.Single));
#else
            var operation = SceneManager.LoadSceneAsync(SceneName, LoadSceneMode.Single);
#endif
            Assert.That(operation, Is.Not.Null, "C01 scene load could not be started.");
            var deadline = Time.realtimeSinceStartup + 10f;
            while (!operation.isDone && Time.realtimeSinceStartup < deadline)
                yield return null;
            Assert.That(operation.isDone, Is.True, "C01 scene load timed out.");
            yield return null;
        }

        private IEnumerator ReloadLoginScene()
        {
            yield return LoadLoginScene();
            yield return null;
        }

        private IEnumerator TypeText(TMP_InputField input, string value)
        {
            Assert.That(input, Is.Not.Null);
#if UNITY_EDITOR
            var gameView = FindGameView();
            Assert.That(gameView != null, Is.True, "An open Unity Game view is required for C01 text input.");
            gameView.Focus();
            yield return null;
#endif
            yield return Click(input);
            var eventSystem = UnityEngine.EventSystems.EventSystem.current;
            var module = eventSystem == null ? null : eventSystem.currentInputModule as UnityEngine.InputSystem.UI.InputSystemUIInputModule;
            Assert.That(input.isFocused && eventSystem != null && eventSystem.currentSelectedGameObject == input.gameObject,
                Is.True, $"UI click did not focus target={input.gameObject.name}; focused={input.isFocused}; " +
                $"selected={(eventSystem == null || eventSystem.currentSelectedGameObject == null ? "none" : eventSystem.currentSelectedGameObject.name)}; " +
                $"pointEnabled={module != null && module.point != null && module.point.action.enabled}; " +
                $"clickEnabled={module != null && module.leftClick != null && module.leftClick.action.enabled}; mousePresent={Mouse.current != null}.");
#if UNITY_EDITOR
            for (var index = 0; index < value.Length; index++)
            {
                EditorGUIUtility.QueueGameViewInputEvent(new Event
                {
                    type = EventType.KeyDown,
                    character = value[index],
                    keyCode = KeyCode.None
                });
                var expectedPrefix = value.Substring(0, index + 1);
                var deadline = Time.realtimeSinceStartup + 1f;
                while (input.text != expectedPrefix && Time.realtimeSinceStartup < deadline)
                    yield return null;
                Assert.That(input.text == expectedPrefix, Is.True,
                    $"Native text event was not acknowledged; focused={input.isFocused}; " +
                    $"selected={eventSystem != null && eventSystem.currentSelectedGameObject == input.gameObject}.");
                EditorGUIUtility.QueueGameViewInputEvent(new Event
                {
                    type = EventType.KeyUp,
                    character = value[index],
                    keyCode = KeyCode.None
                });
                yield return null;
            }
#else
            Assert.Fail("C01 text input automation requires the Unity Editor Game view.");
#endif
            Assert.That(input.text == value, Is.True,
                "Virtual keyboard text events did not populate the selected UI input field.");
        }

#if UNITY_EDITOR
        private static EditorWindow FindGameView()
        {
            foreach (var window in Resources.FindObjectsOfTypeAll<EditorWindow>())
            {
                if (window != null && window.GetType().Name == "GameView")
                    return window;
            }
            return null;
        }
#endif

        private IEnumerator Click(UnityEngine.UI.Selectable selectable)
        {
            Assert.That(selectable, Is.Not.Null);
            var rect = selectable.transform as RectTransform;
            Assert.That(rect, Is.Not.Null, "UI target must have a RectTransform.");
            var position = RectTransformUtility.WorldToScreenPoint(null, rect.position);
            InputSystem.QueueStateEvent(testMouse, new MouseState { position = position });
            yield return null;
            InputSystem.QueueStateEvent(testMouse, new MouseState { position = position }.WithButton(MouseButton.Left));
            yield return null;
            InputSystem.QueueStateEvent(testMouse, new MouseState { position = position });
            yield return null;
        }

        private IEnumerator WaitFor(LoginScreen screen, TMP_Text statusText, TestAccount account,
            string stage, Func<bool> condition, float seconds)
        {
            var deadline = Time.realtimeSinceStartup + seconds;
            while (!condition() && Time.realtimeSinceStartup < deadline)
                yield return null;
            var diagnostics = $"stage={stage}; busy={screen.IsBusy}; authenticated={screen.Authenticated}; " +
                              $"listResponses={screen.ListResponseCount}; status={SanitizeStatus(statusText.text, account)}";
            Assert.That(condition(), Is.True, "Timed out waiting for the expected C01 UI state. " + diagnostics);
        }

        private static string SanitizeStatus(string status, TestAccount account)
        {
            var safe = status ?? string.Empty;
            if (!string.IsNullOrEmpty(account.LoginId)) safe = safe.Replace(account.LoginId, "[redacted]");
            if (!string.IsNullOrEmpty(account.Password)) safe = safe.Replace(account.Password, "[redacted]");
            return System.Text.RegularExpressions.Regex.Replace(safe,
                @"eyJ[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+|[0-9a-fA-F]{8}(?:-[0-9a-fA-F]{4}){3}-[0-9a-fA-F]{12}",
                "[redacted-token]");
        }

        private static T FindComponent<T>(LoginScreen screen, string relativePath) where T : Component
        {
            var target = screen.transform.Find(relativePath);
            return target == null ? null : target.GetComponent<T>();
        }

        private static LoginScreen FindScreen()
        {
            var root = GameObject.Find(ScreenRoot);
            return root == null ? UnityEngine.Object.FindObjectOfType<LoginScreen>(true) : root.GetComponent<LoginScreen>();
        }

        private static void AssertNoSecretDisclosure(TestAccount account, TMP_InputField passwordInput,
            TMP_Text statusText, TMP_Text characterListText, List<string> observedLogs)
        {
            Assert.That(string.IsNullOrEmpty(passwordInput.text), Is.True,
                "Password input should be cleared after submission.");
            var visibleText = statusText.text + "\n" + characterListText.text;
            Assert.That(!visibleText.Contains(account.Password), Is.True, "Password must not appear in the UI.");
            Assert.That(observedLogs.Exists(message => message.Contains(account.Password)), Is.False,
                "Password must not appear in Unity logs.");
            Assert.That(observedLogs.Exists(message => System.Text.RegularExpressions.Regex.IsMatch(message,
                    @"eyJ[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+")), Is.False,
                "JWT-like credentials must not appear in Unity logs.");
        }

        [Serializable]
        private sealed class TestAccount
        {
            public string LoginId;
            public string Password;

            public static TestAccount ReadForCurrentWindowsUser()
            {
                var requestedLoginId = Environment.GetEnvironmentVariable("C01_TEST_LOGIN_ID");
                if (string.IsNullOrEmpty(requestedLoginId)) requestedLoginId = "unity_ai_01";
                if (requestedLoginId != "unity_ai_01" && requestedLoginId != "unity_ai_02")
                    throw new InvalidOperationException("C01_TEST_LOGIN_ID must select one of the two approved test accounts.");
                var path = Environment.GetEnvironmentVariable("C01_TEST_ACCOUNT_FILE");
                if (string.IsNullOrWhiteSpace(path))
                {
                    path = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                        "UnityCoordinator", "test-accounts.xml");
                    if (!File.Exists(path))
                        path = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
                            "AppData", "Local", "UnityCoordinator", "test-accounts.xml");
                }

                if (!File.Exists(path))
                    throw new InvalidOperationException("Encrypted C01 test account file was not found for this Windows user.");

                var escapedPath = path.Replace("'", "''");
                var script = "$ErrorActionPreference='Stop'; $rows=@(Import-Clixml -LiteralPath '" + escapedPath + "'); " +
                    "$requested='" + requestedLoginId + "'; $row=$rows | Where-Object { $_.LoginId -eq $requested } | Select-Object -First 1; " +
                    "if($null -eq $row){exit 3}; $b=[Runtime.InteropServices.Marshal]::SecureStringToBSTR($row.Password); " +
                    "try{$pwd=[Runtime.InteropServices.Marshal]::PtrToStringBSTR($b); " +
                    "$payload=[pscustomobject]@{LoginId=$row.LoginId;Password=$pwd}; " +
                    "[Console]::Out.Write(($payload|ConvertTo-Json -Compress))} " +
                    "finally{[Runtime.InteropServices.Marshal]::ZeroFreeBSTR($b);$pwd=$null;$payload=$null}";
                var info = new ProcessStartInfo
                {
                    FileName = "powershell.exe",
                    Arguments = "-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand " +
                        Convert.ToBase64String(Encoding.Unicode.GetBytes(script)),
                    UseShellExecute = false,
                    CreateNoWindow = true,
                    WindowStyle = ProcessWindowStyle.Hidden,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true
                };

                using (var process = Process.Start(info))
                {
                    if (process == null || !process.WaitForExit(15000))
                        throw new InvalidOperationException("Encrypted C01 test account could not be read.");
                    var json = process.StandardOutput.ReadToEnd();
                    process.StandardError.ReadToEnd();
                    if (process.ExitCode != 0)
                        throw new InvalidOperationException("Encrypted C01 test account could not be read.");
                    var account = JsonUtility.FromJson<TestAccount>(json);
                    if (account == null || account.LoginId != requestedLoginId || string.IsNullOrEmpty(account.Password))
                        throw new InvalidOperationException("Encrypted C01 test account is incomplete.");
                    return account;
                }
            }

            public void Clear()
            {
                LoginId = null;
                Password = null;
            }
        }
    }
}
