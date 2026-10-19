using System;
using System.Reflection;
using System.Threading.Tasks;
using C01;
using C02;
using ClientCore;
using ClientCore.PacketHelper;
using ClientCore.Services;
using NUnit.Framework;
using TMPro;
using UnityEngine;
using UnityEngine.UI;

namespace C02.Tests
{
    // Pure boundary eligibility checks; no scene, input, Play, or server session.
    public sealed class C08ZoneTransitionTests
    {
        public static string RunPureChecks()
        {
            var tests = new C08ZoneTransitionTests();
            tests.ZoneTransitionButtonTargetsOnlyTheRealNeighborInsideTheSharedBoundary();
            tests.TransitionRequestIsSingleAndRejectionRestoresConfirmedWorld();
            tests.AcknowledgementKeepsActionsLockedUntilMatchingSelfFull();
            tests.TransitionTimeoutRequiresReconnect();
            tests.PendingInventoryReplyDuringTransitionIsSavedWithoutUnlockingBagInput();
            tests.FailedInventoryReplyDuringTransitionAllowsManualRetryAfterArrival();
            return "C08 UI regression: 6/6 passed. No Play, input, or server session.";
        }

        [Test]
        public void TransitionRequestIsSingleAndRejectionRestoresConfirmedWorld()
        {
            using (var fixture = new InventoryFixture())
            {
                int before = fixture.Network.ZoneOperations;
                fixture.RequestTransition();
                fixture.RequestTransition();
                Assert.That(fixture.Network.ZoneOperations - before, Is.EqualTo(1));
                Assert.That(fixture.Network.LastZoneOperation, Is.EqualTo(4));
                Assert.That(fixture.World.Stage, Is.EqualTo(C02Stage.TransitionPending));
                Assert.That(fixture.World.CanAct, Is.False);

                ((IViewModelNet)fixture.Model).ZoneChageReceived(0, 0, 0, 0, 0, 0, 0);
                Assert.That(fixture.World.Stage, Is.EqualTo(C02Stage.InWorld));
                Assert.That(fixture.World.ZoneID, Is.EqualTo(1));
                Assert.That(fixture.World.InternalID, Is.EqualTo(11));
                Assert.That(fixture.World.OwnFull.hp, Is.EqualTo(80));
                Assert.That(fixture.World.OwnFull.mp, Is.EqualTo(70));
                Assert.That(fixture.World.CanAct, Is.True);
            }
        }

        [Test]
        public void AcknowledgementKeepsActionsLockedUntilMatchingSelfFull()
        {
            using (var fixture = new InventoryFixture())
            {
                fixture.RequestTransition();
                ((IViewModelNet)fixture.Model).ZoneChageReceived(1, 2, 0, 12, 100, 50, 2);
                Assert.That(fixture.World.Stage, Is.EqualTo(C02Stage.TransitionPending));
                Assert.That(fixture.World.ZoneID, Is.EqualTo(2));
                Assert.That(fixture.World.HasOwnFull, Is.False);
                Assert.That(fixture.World.CanAct, Is.False);

                ((IViewModelNet)fixture.Model).FullReceived(2, 2, 1,
                    new[] { new FullStateField { zoneInternalID = 11, hp = 80, mp = 70, maxHp = 100, maxMp = 100, x = 100, y = 50 } });
                Assert.That(fixture.World.CanAct, Is.False, "A different internal ID cannot unlock the new Zone.");
                ((IViewModelNet)fixture.Model).FullReceived(2, 2, 1,
                    new[] { new FullStateField { zoneInternalID = 12, hp = 75, mp = 65, maxHp = 100, maxMp = 100, x = 100, y = 50 } });
                Assert.That(fixture.World.Stage, Is.EqualTo(C02Stage.InWorld));
                Assert.That(fixture.World.CanAct, Is.True);
                Assert.That(fixture.World.OwnFull.hp, Is.EqualTo(75));
                Assert.That(fixture.World.OwnFull.mp, Is.EqualTo(65));
            }
        }

        [Test]
        public void TransitionTimeoutRequiresReconnect()
        {
            using (var fixture = new InventoryFixture())
            {
                fixture.RequestTransition();
                SetPrivate(fixture.World, "transitionDeadline", Time.unscaledTimeAsDouble - 1);
                typeof(C02WorldScreen).GetMethod("Update", BindingFlags.Instance | BindingFlags.NonPublic)
                    .Invoke(fixture.World, null);
                Assert.That(fixture.World.Stage, Is.EqualTo(C02Stage.ReconnectRequired));
                Assert.That(fixture.World.CanAct, Is.False);
                Assert.That(fixture.WorldStatus.text, Does.Contain("reconnect"));
            }
        }

        [Test]
        public void ZoneTransitionButtonTargetsOnlyTheRealNeighborInsideTheSharedBoundary()
        {
            AssertMapping(1, new Vector2(100, 50), true, 4, 2);
            AssertMapping(2, new Vector2(100, 50), true, 3, 1);
            AssertMapping(1, new Vector2(95, -5), true, 4, 2);
            AssertMapping(2, new Vector2(105, 105), true, 3, 1);
            AssertMapping(1, new Vector2(94.99f, 50), false, 0, 0);
            AssertMapping(2, new Vector2(105.01f, 50), false, 0, 0);
            AssertMapping(1, new Vector2(100, 105.01f), false, 0, 0);
            AssertMapping(1, new Vector2(100, -5.01f), false, 0, 0);
            AssertMapping(3, new Vector2(200, 50), false, 0, 0);
            AssertMapping(4, new Vector2(50, 100), false, 0, 0);
            AssertMapping(1, new Vector2(float.NaN, 50), false, 0, 0);
            AssertMapping(2, new Vector2(100, float.PositiveInfinity), false, 0, 0);
        }

        [Test]
        public void PendingInventoryReplyDuringTransitionIsSavedWithoutUnlockingBagInput()
        {
            using (var fixture = new InventoryFixture())
            {
                fixture.OpenBag();
                Assert.That(fixture.Network.Requests, Is.EqualTo(1));
                fixture.BeginTransition();
                Assert.That(fixture.Panel.activeSelf, Is.False);
                Assert.That(fixture.Bag.gameObject.activeSelf, Is.False);

                fixture.SetItems(new InventoryItem { itemID = 42, quantity = 3, slot = 2 });
                fixture.Receive(true);
                Assert.That(fixture.Grid.childCount, Is.EqualTo(1));
                Assert.That(fixture.ItemName.text, Is.EqualTo("Item #42"));
                Assert.That(fixture.ItemQuantity.text, Is.EqualTo("Qty 3"));
                Assert.That(fixture.Panel.activeSelf, Is.False);
                Assert.That(fixture.Refresh.interactable, Is.False, "Travel keeps bag requests locked.");

                fixture.Arrive();
                fixture.Bag.onClick.Invoke();
                Assert.That(fixture.Panel.activeSelf, Is.True);
                Assert.That(fixture.Grid.childCount, Is.EqualTo(1));
                Assert.That(fixture.ItemName.text, Is.EqualTo("Item #42"));
                Assert.That(fixture.Network.Requests, Is.EqualTo(1), "The saved response avoids a duplicate request after arrival.");
            }
        }

        [Test]
        public void FailedInventoryReplyDuringTransitionAllowsManualRetryAfterArrival()
        {
            using (var fixture = new InventoryFixture())
            {
                fixture.OpenBag();
                fixture.BeginTransition();
                fixture.Receive(false);
                Assert.That(fixture.Panel.activeSelf, Is.False);
                Assert.That(fixture.Refresh.interactable, Is.False, "Travel keeps the bag closed.");
                Assert.That(fixture.Status.text, Does.Contain("after arrival"));

                fixture.Arrive();
                Assert.That(fixture.Refresh.interactable, Is.True, "A definitive failure permits retry after arrival.");
                fixture.Refresh.onClick.Invoke();
                Assert.That(fixture.Network.Requests, Is.EqualTo(2));
            }
        }

        private static void AssertMapping(ushort zone, Vector2 position, bool expected, byte operation, ushort targetZone)
        {
            var args = new object[] { zone, position, (byte)0, (ushort)0 };
            bool available = (bool)typeof(C02WorldScreen)
                .GetMethod("TryNearbyZoneChange", BindingFlags.Static | BindingFlags.NonPublic)
                .Invoke(null, args);
            Assert.That(available, Is.EqualTo(expected), $"Zone {zone} at {position} transition eligibility.");
            Assert.That((byte)args[2], Is.EqualTo(operation));
            Assert.That((ushort)args[3], Is.EqualTo(targetZone));
        }

        private sealed class InventoryFixture : IDisposable
        {
            private readonly GameObject root;
            private readonly object viewData;
            private readonly IViewModelUI model;
            private InventoryItem[] replyItems = Array.Empty<InventoryItem>();
            public readonly C01LoginScreen Login;
            public readonly C02WorldScreen World;
            public readonly C07InventoryScreen Screen;
            public readonly GameObject Panel;
            public readonly Button Bag;
            public readonly Button Refresh;
            public readonly RectTransform Grid;
            public TMP_Text ItemName => Grid.GetChild(0).Find("ItemName").GetComponent<TMP_Text>();
            public TMP_Text ItemQuantity => Grid.GetChild(0).Find("ItemQuantity").GetComponent<TMP_Text>();
            public readonly TMP_Text Status;
            public readonly TMP_Text WorldStatus;
            public readonly FakeNetwork Network = new FakeNetwork();
            public IViewModelUI Model => model;

            public InventoryFixture()
            {
                root = new GameObject("C08 inventory transition fixture");
                root.SetActive(false);
                Login = root.AddComponent<C01LoginScreen>();
                World = root.AddComponent<C02WorldScreen>();
                Screen = root.AddComponent<C07InventoryScreen>();
                SetPrivate(Login, "loginIdInput", Input("login id"));
                SetPrivate(Login, "passwordInput", Input("password"));
                SetPrivate(Login, "loginButton", ButtonObject("login"));
                SetPrivate(Login, "statusText", Text("login status"));
                SetPrivate(Login, "characterListText", Text("character list"));

                SetPrivate(World, "loginScreen", Login);
                SetPrivate(World, "characterChoices", Array.Empty<Button>());
                SetPrivate(World, "enterButton", ButtonObject("enter"));
                SetPrivate(World, "disconnectButton", ButtonObject("disconnect"));
                SetPrivate(World, "entryStatusText", Text("entry status"));
                WorldStatus = Text("world status");
                SetPrivate(World, "worldStatusText", WorldStatus);
                SetPrivate(World, "selectionPanel", Child("selection panel"));
                SetPrivate(World, "background", Child("background"));
                SetPrivate(World, "worldRoot", Child("world root"));
                SetPrivate(World, "worldHud", Child("world HUD"));
                SetPrivate(World, "ownCharacter", Child("own character").transform);
                SetPrivate(World, "fieldPlane", Child("field").transform);
                SetPrivate(World, "quarterViewCamera", Child("camera").transform);

                Panel = Child("inventory panel");
                Bag = ButtonObject("bag");
                Refresh = ButtonObject("refresh");
                SetPrivate(Screen, "loginScreen", Login);
                SetPrivate(Screen, "worldScreen", World);
                SetPrivate(Screen, "inventoryPanel", Panel);
                SetPrivate(Screen, "bagButton", Bag);
                SetPrivate(Screen, "refreshButton", Refresh);
                SetPrivate(Screen, "closeButton", ButtonObject("close"));
                var scrollObject = Child("inventory scroll");
                var scroll = scrollObject.AddComponent<ScrollRect>();
                var gridObject = Child("inventory grid");
                Grid = (RectTransform)gridObject.transform;
                scroll.content = Grid;
                SetPrivate(Screen, "inventoryScroll", scroll);
                SetPrivate(Screen, "inventoryGridContent", Grid);
                SetPrivate(Screen, "detailBadge", Text("detail badge"));
                SetPrivate(Screen, "detailName", Text("detail name"));
                SetPrivate(Screen, "detailSlot", Text("detail slot"));
                SetPrivate(Screen, "detailQuantity", Text("detail quantity"));
                Status = Text("inventory status");
                SetPrivate(Screen, "inventoryStatus", Status);

                var assembly = typeof(IViewModelUI).Assembly;
                viewData = Activator.CreateInstance(assembly.GetType("ClientCore.ViewData", true), true);
                model = (IViewModelUI)Activator.CreateInstance(assembly.GetType("ClientCore.ViewModel", true),
                    BindingFlags.Instance | BindingFlags.NonPublic, null,
                    new object[] { Network, viewData, new NullLogger(), new InlineDispatcher() }, null);
                SetPrivate(model, "_worldReady", true);
                var initializer = new Initializer();
                SetPrivate(initializer, "_viewModel", model);
                SetPrivate(initializer, "_data", viewData);
                SetPrivate(Login, "client", initializer);
                model.ZoneChange(0);
                ((IViewModelNet)model).ZoneChageReceived(1, 1, 1, 11, 100, 50, 1);
                ((IViewModelNet)model).FullReceived(1, 1, 1, new[]
                {
                    new FullStateField { zoneInternalID = 11, hp = 80, mp = 70, maxHp = 100, maxMp = 100, x = 100, y = 50 }
                });
                root.SetActive(true);
                Arrive();
            }

            public void OpenBag() => Bag.onClick.Invoke();
            public void BeginTransition()
            {
                RequestTransition();
            }
            public void RequestTransition() => typeof(C02WorldScreen)
                .GetMethod("RequestNearbyZoneChange", BindingFlags.Instance | BindingFlags.NonPublic)
                .Invoke(World, null);
            public void Arrive()
            {
                if (World.Stage == C02Stage.TransitionPending)
                {
                    ((IViewModelNet)model).ZoneChageReceived(1, 2, 0, 12, 100, 50, 2);
                    ((IViewModelNet)model).FullReceived(2, 2, 1, new[]
                    {
                        new FullStateField { zoneInternalID = 12, hp = 80, mp = 70, maxHp = 100, maxMp = 100, x = 100, y = 50 }
                    });
                    return;
                }
                SetPrivateProperty(World, "Stage", C02Stage.InWorld);
                SetPrivateProperty(World, "HasOwnFull", true);
                SetPrivateProperty(World, "ZoneID", (ushort)1);
                SetPrivateProperty(World, "InternalID", (uint)11);
                SetPrivate(World, "ownFull", new FullStateField
                {
                    zoneInternalID = 11, hp = 80, mp = 70, maxHp = 100, maxMp = 100,
                    x = 100, y = 50, dir = 0
                });
                ((GameObject)GetPrivate(World, "worldRoot")).SetActive(true);
                ((Transform)GetPrivate(World, "ownCharacter")).gameObject.SetActive(true);
                ((Action)GetPrivate(World, "WorldStateChanged"))?.Invoke();
            }
            public void SetItems(params InventoryItem[] items) => replyItems = items;
            public void Receive(bool success) => ((IViewModelNet)model).InventoryReceived(
                success ? (byte)1 : (byte)0, replyItems);

            public void Dispose() { if (root != null) UnityEngine.Object.DestroyImmediate(root); }
            private GameObject Child(string name)
            {
                var child = new GameObject(name, typeof(RectTransform));
                child.transform.SetParent(root.transform, false);
                return child;
            }
            private Button ButtonObject(string name)
            {
                var go = new GameObject(name, typeof(RectTransform), typeof(CanvasRenderer), typeof(Image), typeof(Button));
                go.transform.SetParent(root.transform, false);
                return go.GetComponent<Button>();
            }
            private TMP_Text Text(string name)
            {
                var go = new GameObject(name, typeof(RectTransform), typeof(CanvasRenderer), typeof(TextMeshProUGUI));
                go.transform.SetParent(root.transform, false);
                return go.GetComponent<TMP_Text>();
            }
            private TMP_InputField Input(string name)
            {
                var go = new GameObject(name, typeof(RectTransform), typeof(CanvasRenderer), typeof(Image), typeof(TMP_InputField));
                go.transform.SetParent(root.transform, false);
                var text = new GameObject("text", typeof(RectTransform), typeof(CanvasRenderer), typeof(TextMeshProUGUI));
                text.transform.SetParent(go.transform, false);
                var input = go.GetComponent<TMP_InputField>();
                input.targetGraphic = go.GetComponent<Image>();
                input.textComponent = text.GetComponent<TextMeshProUGUI>();
                input.textViewport = (RectTransform)go.transform;
                return input;
            }
        }

        private static void SetPrivate(object target, string name, object value) => target.GetType()
            .GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).SetValue(target, value);
        private static object GetPrivate(object target, string name) => target.GetType()
            .GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).GetValue(target);
        private static void SetPrivateProperty(object target, string name, object value) => target.GetType()
            .GetProperty(name).GetSetMethod(true).Invoke(target, new[] { value });

        private sealed class InlineDispatcher : IMainThreadDispatcher { public void Post(Action job) => job(); }
        private sealed class NullLogger : ClientCore.ILogger { public void Log(string message) { } }
        private sealed class FakeNetwork : INetworkService
        {
            public int Requests;
            public int ZoneOperations;
            public byte LastZoneOperation;
            public Task Connect(string address, int port) => Task.CompletedTask;
            public void CharacterList() { }
            public void Enter(ulong charID) { }
            public void Chat(string message, byte scope, ulong targetID) { }
            public void Profiles(uint[] profileIds) { }
            public void RequestBazaarMyList(uint requestID) { }
            public void RequestBazaar(uint requestID, ushort itemType, ushort page) { } public void RequestInventory() => Requests++;
            public void Action(byte dir, float x, float y, byte skillSlot) { }
            public void Pong(ulong serverTimeMs) { }
            public void ZoneChange(byte op) { ZoneOperations++; LastZoneOperation = op; }
            public void Log(string message) { }
        }
    }
}
