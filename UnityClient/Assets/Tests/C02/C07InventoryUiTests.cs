using System;
using System.Collections.Generic;
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
    // Deterministic component lifecycle fixtures. These are not native-input or server UI tests.
    public sealed class C07InventoryUiTests
    {
        [Test]
        public void ServerItemsBecomeCardsAndALaterFailureClearsPreviousInventory()
        {
            using (var fixture = new Fixture())
            {
                fixture.OpenBag();
                Assert.That(fixture.Network.Requests, Is.EqualTo(1));
                Assert.That(fixture.Refresh.interactable, Is.False, "A pending request disables duplicate refresh.");
                fixture.InvokeScreen("RequestInventory");
                Assert.That(fixture.Network.Requests, Is.EqualTo(1), "A second request cannot be sent while one is pending.");

                var tenRows = new InventoryItem[10];
                for (int slot = 9; slot >= 0; slot--)
                    tenRows[9 - slot] = new InventoryItem { itemID = (uint)(100 + slot), quantity = (ushort)(slot + 1), slot = (byte)slot };
                fixture.Receive(true, tenRows);

                Button[] cards = fixture.Cards;
                Assert.That(cards.Length, Is.EqualTo(10), "Each actual response item is represented by an active card.");
                Assert.That(cards[0].transform.Find("ItemName").GetComponent<TMP_Text>().text, Is.EqualTo("Item #100"));
                Assert.That(cards[9].transform.Find("ItemName").GetComponent<TMP_Text>().text, Is.EqualTo("Item #109"));
                cards[1].onClick.Invoke();
                Assert.That(fixture.DetailBadge.text, Is.EqualTo("?"));
                Assert.That(fixture.DetailName.text, Is.EqualTo("Item #101"));
                Assert.That(fixture.DetailSlot.text, Is.EqualTo("Slot 1"));
                Assert.That(fixture.DetailQuantity.text, Is.EqualTo("Quantity 2"));

                fixture.Refresh.onClick.Invoke();
                Assert.That(fixture.Cards, Is.Empty, "The previous cards clear while refreshing.");
                fixture.Receive(false);
                Assert.That(fixture.Cards, Is.Empty);
                Assert.That(fixture.Status.text, Does.Contain("failed"));
                Assert.That(fixture.Login.Data.GetInventory(), Is.Empty);
                Assert.That(fixture.Refresh.interactable, Is.True, "A definitive failure permits a manual retry.");
            }
        }

        [Test]
        public void KnownItemsUseServerBackedNamesAndAnEmptySuccessShowsNoInventedCard()
        {
            using (var fixture = new Fixture())
            {
                fixture.OpenBag();
                fixture.Receive(true,
                    new InventoryItem { itemID = 1, quantity = 4, slot = 2 },
                    new InventoryItem { itemID = 5, quantity = 1, slot = 3 });
                Assert.That(fixture.Cards.Length, Is.EqualTo(2));
                Assert.That(fixture.Cards[0].transform.Find("ItemName").GetComponent<TMP_Text>().text, Is.EqualTo("HP Potion"));
                Assert.That(fixture.Cards[0].transform.Find("ItemQuantity").GetComponent<TMP_Text>().text, Is.EqualTo("Qty 4"));
                Assert.That(fixture.Cards[1].transform.Find("ItemName").GetComponent<TMP_Text>().text, Is.EqualTo("Leather Armor"));
                fixture.Cards[0].onClick.Invoke();
                Assert.That(fixture.DetailBadge.text, Is.EqualTo("HP"));
                Assert.That(fixture.DetailName.text, Is.EqualTo("HP Potion"));
                Assert.That(fixture.DetailSlot.text, Is.EqualTo("Slot 2"));

                fixture.Refresh.onClick.Invoke();
                Assert.That(fixture.Cards, Is.Empty);
                fixture.Receive(true);
                Assert.That(fixture.Cards, Is.Empty);
                Assert.That(fixture.Status.text, Is.EqualTo("No items."));
                Assert.That(fixture.DetailName.text, Is.EqualTo("Select an item"));
                Assert.That(fixture.Login.Data.GetInventory(), Is.Empty);
            }
        }

        [Test]
        public void ReenablingAfterSuccessRequestsFreshItemsInsteadOfKeepingAnEmptyCachedView()
        {
            using (var fixture = new Fixture())
            {
                fixture.OpenBag();
                fixture.Receive(true, new InventoryItem { itemID = 1, quantity = 3, slot = 0 });
                Assert.That(fixture.Cards.Length, Is.EqualTo(1));
                fixture.InvokeScreen("OnDisable");
                Assert.That(fixture.Cards, Is.Empty);
                fixture.InvokeScreen("OnEnable");
                fixture.OpenBag();
                Assert.That(fixture.Network.Requests, Is.EqualTo(2), "A cleared view must request a fresh snapshot on reactivation.");
                fixture.Receive(true, new InventoryItem { itemID = 2, quantity = 7, slot = 0 });
                Assert.That(fixture.Cards.Length, Is.EqualTo(1));
                fixture.Cards[0].onClick.Invoke();
                Assert.That(fixture.DetailName.text, Is.EqualTo("MP Potion"));
                Assert.That(fixture.DetailQuantity.text, Is.EqualTo("Quantity 7"));
            }
        }

        [Test]
        public void TimeoutStaysLockedAcrossCloseOpenAndIgnoresLateReply()
        {
            using (var fixture = new Fixture())
            {
                fixture.OpenBag();
                SetPrivate(fixture.Screen, "requestDeadline", Time.unscaledTime - 1f);
                fixture.InvokeScreen("Update");
                Assert.That(fixture.Status.text, Does.Contain("Reconnect"));
                Assert.That(fixture.Refresh.interactable, Is.False);

                fixture.Close.onClick.Invoke();
                fixture.Bag.onClick.Invoke();
                Assert.That(fixture.Panel.activeSelf, Is.True);
                fixture.InvokeScreen("RequestInventory");
                Assert.That(fixture.Network.Requests, Is.EqualTo(1));

                fixture.Receive(true, new InventoryItem { itemID = 900, quantity = 1, slot = 0 });
                Assert.That(fixture.Cards, Is.Empty, "A reply arriving after timeout cannot repopulate the UI.");
                Assert.That(fixture.Status.text, Does.Contain("Reconnect"));
                Assert.That(fixture.Refresh.interactable, Is.False);
            }
        }

        [Test]
        public void ANewClientModelClearsTheTimeoutLock()
        {
            using (var fixture = new Fixture())
            {
                fixture.OpenBag();
                SetPrivate(fixture.Screen, "requestDeadline", Time.unscaledTime - 1f);
                fixture.InvokeScreen("Update");
                Assert.That(fixture.Refresh.interactable, Is.False);

                fixture.Login.ReleaseClient();
                var replacement = fixture.CreateClient();
                SetPrivate(fixture.Login, "client", replacement.Initializer);
                fixture.RaiseClientChanged(replacement.Model);
                fixture.SetWorldReady();

                Assert.That(fixture.Refresh.interactable, Is.True);
                fixture.Bag.onClick.Invoke();
                Assert.That(replacement.Network.Requests, Is.EqualTo(1));
            }
        }

        private sealed class Fixture : IDisposable
        {
            private readonly GameObject root;
            private readonly List<Initializer> clients = new List<Initializer>();
            public readonly C01LoginScreen Login;
            public readonly C02WorldScreen World;
            public readonly C07InventoryScreen Screen;
            public readonly GameObject Panel;
            public readonly Button Bag;
            public readonly Button Refresh;
            public readonly Button Close;
            public readonly RectTransform Grid;
            public readonly TMP_Text DetailBadge;
            public readonly TMP_Text DetailName;
            public readonly TMP_Text DetailSlot;
            public readonly TMP_Text DetailQuantity;
            public readonly TMP_Text Status;
            public Button[] Cards => Grid.GetComponentsInChildren<Button>();
            public FakeNetwork Network { get; private set; }

            public Fixture()
            {
                root = new GameObject("C07 inventory lifecycle fixture");
                root.SetActive(false);
                Login = root.AddComponent<C01LoginScreen>();
                World = root.AddComponent<C02WorldScreen>();
                Screen = root.AddComponent<C07InventoryScreen>();

                SetPrivate(Login, "loginIdInput", Input("login id"));
                SetPrivate(Login, "passwordInput", Input("password"));
                SetPrivate(Login, "loginButton", ButtonObject("login"));
                SetPrivate(Login, "statusText", Text("login status"));
                SetPrivate(Login, "characterListText", Text("character list"));

                var selection = Child("selection panel");
                var background = Child("background");
                var worldRoot = Child("world root");
                worldRoot.SetActive(false);
                var worldHud = Child("world HUD");
                var own = Child("own character");
                var field = Child("field");
                var camera = Child("camera");
                SetPrivate(World, "loginScreen", Login);
                SetPrivate(World, "characterChoices", Array.Empty<Button>());
                SetPrivate(World, "enterButton", ButtonObject("enter"));
                SetPrivate(World, "disconnectButton", ButtonObject("disconnect"));
                SetPrivate(World, "entryStatusText", Text("entry status"));
                SetPrivate(World, "worldStatusText", Text("world status"));
                SetPrivate(World, "selectionPanel", selection);
                SetPrivate(World, "background", background);
                SetPrivate(World, "worldRoot", worldRoot);
                SetPrivate(World, "worldHud", worldHud);
                SetPrivate(World, "ownCharacter", own.transform);
                SetPrivate(World, "fieldPlane", field.transform);
                SetPrivate(World, "quarterViewCamera", camera.transform);

                Panel = Child("inventory panel");
                Bag = ButtonObject("bag");
                Refresh = ButtonObject("refresh");
                Close = ButtonObject("close");
                var scrollObject = Child("inventory scroll");
                var scroll = scrollObject.AddComponent<ScrollRect>();
                var viewport = Child("inventory viewport");
                viewport.transform.SetParent(scrollObject.transform, false);
                viewport.AddComponent<Image>();
                viewport.AddComponent<Mask>();
                scroll.viewport = (RectTransform)viewport.transform;
                var gridObject = Child("inventory grid");
                gridObject.transform.SetParent(viewport.transform, false);
                Grid = (RectTransform)gridObject.transform;
                gridObject.AddComponent<GridLayoutGroup>();
                scroll.content = Grid;
                DetailName = Text("detail name");
                DetailBadge = Text("detail badge");
                DetailSlot = Text("detail slot");
                DetailQuantity = Text("detail quantity");
                Status = Text("inventory status");
                SetPrivate(Screen, "loginScreen", Login);
                SetPrivate(Screen, "worldScreen", World);
                SetPrivate(Screen, "inventoryPanel", Panel);
                SetPrivate(Screen, "bagButton", Bag);
                SetPrivate(Screen, "refreshButton", Refresh);
                SetPrivate(Screen, "closeButton", Close);
                SetPrivate(Screen, "inventoryScroll", scroll);
                SetPrivate(Screen, "inventoryGridContent", Grid);
                SetPrivate(Screen, "detailBadge", DetailBadge);
                SetPrivate(Screen, "detailName", DetailName);
                SetPrivate(Screen, "detailSlot", DetailSlot);
                SetPrivate(Screen, "detailQuantity", DetailQuantity);
                SetPrivate(Screen, "inventoryStatus", Status);

                var client = CreateClient();
                SetPrivate(Login, "client", client.Initializer);
                root.SetActive(true);
                SetWorldReady();
            }

            public void OpenBag() => Bag.onClick.Invoke();

            public void SetWorldReady()
            {
                var worldRoot = (GameObject)GetPrivate(World, "worldRoot");
                worldRoot.SetActive(true);
                SetPrivateProperty(World, "Stage", C02Stage.InWorld);
                SetPrivateProperty(World, "HasOwnFull", true);
                var changed = (Action)GetPrivate(World, "WorldStateChanged");
                changed?.Invoke();
            }

            public void Receive(bool success, params InventoryItem[] items) =>
                ((IViewModelNet)Login.Model).InventoryReceived(success ? (byte)1 : (byte)0, items ?? Array.Empty<InventoryItem>());

            public void RaiseClientChanged(IViewModelUI model)
            {
                var changed = (Action<IViewModelUI>)GetPrivate(Login, "ClientChanged");
                changed?.Invoke(model);
            }

            public (Initializer Initializer, IViewModelUI Model, FakeNetwork Network) CreateClient()
            {
                var assembly = typeof(IViewModelUI).Assembly;
                var dataType = assembly.GetType("ClientCore.ViewData", true);
                var data = Activator.CreateInstance(dataType, true);
                var network = new FakeNetwork();
                var dispatcher = new InlineDispatcher();
                var modelType = assembly.GetType("ClientCore.ViewModel", true);
                var model = (IViewModelUI)Activator.CreateInstance(modelType,
                    BindingFlags.Instance | BindingFlags.NonPublic, null,
                    new object[] { network, data, new NullLogger(), dispatcher }, null);
                model.ZoneChange(0);
                ((IViewModelNet)model).ZoneChageReceived(1, 1, 1, 1, 0, 0, 1);
                ((IViewModelNet)model).FullReceived(1, 1, 1, new[] { new FullStateField { zoneInternalID = 1 } });
                var initializer = new Initializer();
                SetPrivate(initializer, "_viewModel", model);
                SetPrivate(initializer, "_data", data);
                clients.Add(initializer);
                if (Network == null) Network = network;
                return (initializer, model, network);
            }

            public void InvokeScreen(string name) => typeof(C07InventoryScreen)
                .GetMethod(name, BindingFlags.Instance | BindingFlags.NonPublic).Invoke(Screen, null);

            public void Dispose()
            {
                if (root != null) UnityEngine.Object.DestroyImmediate(root);
                foreach (var initializer in clients) initializer.Dispose();
            }

            private GameObject Child(string name)
            {
                var child = new GameObject(name, typeof(RectTransform));
                child.transform.SetParent(root.transform, false);
                return child;
            }

            private TMP_InputField Input(string name)
            {
                var go = new GameObject(name, typeof(RectTransform), typeof(CanvasRenderer), typeof(Image), typeof(TMP_InputField));
                go.transform.SetParent(root.transform, false);
                var textObject = new GameObject("text", typeof(RectTransform), typeof(CanvasRenderer), typeof(TextMeshProUGUI));
                textObject.transform.SetParent(go.transform, false);
                var input = go.GetComponent<TMP_InputField>();
                input.targetGraphic = go.GetComponent<Image>();
                input.textComponent = textObject.GetComponent<TextMeshProUGUI>();
                input.textViewport = (RectTransform)go.transform;
                return input;
            }

            private Button ButtonObject(string name)
            {
                var go = new GameObject(name, typeof(RectTransform), typeof(CanvasRenderer), typeof(Image), typeof(Button));
                go.transform.SetParent(root.transform, false);
                var button = go.GetComponent<Button>();
                button.targetGraphic = go.GetComponent<Image>();
                return button;
            }

            private TMP_Text Text(string name)
            {
                var go = new GameObject(name, typeof(RectTransform), typeof(CanvasRenderer), typeof(TextMeshProUGUI));
                go.transform.SetParent(root.transform, false);
                var text = go.GetComponent<TextMeshProUGUI>();
                if (TMP_Settings.defaultFontAsset != null) text.font = TMP_Settings.defaultFontAsset;
                return text;
            }
        }

        private sealed class InlineDispatcher : IMainThreadDispatcher
        {
            public void Post(Action job) => job();
        }

        private sealed class FakeNetwork : INetworkService
        {
            public int Requests;
            public void RequestBazaarMyList(uint requestID) { }
            public void RequestBazaar(uint requestID, ushort itemType, ushort page) { }
            public void RequestInventory() => Requests++;
            public Task Connect(string address, int port) => Task.CompletedTask;
            public void CharacterList() { }
            public void Enter(ulong characterId) { }
            public void Chat(string message, byte scope, ulong targetId) { }
            public void Profiles(uint[] profileIds) { }
            public void Action(byte direction, float x, float y, byte skillSlot) { }
            public void Pong(ulong serverTimeMs) { }
            public void ZoneChange(byte op) { }
            public void Log(string message) { }
        }

        private sealed class NullLogger : ClientCore.ILogger
        {
            public void Log(string message) { }
        }

        private static object GetPrivate(object target, string name) => target.GetType()
            .GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).GetValue(target);

        private static void SetPrivate(object target, string name, object value) => target.GetType()
            .GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).SetValue(target, value);

        private static void SetPrivateProperty(object target, string name, object value) => target.GetType()
            .GetProperty(name, BindingFlags.Instance | BindingFlags.Public).GetSetMethod(true).Invoke(target, new[] { value });
    }
}
