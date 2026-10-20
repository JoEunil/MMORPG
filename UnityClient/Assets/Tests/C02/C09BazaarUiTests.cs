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
using UnityEngine.EventSystems;
using UnityEngine.UI;

namespace C02.Tests
{
    public sealed class C09BazaarUiTests
    {
        [Test]
        public void SearchRendersThirtyRealRowsAndEmptySuccessHasNoInventedRows()
        {
            using (var fixture = new Fixture())
            {
                fixture.OpenMarket();
                var listings = new BazaarListing[30];
                for (int i = 0; i < listings.Length; i++)
                    listings[i] = new BazaarListing
                    {
                        listingID = (ulong)(1000 + i), sellerCharacterID = (ulong)(5000 + i),
                        itemID = (uint)(700 + i), quantity = (ushort)(i + 1), price = (ulong)(9000 + i),
                        registeredAt = (ulong)(1700000000 + i), status = 0,
                    };
                fixture.Receive(true, listings);
                Assert.That(fixture.ActiveRowCount, Is.EqualTo(30));
                var lastRow = fixture.Row(1029);
                Assert.That(lastRow.Find("Item").GetComponent<TMP_Text>().text, Is.EqualTo("Item #729"));
                Assert.That(lastRow.Find("Quantity").GetComponent<TMP_Text>().text, Is.EqualTo("30"));
                Assert.That(lastRow.Find("Price").GetComponent<TMP_Text>().text, Is.EqualTo("9029"));
                Assert.That(lastRow.Find("State").GetComponent<TMP_Text>().text, Is.EqualTo("Trading"));
                lastRow.GetComponent<Button>().onClick.Invoke();
                Assert.That(fixture.Detail.text, Does.Contain("Listing #1029"));
                Assert.That(fixture.Detail.text, Does.Contain("Quantity 30"));
                Assert.That(fixture.Detail.text, Does.Contain("Price 9029"));
                Assert.That(fixture.Detail.text, Does.Contain("Seller #5029"));
                Assert.That(fixture.Detail.text, Does.Contain("Registered 1700000029"));

                fixture.Search.onClick.Invoke();
                Assert.That(fixture.ActiveRowCount, Is.Zero, "Refreshing clears the previous page before sending.");
                Assert.That(fixture.EmptyState.gameObject.activeSelf, Is.False);
                fixture.Receive(true);
                Assert.That(fixture.ActiveRowCount, Is.Zero);
                Assert.That(fixture.EmptyState.gameObject.activeSelf, Is.True);
                Assert.That(fixture.Status.text, Is.EqualTo("No listings in potions on this page."));
            }
        }

        [Test]
        public void FailureClearsRowsAndPermitsRetry()
        {
            using (var fixture = new Fixture())
            {
                fixture.OpenMarket();
                fixture.Receive(true, new BazaarListing { listingID = 8, itemID = 4, quantity = 2, price = 10 });
                Assert.That(fixture.ActiveRowCount, Is.EqualTo(1));
                Assert.That(fixture.Row(8).Find("Item").GetComponent<TMP_Text>().text, Is.EqualTo("Iron Shield"));
                fixture.Search.onClick.Invoke();
                fixture.Receive(false);
                Assert.That(fixture.ActiveRowCount, Is.Zero);
                Assert.That(fixture.Status.text, Does.Contain("failed"));
                Assert.That(fixture.Search.interactable, Is.True);
                fixture.Search.onClick.Invoke();
                Assert.That(fixture.Network.Requests, Is.EqualTo(3));
            }
        }

        [Test]
        public void TypeSelectionAndPaginationSendTheExistingItemTypeAndPageValues()
        {
            using (var fixture = new Fixture())
            {
                fixture.OpenMarket();
                Assert.That(fixture.Network.LastItemType, Is.EqualTo((ushort)1));
                Assert.That(fixture.Network.LastPage, Is.Zero);
                fixture.Receive(true);
                fixture.Next.onClick.Invoke();
                Assert.That(fixture.Network.LastItemType, Is.EqualTo((ushort)1));
                Assert.That(fixture.Network.LastPage, Is.EqualTo((ushort)1));
                fixture.Receive(true);
                fixture.TypeButtons[1].onClick.Invoke();
                Assert.That(fixture.Network.LastItemType, Is.EqualTo((ushort)2));
                Assert.That(fixture.Network.LastPage, Is.Zero);
            }
        }

        [Test]
        public void CloseAndTimeoutCancelPendingAndIgnoreLateResponse()
        {
            using (var fixture = new Fixture())
            {
                fixture.OpenMarket();
                uint firstId = fixture.Network.LastRequestID;
                fixture.Close.onClick.Invoke();
                Assert.That(Get<bool>(fixture.Screen, "requestPending"), Is.False);
                fixture.Receive(firstId, true, new BazaarListing { listingID = 9, itemID = 2, quantity = 1, price = 3 });
                Assert.That(fixture.ActiveRowCount, Is.Zero);
                Assert.That(fixture.Panel.activeSelf, Is.False);

                fixture.OpenMarket();
                uint timedOutId = fixture.Network.LastRequestID;
                Set(fixture.Screen, "requestDeadline", Time.unscaledTime - 1);
                Invoke(fixture.Screen, "Update");
                Assert.That(Get<bool>(fixture.Screen, "requestPending"), Is.False);
                Assert.That(fixture.Status.text, Does.Contain("timed out"));
                Assert.That(fixture.Search.interactable, Is.True, "A timed-out query can be retried with a new request ID.");
                fixture.Receive(timedOutId, true, new BazaarListing { listingID = 10, itemID = 3, quantity = 1, price = 4 });
                Assert.That(fixture.ActiveRowCount, Is.Zero);
                fixture.Search.onClick.Invoke();
                Assert.That(fixture.Network.Requests, Is.EqualTo(3));
                Assert.That(fixture.Network.LastRequestID, Is.Not.EqualTo(timedOutId));
            }
        }

        [Test]
        public void ZoneTransitionCancelsRequestAndClearsPanel()
        {
            using (var fixture = new Fixture())
            {
                fixture.OpenMarket();
                uint requestId = fixture.Network.LastRequestID;
                SetProperty(fixture.World, "Stage", C02Stage.TransitionPending);
                RaiseWorldChanged(fixture.World);
                Assert.That(fixture.Panel.activeSelf, Is.False);
                fixture.Receive(requestId, true, new BazaarListing { listingID = 12, itemID = 5, quantity = 1, price = 8 });
                Assert.That(fixture.ActiveRowCount, Is.Zero);
                Assert.That(fixture.Login.Data.GetBazaar(), Is.Empty,
                    "A cancelled reply must not repopulate the model behind the closed panel.");
            }
        }

        [Test]
        public void OpeningMarketStopsQueuedActionsAndBlocksSkillKeys()
        {
            using (var fixture = new Fixture())
            {
                fixture.Login.Data.UpdateMove(3, 11, 10);
                fixture.Login.Data.UpdateAction(3, 11, 10, 1);
                fixture.OpenMarket();
                var action = ((IViewDataModel)fixture.Login.Data).GetActionState();
                Assert.That(action.Item1, Is.False, "Opening the modal clears movement and queued skill actions.");
                Assert.That(InvokeBool(fixture.Combat, "InputAvailable"), Is.False,
                    "Digit-key skills are blocked while the market modal is visible.");
            }
        }

        private sealed class Fixture : IDisposable
        {
            private readonly GameObject root;
            private readonly List<Initializer> clients = new List<Initializer>();
            public readonly C01LoginScreen Login;
            public readonly C02WorldScreen World;
            public readonly C05CombatScreen Combat;
            public readonly C09BazaarScreen Screen;
            public readonly GameObject Panel;
            public readonly Button Market, Search, Close, Next;
            public readonly Button[] TypeButtons;
            public readonly Transform Rows;
            public readonly TMP_Text EmptyState, Detail, Status;
            public int ActiveRowCount
            {
                get
                {
                    int count = 0;
                    for (int i = 0; i < Rows.childCount; i++) if (Rows.GetChild(i).gameObject.activeSelf) count++;
                    return count;
                }
            }
            public FakeNetwork Network { get; private set; }

            public Fixture()
            {
                root = new GameObject("C09 bazaar fixture");
                root.SetActive(false);
                Login = root.AddComponent<C01LoginScreen>();
                World = root.AddComponent<C02WorldScreen>();
                Screen = root.AddComponent<C09BazaarScreen>();
                Combat = root.AddComponent<C05CombatScreen>();
                Set(Login, "loginIdInput", Input("login id"));
                Set(Login, "passwordInput", Input("password"));
                Set(Login, "loginButton", ButtonObject("login"));
                Set(Login, "statusText", Text("login status"));
                Set(Login, "characterListText", Text("character list"));
                var selection = Child("selection");
                var background = Child("background");
                var worldRoot = Child("world root");
                var hud = Child("world hud");
                var own = Child("own character");
                var field = Child("field");
                var camera = Child("camera");
                Set(World, "loginScreen", Login);
                Set(World, "characterChoices", Array.Empty<Button>());
                Set(World, "enterButton", ButtonObject("enter"));
                Set(World, "disconnectButton", ButtonObject("disconnect"));
                Set(World, "entryStatusText", Text("entry status"));
                Set(World, "worldStatusText", Text("world status"));
                Set(World, "selectionPanel", selection);
                Set(World, "background", background);
                Set(World, "worldRoot", worldRoot);
                Set(World, "worldHud", hud);
                Set(World, "ownCharacter", own.transform);
                Set(World, "fieldPlane", field.transform);
                Set(World, "quarterViewCamera", camera.transform);
                Panel = Child("bazaar panel");
                Panel.SetActive(false);
                Market = ButtonObject("market");
                Search = ButtonObject("search");
                Close = ButtonObject("close");
                TypeButtons = new[] { ButtonObject("type1"), ButtonObject("type2"), ButtonObject("type3") };
                var previous = ButtonObject("previous");
                Next = ButtonObject("next");
                var rowsObject = Child("rows");
                Rows = rowsObject.transform;
                Rows.gameObject.AddComponent<VerticalLayoutGroup>();
                Rows.gameObject.AddComponent<ContentSizeFitter>();
                EmptyState = Text("empty state");
                Detail = Text("selected detail");
                Status = Text("status");
                var page = Text("page");
                Set(Screen, "loginScreen", Login);
                Set(Screen, "worldScreen", World);
                Set(Screen, "bazaarPanel", Panel);
                Set(Screen, "marketButton", Market);
                Set(Screen, "itemTypeButtons", TypeButtons);
                Set(Screen, "searchButton", Search);
                Set(Screen, "previousPageButton", previous);
                Set(Screen, "nextPageButton", Next);
                Set(Screen, "closeButton", Close);
                Set(Screen, "pageText", page);
                Set(Screen, "listingRows", Rows);
                Set(Screen, "emptyStateText", EmptyState);
                Set(Screen, "selectedDetailText", Detail);
                Set(Screen, "statusText", Status);
                Set(Combat, "loginScreen", Login);
                Set(Combat, "worldScreen", World);
                Set(Combat, "skillButtons", new[] { ButtonObject("strike"), ButtonObject("ring"), ButtonObject("line") });
                Set(Combat, "vitalsText", Text("vitals"));
                Set(Combat, "combatStatusText", Text("combat status"));
                Set(Combat, "aimText", Text("aim"));
                Set(Combat, "bazaarScreen", Screen);
                Child("event system").AddComponent<EventSystem>();
                var client = CreateClient();
                Set(Login, "client", client.Initializer);
                root.SetActive(true);
                worldRoot.SetActive(true);
                SetProperty(World, "Stage", C02Stage.InWorld);
                SetProperty(World, "HasOwnFull", true);
                Set(World, "ownFull", new FullStateField { hp = 100, zoneInternalID = 1 });
                RaiseWorldChanged(World);
            }

            public void OpenMarket() => Market.onClick.Invoke();
            public Transform Row(ulong id) => Rows.Find("Listing-" + id);
            public void Receive(bool success, params BazaarListing[] rows) => Receive(Network.LastRequestID, success, rows);
            public void Receive(uint requestID, bool success, params BazaarListing[] rows) =>
                ((IViewModelNet)Login.Model).BazaarReceived(requestID, (byte)(success ? 1 : 0), rows ?? Array.Empty<BazaarListing>());

            private (Initializer Initializer, IViewModelUI Model, FakeNetwork Network) CreateClient()
            {
                var assembly = typeof(IViewModelUI).Assembly;
                var dataType = assembly.GetType("ClientCore.ViewData", true);
                var data = Activator.CreateInstance(dataType, true);
                Network = new FakeNetwork();
                var modelType = assembly.GetType("ClientCore.ViewModel", true);
                var model = (IViewModelUI)Activator.CreateInstance(modelType, BindingFlags.Instance | BindingFlags.NonPublic, null,
                    new object[] { Network, data, new NullLogger(), new InlineDispatcher() }, null);
                model.ZoneChange(0);
                ((IViewModelNet)model).ZoneChageReceived(1, 1, 0, 1, 10, 10, 1);
                ((IViewModelNet)model).FullReceived(1, 1, 1, new[] { new FullStateField { zoneInternalID = 1, hp = 100 } });
                var initializer = new Initializer();
                Set(initializer, "_viewModel", model);
                Set(initializer, "_data", data);
                clients.Add(initializer);
                return (initializer, model, Network);
            }

            private GameObject Child(string name)
            {
                var child = new GameObject(name, typeof(RectTransform));
                child.transform.SetParent(root.transform, false);
                return child;
            }
            private TMP_InputField Input(string name)
            {
                var go = new GameObject(name, typeof(RectTransform), typeof(Image), typeof(TMP_InputField));
                go.transform.SetParent(root.transform, false);
                var label = new GameObject("text", typeof(RectTransform), typeof(TextMeshProUGUI));
                label.transform.SetParent(go.transform, false);
                var input = go.GetComponent<TMP_InputField>();
                input.targetGraphic = go.GetComponent<Image>();
                input.textComponent = label.GetComponent<TextMeshProUGUI>();
                input.textViewport = (RectTransform)go.transform;
                return input;
            }
            private Button ButtonObject(string name)
            {
                var go = new GameObject(name, typeof(RectTransform), typeof(Image), typeof(Button));
                go.transform.SetParent(root.transform, false);
                var button = go.GetComponent<Button>();
                button.targetGraphic = go.GetComponent<Image>();
                return button;
            }
            private TMP_Text Text(string name)
            {
                var go = new GameObject(name, typeof(RectTransform), typeof(TextMeshProUGUI));
                go.transform.SetParent(root.transform, false);
                var text = go.GetComponent<TextMeshProUGUI>();
                if (TMP_Settings.defaultFontAsset != null) text.font = TMP_Settings.defaultFontAsset;
                return text;
            }
            public void Dispose()
            {
                if (root != null) UnityEngine.Object.DestroyImmediate(root);
                foreach (var client in clients) client.Dispose();
            }
        }

        private sealed class FakeNetwork : INetworkService
        {
            public int Requests;
            public uint LastRequestID;
            public ushort LastItemType, LastPage;
            public void RequestBazaarBuy(uint requestID, ulong listingID) { }
            public void RequestBazaarCheckOutbox(uint requestID) { }
            public void RequestBazaarClaim(uint requestID, ulong listingID) { }
            public void RequestBazaarMyList(uint requestID) { }
            public void RequestBazaar(uint requestID, ushort itemType, ushort page)
            { Requests++; LastRequestID = requestID; LastItemType = itemType; LastPage = page; }
            public Task Connect(string address, int port) => Task.CompletedTask;
            public void CharacterList() { }
            public void Enter(ulong characterId) { }
            public void Chat(string message, byte scope, ulong targetId) { }
            public void Profiles(uint[] profileIds) { }
            public void RequestInventory() { }
            public void Action(byte direction, float x, float y, byte skillSlot) { }
            public void Pong(ulong serverTimeMs) { }
            public void ZoneChange(byte op) { }
            public void Log(string message) { }
        }
        private sealed class InlineDispatcher : IMainThreadDispatcher { public void Post(Action job) => job(); }
        private sealed class NullLogger : ClientCore.ILogger { public void Log(string message) { } }
        private static void Invoke(object target, string name) => target.GetType().GetMethod(name, BindingFlags.Instance | BindingFlags.NonPublic).Invoke(target, null);
        private static bool InvokeBool(object target, string name) => (bool)target.GetType().GetMethod(name, BindingFlags.Instance | BindingFlags.NonPublic).Invoke(target, null);
        private static T Get<T>(object target, string name) => (T)target.GetType().GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).GetValue(target);
        private static void Set(object target, string name, object value) => target.GetType().GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).SetValue(target, value);
        private static void SetProperty(object target, string name, object value) => target.GetType().GetProperty(name, BindingFlags.Instance | BindingFlags.Public).GetSetMethod(true).Invoke(target, new[] { value });
        private static void RaiseWorldChanged(C02WorldScreen world) => ((Action)typeof(C02WorldScreen).GetField("WorldStateChanged", BindingFlags.Instance | BindingFlags.NonPublic).GetValue(world))?.Invoke();
    }
}
