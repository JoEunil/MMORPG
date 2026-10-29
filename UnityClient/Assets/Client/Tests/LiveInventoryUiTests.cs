#if UNITY_EDITOR
using System;
using System.Collections;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text.RegularExpressions;
using Mmorpg.Client;
using ClientCore;
using NUnit.Framework;
using TMPro;
using UnityEditor;
using UnityEngine;

namespace Mmorpg.Client.Tests
{
    // Windows Computer Use supplies physical clicks. This helper observes the real client;
    // credentials stay in memory and are entered through GameView text events, never handlers.
    public static class LiveInventoryUiTests
    {
        private static LoginScreen login;
        private static WorldScreen world;
        private static IViewModelUI model;
        private static Action<bool> observer;
        private static string output;
        private static string id;
        private static int responses;
        private static bool lastSuccess;
        private static Vector2 position;

        public static void TypeSelectedField(string runId, bool password)
        {
            ValidateId(runId);
            var evidence = PathFor("c07-" + runId + (password ? "-password.txt" : "-loginid.txt"));
            if (File.Exists(evidence)) throw new IOException("Earlier text-input evidence exists.");
            var runner = AppDomain.CurrentDomain.GetAssemblies().Select(a => a.GetType("Mmorpg.Client.Tests.LoginEditorInputRun", false)).First(t => t != null);
            runner.GetMethod("Start", new[] { typeof(IEnumerator), typeof(string), typeof(string) }).Invoke(null,
                new object[] { TypeSelected(password), evidence, "PASS native text input acknowledged; value omitted." });
        }

        private static IEnumerator TypeSelected(bool password)
        {
            var field = GameObject.Find(password ? "PasswordInput" : "LoginIdInput").GetComponent<TMP_InputField>();
            Assert.That(field.isFocused && UnityEngine.EventSystems.EventSystem.current.currentSelectedGameObject == field.gameObject, Is.True,
                "The physical mouse must focus the intended text field first.");
            var fixtureType = typeof(EmptyListUiTests);
            var account = fixtureType.GetMethod("ReadAccount", BindingFlags.Static | BindingFlags.NonPublic).Invoke(null, null);
            var value = (string)fixtureType.GetMethod("ReadString", BindingFlags.Static | BindingFlags.NonPublic)
                .Invoke(null, new object[] { account, password ? "Password" : "LoginId" });
            bool complete = false;
            try
            {
                Assert.That(field.text.Length, Is.Zero, "Start with an empty field.");
                for (int i = 0; i < value.Length; i++)
                {
                    EditorGUIUtility.QueueGameViewInputEvent(new Event { type = EventType.KeyDown, keyCode = KeyCode.None, character = value[i] });
                    double deadline = Time.realtimeSinceStartupAsDouble + 2;
                    while (field.text.Length < i + 1 && Time.realtimeSinceStartupAsDouble < deadline) yield return null;
                    Assert.That(field.text.Length == i + 1, Is.True, "Native text event was not acknowledged.");
                    EditorGUIUtility.QueueGameViewInputEvent(new Event { type = EventType.KeyUp, keyCode = KeyCode.None, character = value[i] });
                    yield return null;
                }
                Assert.That(field.text == value, Is.True, "Typed text did not match the dedicated account.");
                complete = true;
            }
            finally
            {
                fixtureType.GetMethod("ClearAccount", BindingFlags.Static | BindingFlags.NonPublic).Invoke(null, new[] { account });
                value = null;
                if (!complete && password && field != null) field.SetTextWithoutNotify(string.Empty);
            }
        }

        public static void Begin(string runId)
        {
            ValidateId(runId);
            if (model != null) throw new InvalidOperationException("A C07 observation is already active.");
            login = GameObject.Find("C01Client").GetComponent<LoginScreen>();
            world = GameObject.Find("C02Client").GetComponent<WorldScreen>();
            Assert.That(login.Model != null && world.Stage == WorldStage.InWorld && world.SelectedCharacterID == 5011 && world.HasOwnFull, Is.True);
            id = runId;
            output = PathFor("c07-physical-" + id + ".txt");
            if (File.Exists(output)) throw new IOException("Earlier observation evidence exists.");
            File.WriteAllText(output, "Actual Windows mouse input / passive server observation.\nDedicated inventory rows are missing; status0 is expected, not empty success.\n");
            position = world.ServerPosition;
            responses = 0;
            model = login.Model;
            observer = success => { responses++; lastSuccess = success; Append("SERVER response=" + responses + "; status=" + (success ? 1 : 0) + "; rows=" + login.Data.GetInventory().Count); };
            model.OnInventoryReceived += observer;
        }

        public static void VerifyFailure(int count)
        {
            Assert.That(model != null && login.Model == model && responses == count && !lastSuccess, Is.True, "Expected the actual status0 server response.");
            var canvas = UnityEngine.Object.FindFirstObjectByType<Canvas>();
            var panel = canvas.transform.Find("C07InventoryPanel");
            var status = panel.Find("InventoryParchment/InventoryStatus").GetComponent<TMP_Text>();
            var rows = panel.Find("InventoryParchment/InventoryScroll/InventoryViewport/InventoryRows").GetComponent<TMP_Text>();
            Assert.That(panel.gameObject.activeSelf && status.text.Contains("failed") && rows.text.Length == 0 && login.Data.GetInventory().Count == 0, Is.True);
            Assert.That(world.ServerPosition, Is.EqualTo(position), "UI clicks may not move the character.");
            Append("PASS response " + count + ": actual failure displayed, stale rows empty, server position unchanged.");
        }

        public static void VerifyClosed()
        {
            var canvas = UnityEngine.Object.FindFirstObjectByType<Canvas>();
            Assert.That(canvas.transform.Find("C07InventoryPanel").gameObject.activeSelf, Is.False);
            Assert.That(world.ServerPosition, Is.EqualTo(position));
            Append("PASS physical Close click hid the panel; server position unchanged.");
        }

        public static string Capture()
        {
            var path = PathFor("c07-physical-" + id + ".png");
            if (File.Exists(path)) throw new IOException("Earlier capture exists.");
            ScreenCapture.CaptureScreenshot(path);
            return path;
        }

        public static void Finish()
        {
            try
            {
                Assert.That(login.Model == null && !world.IsWorldVisible && responses == 3, Is.True, "Physical disconnect and all three responses must complete.");
                Append("PASS actual login/selection/entry, BAG/Refresh/Close/reopen, three server failures, normal UI disconnect. Successful inventory contents remain unverified.");
            }
            finally { if (model != null && observer != null) model.OnInventoryReceived -= observer; model = null; observer = null; }
        }

        private static void ValidateId(string value)
        { if (!EditorApplication.isPlaying || !Regex.IsMatch(value ?? "", @"^[A-Za-z0-9-]{1,40}$")) throw new InvalidOperationException("Play and a valid run ID are required."); }
        private static string PathFor(string name) => Path.GetFullPath(Path.Combine(Application.dataPath, "../Captures", name));
        private static void Append(string text) => File.AppendAllText(output, text + Environment.NewLine);
    }
}
#endif
