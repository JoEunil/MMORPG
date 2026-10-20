using System;
using System.Collections.Generic;
using System.Reflection;
using System.Threading.Tasks;
using C01;
using ClientCore;
using ClientCore.PacketHelper;
using ClientCore.Services;
using NUnit.Framework;
using TMPro;
using UnityEngine;
using UnityEngine.UI;

namespace C02.Tests
{
    public sealed class C06ChatScopeTests
    {
        [Test]
        public void GlobalZoneAndMissingWhisperTargetUseOnlyTheirValidScopes()
        {
            using (var fixture = new ChatFixture())
            {
                fixture.SendAs(0, "global line");
                fixture.SendAs(1, "zone line");
                Assert.That(fixture.Network.ChatCalls.Count, Is.EqualTo(2));
                Assert.That(fixture.Network.ChatCalls[0], Is.EqualTo(( "global line", (byte)0, 0UL )));
                Assert.That(fixture.Network.ChatCalls[1], Is.EqualTo(( "zone line", (byte)1, 0UL )));

                fixture.ScopeButtons[2].onClick.Invoke();
                fixture.Input.SetTextWithoutNotify("must not send");
                fixture.SendButton.onClick.Invoke();
                Assert.That(fixture.Network.ChatCalls.Count, Is.EqualTo(2));
                Assert.That(fixture.SendStatus.text, Does.Contain("실제 대상을"));
                Assert.That(fixture.SendButton.interactable, Is.False);
            }
        }

        [Test]
        public void WhisperTargetsComeFromRecentSendersAndOldSessionCallbacksAreIgnored()
        {
            using (var fixture = new ChatFixture())
            {
                var zoneHandler = (Action<ushort, ulong, uint, float, float>)Get(fixture.Chat, "zoneHandler");
                zoneHandler(1, 555, 123, 10, 10);
                fixture.Receive(0, 555, "own echo");
                fixture.Receive(2, 600, "first whisper");
                fixture.Receive(1, 601, "second sender");
                Assert.That(fixture.ChatLog.text, Does.Contain("[Global]").And.Contain("[Whisper]").And.Contain("[Zone]"));
                Assert.That(fixture.WhisperLabel.text, Does.Contain("#600"), "New traffic cannot change the chosen recipient.");
                Assert.That(fixture.WhisperLabel.text, Does.Not.Contain("#555"), "The own server chat ID must not become a reply target.");

                fixture.ScopeButtons[2].onClick.Invoke();
                fixture.SendAs(2, "reply selected");
                fixture.Receive(2, 555, "reply selected");
                fixture.WhisperTargetButton.onClick.Invoke();
                fixture.SendAs(2, "reply next");
                fixture.Receive(2, 555, "reply next");
                Assert.That(fixture.Network.ChatCalls[0], Is.EqualTo(( "reply selected", (byte)2, 600UL )));
                Assert.That(fixture.Network.ChatCalls[1], Is.EqualTo(( "reply next", (byte)2, 601UL )));
                for (ulong sender = 700; sender < 709; sender++) fixture.Receive(0, sender, "new incoming traffic");
                Assert.That(fixture.ChatScroll.content.rect.height, Is.GreaterThan(fixture.ChatScroll.viewport.rect.height));
                Assert.That(fixture.ChatScroll.verticalNormalizedPosition, Is.EqualTo(0).Within(.001),
                    "New messages must stay visible after the history exceeds the viewport.");
                fixture.SendAs(2, "still the chosen recipient");
                Assert.That(fixture.Network.ChatCalls[2], Is.EqualTo(( "still the chosen recipient", (byte)2, 601UL )),
                    "The bounded recent list must not evict and redirect the current recipient.");

                Action<byte, ulong, string, string> oldHandler =
                    (Action<byte, ulong, string, string>)Get(fixture.Chat, "chatHandler");
                fixture.Login.ReleaseClient();
                string clearedLog = fixture.ChatLog.text;
                oldHandler(2, 602, "late sender", "stale event");
                Assert.That(fixture.ChatLog.text, Is.EqualTo(clearedLog));
                Assert.That(fixture.WhisperLabel.text, Does.Not.Contain("#601"));
                Assert.That(fixture.SendButton.interactable, Is.False);
            }
        }

        [Test]
        public void UnconfirmedWhisperPreservesDraftAndInvalidatesOnlyItsRecipient()
        {
            using (var fixture = new ChatFixture())
            {
                ((Action<ushort, ulong, uint, float, float>)Get(fixture.Chat, "zoneHandler"))(1, 555, 123, 10, 10);
                fixture.Receive(0, 600, "target");
                fixture.SendAs(2, "  keep this draft  ");
                Assert.That(fixture.Input.text, Is.EqualTo("  keep this draft  "));
                Assert.That(fixture.Input.interactable, Is.False);
                fixture.SendButton.onClick.Invoke();
                Assert.That(fixture.Network.ChatCalls.Count, Is.EqualTo(1));
                fixture.Receive(2, 601, "keep this draft");
                fixture.Receive(0, 555, "keep this draft");
                Assert.That(fixture.SendStatus.text, Does.Contain("확인 중"));
                Set(fixture.Chat, "whisperDeadline", Time.unscaledTime - 1);
                Invoke(fixture.Chat, "Update");
                Assert.That(fixture.Input.text, Is.EqualTo("  keep this draft  "));
                Assert.That(fixture.Input.interactable, Is.True);
                Assert.That(fixture.SendStatus.text, Does.Contain("초과"));
                Assert.That(fixture.SendButton.interactable, Is.False);
                Assert.That((List<KeyValuePair<ulong, string>>)Get(fixture.Chat, "whisperRecipients"),
                    Has.None.Matches<KeyValuePair<ulong, string>>(recipient => recipient.Key == 600));
                fixture.WhisperTargetButton.onClick.Invoke();
                fixture.SendButton.onClick.Invoke();
                fixture.Receive(2, 555, "keep this draft");
                Assert.That(fixture.Input.text, Is.Empty);
                Assert.That(fixture.SendStatus.text, Does.Contain("확인됨"));
                fixture.SendAs(2, "logout pending");
                fixture.Login.ReleaseClient();
                Assert.That(Get(fixture.Chat, "pendingWhisper"), Is.Null);
                Assert.That(fixture.Input.text, Is.Empty);
                Assert.That(fixture.SendButton.interactable, Is.False);
            }
        }

        private sealed class ChatFixture : IDisposable
        {
            private readonly GameObject root;
            private readonly Initializer initializer;
            public readonly RecordingNetwork Network = new RecordingNetwork();
            public readonly QueuedDispatcher Dispatcher = new QueuedDispatcher();
            public readonly C01LoginScreen Login;
            public readonly C02WorldScreen World;
            public readonly C06ChatScreen Chat;
            public readonly ViewModel Model;
            public readonly Button[] ScopeButtons;
            public readonly Button WhisperTargetButton;
            public readonly TMP_Text WhisperLabel;
            public readonly TMP_Text SendStatus;
            public readonly TMP_Text ChatLog;
            public readonly ScrollRect ChatScroll;
            public readonly TMP_InputField Input;
            public readonly Button SendButton;

            public ChatFixture()
            {
                var dataType = typeof(ViewModel).Assembly.GetType("ClientCore.ViewData", true);
                var data = Activator.CreateInstance(dataType, true);
                Model = (ViewModel)Activator.CreateInstance(typeof(ViewModel), BindingFlags.Instance | BindingFlags.NonPublic,
                    null, new object[] { Network, (IViewDataModel)data, new NullLogger(), Dispatcher }, null);
                C08TestSession.Ready(Model, Dispatcher.Drain);
                initializer = new Initializer();
                Set(initializer, "_viewModel", Model);
                Set(initializer, "_data", data);

                root = new GameObject("C06 scope fixture", typeof(RectTransform), typeof(Canvas));
                root.SetActive(false);
                Login = root.AddComponent<C01LoginScreen>();
                Set(Login, "client", initializer);
                Set(Login, "loginIdInput", InputField("login id"));
                Set(Login, "passwordInput", InputField("password"));
                Set(Login, "loginButton", ButtonObject("login"));
                Set(Login, "statusText", Text("login status"));
                Set(Login, "characterListText", Text("character list"));

                World = root.AddComponent<C02WorldScreen>();
                Set(World, "loginScreen", Login);
                Set(World, "characterChoices", Array.Empty<Button>());
                Set(World, "enterButton", ButtonObject("enter"));
                Set(World, "disconnectButton", ButtonObject("disconnect"));
                Set(World, "entryStatusText", Text("entry status"));
                Set(World, "worldStatusText", Text("world status"));
                Set(World, "selectionPanel", Child("selection panel"));
                Set(World, "background", Child("background"));
                Set(World, "worldRoot", Child("world root"));
                Set(World, "worldHud", Child("world hud"));
                Set(World, "ownCharacter", Child("own character").transform);
                Set(World, "fieldPlane", Child("field plane").transform);
                Set(World, "quarterViewCamera", Child("camera").transform);

                Chat = root.AddComponent<C06ChatScreen>();
                Set(Chat, "loginScreen", Login);
                Set(Chat, "worldScreen", World);
                Set(Chat, "chatPanel", Child("chat panel"));
                ChatLog = Text("chat log");
                ChatLog.fontSize = 13;
                ChatLog.overflowMode = TextOverflowModes.Overflow;
                var history = Child("history viewport");
                var viewport = history.GetComponent<RectTransform>();
                viewport.sizeDelta = new Vector2(460, 40);
                ChatLog.transform.SetParent(history.transform, false);
                ChatLog.rectTransform.anchorMin = new Vector2(0, 1);
                ChatLog.rectTransform.anchorMax = Vector2.one;
                ChatLog.rectTransform.pivot = new Vector2(.5f, 1);
                ChatLog.rectTransform.anchoredPosition = Vector2.zero;
                ChatLog.rectTransform.sizeDelta = Vector2.zero;
                ChatLog.gameObject.AddComponent<ContentSizeFitter>().verticalFit = ContentSizeFitter.FitMode.PreferredSize;
                ChatScroll = history.AddComponent<ScrollRect>();
                ChatScroll.viewport = viewport;
                ChatScroll.content = ChatLog.rectTransform;
                ChatScroll.horizontal = false;
                Set(Chat, "chatScroll", ChatScroll);
                SendStatus = Text("send status");
                Set(Chat, "chatLog", ChatLog);
                Set(Chat, "sendStatus", SendStatus);
                Input = InputField("message input");
                Set(Chat, "messageInput", Input);
                SendButton = ButtonObject("send");
                Set(Chat, "sendButton", SendButton);
                ScopeButtons = new[] { ButtonObject("global"), ButtonObject("zone"), ButtonObject("whisper") };
                Set(Chat, "scopeButtons", ScopeButtons);
                WhisperTargetButton = ButtonObject("whisper target");
                WhisperLabel = Text("whisper target label");
                WhisperLabel.transform.SetParent(WhisperTargetButton.transform, false);
                Set(Chat, "whisperTargetButton", WhisperTargetButton);

                root.SetActive(true);
                Set(World, "<Stage>k__BackingField", C02Stage.InWorld);
                Set(World, "<ZoneID>k__BackingField", (ushort)1);
                Set(World, "<InternalID>k__BackingField", 123U);
                Set(World, "<HasOwnFull>k__BackingField", true);
                Set(World, "ownFull", new FullStateField
                {
                    zoneInternalID = 123, hp = 100, maxHp = 100, mp = 50, maxMp = 50,
                    x = 10, y = 10, dir = 0
                });
                ((GameObject)Get(World, "worldRoot")).SetActive(true);
                Invoke(Chat, "WorldChanged");
            }

            public void SendAs(byte scope, string message)
            {
                ScopeButtons[scope].onClick.Invoke();
                Input.SetTextWithoutNotify(message);
                SendButton.onClick.Invoke();
            }

            public void Receive(byte scope, ulong sender, string message)
            {
                Model.ChatReceived(new Packet.Message { senderID = sender, senderProfileId = (uint)sender, scope = scope, message = message });
                Dispatcher.Drain();
            }

            public void Dispose()
            {
                if (Login != null && Login.Model != null) Login.ReleaseClient();
                UnityEngine.Object.DestroyImmediate(root);
                initializer.Dispose();
            }

            private GameObject Child(string name)
            {
                var child = new GameObject(name, typeof(RectTransform));
                child.transform.SetParent(root.transform, false);
                return child;
            }

            private TMP_Text Text(string name) => Child(name).AddComponent<TextMeshProUGUI>();

            private TMP_InputField InputField(string name)
            {
                var field = Child(name).AddComponent<TMP_InputField>();
                field.textComponent = Child(name + " text").AddComponent<TextMeshProUGUI>();
                return field;
            }

            private Button ButtonObject(string name) => Child(name).AddComponent<Button>();
        }

        private sealed class QueuedDispatcher : IMainThreadDispatcher
        {
            private readonly Queue<Action> jobs = new Queue<Action>();
            public void Post(Action job) => jobs.Enqueue(job);
            public void Drain() { while (jobs.Count != 0) jobs.Dequeue()(); }
        }

        private sealed class NullLogger : ClientCore.ILogger
        {
            public void Log(string message) { }
        }

        private sealed class RecordingNetwork : INetworkService
        {
            public readonly List<(string, byte, ulong)> ChatCalls = new List<(string, byte, ulong)>();
            public void Chat(string message, byte scope, ulong targetID) => ChatCalls.Add((message, scope, targetID));
            public Task Connect(string address, int port) => Task.CompletedTask;
            public void CharacterList() { }
            public void Enter(ulong id) { }
            public void Action(byte dir, float x, float y, byte slot) { }
            public void Pong(ulong time) { }
            public void ZoneChange(byte operation) { }
            public void Log(string message) { }
            public void Profiles(uint[] profileIds) { }
            public void RequestInventory() { }
            public void RequestBazaar(uint requestID, ushort itemType, ushort page) { }
            public void RequestBazaarMyList(uint requestID) { }
            public void RequestBazaarClaim(uint requestID, ulong listingID) { }
            public void RequestBazaarBuy(uint requestID, ulong listingID) { }
            public void RequestBazaarCheckOutbox(uint requestID) { }
            public void RequestBazaarCancelListing(uint requestID, ulong listingID) { }
        }

        private static void Set(object target, string name, object value) =>
            target.GetType().GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).SetValue(target, value);

        private static object Get(object target, string name) =>
            target.GetType().GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).GetValue(target);

        private static void Invoke(object target, string name, params object[] args) =>
            target.GetType().GetMethod(name, BindingFlags.Instance | BindingFlags.NonPublic).Invoke(target, args);
    }
}
