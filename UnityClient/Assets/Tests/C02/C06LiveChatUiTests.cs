#if UNITY_EDITOR
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text.RegularExpressions;
using ClientCore;
using ClientCore.PacketHelper;
using NUnit.Framework;
using TMPro;
using UnityEditor;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.LowLevel;
using UnityEngine.UI;

namespace C02.Tests
{
    public sealed class C06LiveChatUiTests
    {
        private readonly C02EmptyListUiTests entry = new C02EmptyListUiTests();
        private readonly List<string> received = new List<string>();
        private string id, output;
        private Mouse mouse;
        private Keyboard keyboard;
        private C01.C01LoginScreen login;
        private IViewModelUI model;
        private Action<byte, ulong, string, string> observer;
        private Action<ushort, ActionResultField[]> actionObserver;
        private int ownPhases;

        public static void Start(string runId)
        {
            if (!Regex.IsMatch(runId ?? "", @"^[A-Za-z0-9-]{1,40}$")) throw new ArgumentException("Invalid run ID.");
            if (!EditorApplication.isPlaying) throw new InvalidOperationException("Enter Play first.");
            var test = new C06LiveChatUiTests { id = runId, output = PathFor("c06-live-" + runId + ".txt") };
            if (File.Exists(test.output)) throw new IOException("Earlier C06 evidence already exists.");
            var runner = AppDomain.CurrentDomain.GetAssemblies().Select(a => a.GetType("C01.Tests.C01EditorInputRun", false)).First(t => t != null);
            runner.GetMethod("Start", new[] { typeof(IEnumerator), typeof(string), typeof(string) }).Invoke(null,
                new object[] { test.Run(), test.output, "PASS: C06 native UI validation, typing guard, server own/peer echo and Korean text completed." });
        }

        public static void StartReadability(string runId)
        {
            if (!Regex.IsMatch(runId ?? "", @"^[A-Za-z0-9-]{1,40}$") || !EditorApplication.isPlaying)
                throw new InvalidOperationException("Readability check needs Play and a valid run ID.");
            var test = new C06LiveChatUiTests { id = runId, output = PathFor("c06-live-" + runId + ".txt") };
            if (File.Exists(test.output)) throw new IOException("Earlier readability evidence exists.");
            var runner = AppDomain.CurrentDomain.GetAssemblies().Select(a => a.GetType("C01.Tests.C01EditorInputRun", false)).First(t => t != null);
            runner.GetMethod("Start", new[] { typeof(IEnumerator), typeof(string), typeof(string) }).Invoke(null,
                new object[] { test.Readability(), test.output, "PASS: affected chat readability check; three real server lines fit above the input." });
        }

        private IEnumerator Readability()
        {
            bool background = Application.runInBackground;
            try
            {
                Application.runInBackground = true;
                PreserveEntryEvidence();
                yield return entry.RealCharacterSelectionAndEntry_AndCapture(5011);
                login = GameObject.Find("C01Client").GetComponent<C01.C01LoginScreen>();
                model = login.Model;
                mouse = InputSystem.AddDevice<Mouse>();
                SetField("testMouse", mouse);
                SetField("inputDiagnosticResultPath", output);
                var input = GameObject.Find("ChatInput").GetComponent<TMP_InputField>();
                var send = GameObject.Find("ChatSendButton").GetComponent<Button>();
                var history = GameObject.Find("ChatHistoryText").GetComponent<TMP_Text>();
                observer = (scope, sender, name, text) =>
                {
                    if (scope == 0 && text.Contains(id)) { received.Add(text); Append("SERVER " + name + ": " + text); }
                };
                model.OnChatReceived += observer;
                foreach (string suffix in new[] { "확인 하나", "확인 둘", "확인 셋" })
                {
                    string text = "C06 " + id + " " + suffix;
                    yield return Type(input, text);
                    yield return Click(send);
                    yield return Wait(() => received.Contains(text), 12, "readability server echo");
                }
                yield return Frame();
                Assert.That(received.Count == 3 && received.All(text => history.text.Contains(text)), Is.True,
                    "All three actual server messages must appear in the history.");
                history.ForceMeshUpdate();
                Assert.That(history.isTextOverflowing, Is.False, "Three server chat lines must fit the history.");
                var logCorners = new Vector3[4];
                var inputCorners = new Vector3[4];
                history.rectTransform.GetWorldCorners(logCorners);
                input.GetComponent<RectTransform>().GetWorldCorners(inputCorners);
                Assert.That(logCorners[0].y, Is.GreaterThan(inputCorners[1].y), "History may not overlap the input field.");
                string capture = PathFor("c06-live-" + id + ".png");
                if (File.Exists(capture)) throw new IOException("Earlier readability capture exists.");
                ScreenCapture.CaptureScreenshot(capture);
                yield return Wait(() => File.Exists(capture) && new FileInfo(capture).Length > 0, 5, "readability capture");
                yield return Click(GameObject.Find("DisconnectButton").GetComponent<Button>());
                yield return Wait(() => login.Model == null, 8, "normal readability disconnect");
            }
            finally
            {
                if (model != null && observer != null) model.OnChatReceived -= observer;
                if (login != null && login.Model != null) login.ReleaseClient();
                if (mouse != null && mouse.added) InputSystem.RemoveDevice(mouse);
                Application.runInBackground = background;
            }
        }

        private IEnumerator Run()
        {
            bool background = Application.runInBackground;
            try
            {
                Application.runInBackground = true;
                PreserveEntryEvidence();
                yield return entry.RealCharacterSelectionAndEntry_AndCapture(5011);
                login = GameObject.Find("C01Client").GetComponent<C01.C01LoginScreen>();
                var world = GameObject.Find("C02Client").GetComponent<C02WorldScreen>();
                model = login.Model;
                Assert.That(model != null && world.CanAct, Is.True, "B06 needs the live dedicated account1 alive in the field.");
                var input = GameObject.Find("ChatInput").GetComponent<TMP_InputField>();
                var send = GameObject.Find("ChatSendButton").GetComponent<Button>();
                var history = GameObject.Find("ChatHistoryText").GetComponent<TMP_Text>();
                var status = GameObject.Find("ChatStatusText").GetComponent<TMP_Text>();
                Assert.That(input != null && send != null && history != null && status != null && !history.richText,
                    Is.True, "Installed chat UI must render received text literally.");

                mouse = InputSystem.AddDevice<Mouse>();
                keyboard = InputSystem.AddDevice<Keyboard>();
                SetField("testMouse", mouse);
                SetField("inputDiagnosticResultPath", output);
                observer = (scope, sender, name, text) =>
                {
                    if (scope != 0 || !text.Contains(id)) return;
                    received.Add(text);
                    Append("SERVER chatId=" + sender + "; name=" + name + "; text=" + text);
                };
                model.OnChatReceived += observer;
                actionObserver = (count, results) =>
                {
                    for (int i = 0; i < count; i++)
                        if (results[i].casterType == 0 && results[i].casterId == world.InternalID) ownPhases++;
                };
                model.OnActionResultReceived += actionObserver;
                Vector2 before = world.ServerPosition;
                int mp = world.OwnFull.mp;

                yield return Type(input, new string('x', 129));
                yield return Click(send);
                yield return Wait(() => status.text.Length != 0, 3, "over-limit rejection status");
                Assert.That(input.text.Length, Is.EqualTo(129), "Rejected text must be preserved.");
                Assert.That(received.Count, Is.EqualTo(0), "Invalid input must not create a server chat line.");
                Append("PASS 129-byte native input rejected; text preserved; no local success echo.");

                yield return Clear(input);
                string ownText = "C06 " + id + " 123 <b>literal</b>";
                yield return Type(input, ownText);
                Assert.That(input.isFocused, Is.True);
                foreach (var key in new[] { Key.Digit1, Key.Digit2, Key.Digit3 })
                {
                    InputSystem.QueueStateEvent(keyboard, new KeyboardState(key));
                    yield return Frame();
                    InputSystem.QueueStateEvent(keyboard, new KeyboardState());
                    yield return Frame();
                }
                Assert.That(input.text, Is.EqualTo(ownText), "Physical key-state checks must not rewrite native text.");
                double observationEnd = Time.realtimeSinceStartupAsDouble + 2;
                while (Time.realtimeSinceStartupAsDouble < observationEnd) yield return null;
                Assert.That(world.ServerPosition, Is.EqualTo(before), "Chat field clicks/typing must not move the player.");
                Assert.That(world.OwnFull.mp, Is.EqualTo(mp), "Skill keys while typing must not consume server MP.");
                Assert.That(ownPhases, Is.EqualTo(0), "No own server skill phase may arrive after focused typing.");
                Append("PASS focused native chat input: after 2s, server position/MP unchanged and own action phases=0.");
                yield return Click(send);
                yield return Wait(() => received.Contains(ownText), 12, "own server echo");
                string peerText = "C06 peer " + id;
                yield return Wait(() => received.Contains(peerText), 18, "dedicated peer server response");
                Assert.That(received.Count(x => x == ownText), Is.EqualTo(1), "Own line must come from one server echo.");
                Assert.That(history.text.Contains("<b>literal</b>") && history.text.Contains(peerText), Is.True);

                string korean = "C06 한글 " + id;
                yield return Type(input, korean);
                yield return Click(send);
                yield return Wait(() => received.Contains(korean), 12, "Korean UTF8 server echo");
                Assert.That(history.text.Contains(korean) && !history.richText, Is.True);
                Append("PASS own/peer server echoes and Korean UTF8 message rendered in literal chat history.");
                string capture = PathFor("c06-live-" + id + ".png");
                if (File.Exists(capture)) throw new IOException("Earlier capture already exists.");
                ScreenCapture.CaptureScreenshot(capture);
                yield return Wait(() => File.Exists(capture) && new FileInfo(capture).Length > 0, 5, "chat capture");
                yield return Click(GameObject.Find("DisconnectButton").GetComponent<Button>());
                yield return Wait(() => login.Model == null && !world.IsWorldVisible, 8, "normal UI disconnect");
                Append("PASS normal disconnect and chat panel cleanup.");
            }
            finally
            {
                if (model != null && observer != null) model.OnChatReceived -= observer;
                if (model != null && actionObserver != null) model.OnActionResultReceived -= actionObserver;
                var current = GameObject.Find("C01Client")?.GetComponent<C01.C01LoginScreen>();
                if (current != null && current.Model != null) current.ReleaseClient();
                if (keyboard != null && keyboard.added) InputSystem.RemoveDevice(keyboard);
                if (mouse != null && mouse.added) InputSystem.RemoveDevice(mouse);
                Application.runInBackground = background;
            }
        }

        private IEnumerator Type(TMP_InputField input, string value) => (IEnumerator)Invoke("TypeText", input, value);
        private IEnumerator Click(Selectable value) => (IEnumerator)Invoke("Click", value);
        private object Invoke(string name, params object[] args) => typeof(C02EmptyListUiTests)
            .GetMethod(name, BindingFlags.NonPublic | BindingFlags.Instance).Invoke(entry, args);
        private void SetField(string name, object value) => typeof(C02EmptyListUiTests)
            .GetField(name, BindingFlags.NonPublic | BindingFlags.Instance).SetValue(entry, value);

        private IEnumerator Clear(TMP_InputField input)
        {
            yield return Click(input);
            EditorGUIUtility.QueueGameViewInputEvent(new Event { type = EventType.KeyDown, keyCode = KeyCode.A, modifiers = EventModifiers.Control });
            yield return Frame();
            EditorGUIUtility.QueueGameViewInputEvent(new Event { type = EventType.KeyUp, keyCode = KeyCode.A, modifiers = EventModifiers.Control });
            yield return Frame();
            EditorGUIUtility.QueueGameViewInputEvent(new Event { type = EventType.KeyDown, keyCode = KeyCode.Backspace });
            yield return Frame();
            EditorGUIUtility.QueueGameViewInputEvent(new Event { type = EventType.KeyUp, keyCode = KeyCode.Backspace });
            yield return Wait(() => input.text.Length == 0, 3, "native select-all/backspace");
        }

        private static IEnumerator Frame()
        {
            int frame = Time.frameCount;
            double deadline = EditorApplication.timeSinceStartup + 3;
            EditorApplication.QueuePlayerLoopUpdate();
            while (Time.frameCount == frame && EditorApplication.timeSinceStartup < deadline) yield return null;
            Assert.That(Time.frameCount > frame, Is.True, "Real input Player frame did not advance.");
        }
        private IEnumerator Wait(Func<bool> condition, float seconds, string stage)
        {
            double deadline = Time.realtimeSinceStartupAsDouble + seconds;
            while (!condition() && Time.realtimeSinceStartupAsDouble < deadline) yield return null;
            if (!condition()) Append("FAIL stage=" + stage);
            Assert.That(condition(), Is.True, "C06 timed out: " + stage);
        }
        private void PreserveEntryEvidence()
        {
            foreach (string name in new[] { "c02-live-character-list.png", "c02-live-entry.png", "c02-live-entry-ui.txt" })
            {
                string source = PathFor(name);
                if (File.Exists(source)) File.Copy(source, PathFor("c06-" + id + "-preserved-" + name), false);
            }
        }
        private void Append(string text) => File.AppendAllText(output, text + Environment.NewLine);
        private static string PathFor(string name) => Path.GetFullPath(Path.Combine(Application.dataPath, "../Captures", name));
    }
}
#endif
