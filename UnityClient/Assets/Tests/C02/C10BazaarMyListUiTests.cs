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
    public sealed class C10BazaarMyListUiTests
    {
        [Test]
        public void MyListingsUsesServerRowsAndDoesNotDescribeSoldItemsAsClaimable()
        {
            using (var fixture = new Fixture())
            {
                fixture.OpenMarket();
                uint lateSearchId = fixture.Network.LastSearchID;
                fixture.MyListings.onClick.Invoke();
                Assert.That(fixture.Network.MyListRequests, Is.EqualTo(1));
                uint myListId = fixture.Network.LastMyListID;

                ((IViewModelNet)fixture.Login.Model).BazaarReceived(lateSearchId, 1, new[] { Listing(90, 0) });
                Assert.That(fixture.ScreenRows.text, Is.Empty, "The previous tab's late response must not populate this view.");
                Assert.That(fixture.Status.text, Is.EqualTo("Loading my listings…"));

                ((IViewModelNet)fixture.Login.Model).BazaarMyListReceived(myListId, 1,
                    new[] { Listing(91, 1), Listing(92, 2) });
                Assert.That(fixture.ScreenRows.text, Does.Contain("Listing #91 · Sold"));
                Assert.That(fixture.ScreenRows.text, Does.Contain("Listing #92 · Claimed"));
                Assert.That(fixture.ScreenRows.text, Does.Contain("Item 700  |  Quantity 2  |  Price 9000"));
                Assert.That(fixture.ScreenRows.text, Does.Not.Contain("Available to claim"));
                Assert.That(fixture.Status.text, Is.EqualTo("My listings: 2 listings · up to 30 shown"));
                Assert.That(fixture.Next.gameObject.activeSelf, Is.False, "My listings has no search pagination.");
            }
        }

        [Test]
        public void ReturningToSearchKeepsItsFilterAndPageAndClosingCancelsMyList()
        {
            using (var fixture = new Fixture())
            {
                fixture.OpenMarket();
                fixture.ReceiveSearch();
                fixture.TypeButtons[1].onClick.Invoke();
                fixture.ReceiveSearch();
                fixture.Next.onClick.Invoke();
                fixture.ReceiveSearch();
                Assert.That(fixture.Network.LastItemType, Is.EqualTo((ushort)2));
                Assert.That(fixture.Network.LastPage, Is.EqualTo((ushort)1));

                fixture.MyListings.onClick.Invoke();
                fixture.ReceiveMyList();
                fixture.SearchListings.onClick.Invoke();
                Assert.That(fixture.Network.LastItemType, Is.EqualTo((ushort)2));
                Assert.That(fixture.Network.LastPage, Is.EqualTo((ushort)1));

                fixture.MyListings.onClick.Invoke();
                uint requestID = fixture.Network.LastMyListID;
                fixture.Close.onClick.Invoke();
                ((IViewModelNet)fixture.Login.Model).BazaarMyListReceived(requestID, 1, new[] { Listing(93, 0) });
                Assert.That(fixture.Panel.activeSelf, Is.False);
                Assert.That(fixture.ScreenRows.text, Is.Empty);
            }
        }

        private static BazaarListing Listing(ulong id, byte status) => new BazaarListing
        {
            listingID = id,
            sellerCharacterID = 5011,
            itemID = 700,
            quantity = 2,
            price = 9000,
            registeredAt = 1700000000,
            status = status,
        };

        private sealed class Fixture : IDisposable
        {
            private readonly GameObject root;
            private readonly Initializer client;
            public readonly C01LoginScreen Login;
            public readonly C02WorldScreen World;
            public readonly C09BazaarScreen Screen;
            public readonly GameObject Panel;
            public readonly Button Market, Search, Close, Next, SearchListings, MyListings;
            public readonly Button[] TypeButtons;
            public readonly TMP_Text ScreenRows, Status;
            public readonly FakeNetwork Network;

            public Fixture()
            {
                root = new GameObject("C10 bazaar UI fixture");
                root.SetActive(false);
                Login = root.AddComponent<C01LoginScreen>();
                World = root.AddComponent<C02WorldScreen>();
                Screen = root.AddComponent<C09BazaarScreen>();
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
                Search = ButtonObject("refresh");
                Close = ButtonObject("close");
                SearchListings = ButtonObject("search listings");
                MyListings = ButtonObject("my listings");
                TypeButtons = new[] { ButtonObject("type 1"), ButtonObject("type 2"), ButtonObject("type 3") };
                var previous = ButtonObject("previous");
                Next = ButtonObject("next");
                ScreenRows = Text("rows");
                Status = Text("status");
                Set(Screen, "loginScreen", Login);
                Set(Screen, "worldScreen", World);
                Set(Screen, "bazaarPanel", Panel);
                Set(Screen, "marketButton", Market);
                Set(Screen, "searchListingsButton", SearchListings);
                Set(Screen, "myListingsButton", MyListings);
                Set(Screen, "itemTypeButtons", TypeButtons);
                Set(Screen, "searchButton", Search);
                Set(Screen, "previousPageButton", previous);
                Set(Screen, "nextPageButton", Next);
                Set(Screen, "closeButton", Close);
                Set(Screen, "pageText", Text("page"));
                Set(Screen, "rowsText", ScreenRows);
                Set(Screen, "statusText", Status);
                root.AddComponent<EventSystem>();

                var dataType = typeof(IViewModelUI).Assembly.GetType("ClientCore.ViewData", true);
                var data = Activator.CreateInstance(dataType, true);
                Network = new FakeNetwork();
                var modelType = typeof(IViewModelUI).Assembly.GetType("ClientCore.ViewModel", true);
                var model = (IViewModelUI)Activator.CreateInstance(modelType, BindingFlags.Instance | BindingFlags.NonPublic, null,
                    new object[] { Network, data, new NullLogger(), new InlineDispatcher() }, null);
                model.ZoneChange(0);
                ((IViewModelNet)model).ZoneChageReceived(1, 1, 0, 1, 10, 10, 1);
                ((IViewModelNet)model).FullReceived(1, 1, 1, new[] { new FullStateField { zoneInternalID = 1, hp = 100 } });
                client = new Initializer();
                Set(client, "_viewModel", model);
                Set(client, "_data", data);
                Set(Login, "client", client);

                root.SetActive(true);
                worldRoot.SetActive(true);
                SetProperty(World, "Stage", C02Stage.InWorld);
                SetProperty(World, "HasOwnFull", true);
                Set(World, "ownFull", new FullStateField { hp = 100, zoneInternalID = 1 });
                RaiseWorldChanged(World);
            }

            public void OpenMarket() => Market.onClick.Invoke();
            public void ReceiveSearch() => ((IViewModelNet)Login.Model).BazaarReceived(Network.LastSearchID, 1, Array.Empty<BazaarListing>());
            public void ReceiveMyList() => ((IViewModelNet)Login.Model).BazaarMyListReceived(Network.LastMyListID, 1, Array.Empty<BazaarListing>());

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
                client.Dispose();
            }
        }

        private sealed class FakeNetwork : INetworkService
        {
            public int MyListRequests;
            public uint LastMyListID, LastSearchID;
            public ushort LastItemType, LastPage;
            public void RequestBazaarMyList(uint requestID) { MyListRequests++; LastMyListID = requestID; }
            public void RequestBazaar(uint requestID, ushort itemType, ushort page)
            { LastSearchID = requestID; LastItemType = itemType; LastPage = page; }
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
        private static void Set(object target, string name, object value) => target.GetType().GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).SetValue(target, value);
        private static void SetProperty(object target, string name, object value) => target.GetType().GetProperty(name, BindingFlags.Instance | BindingFlags.Public).GetSetMethod(true).Invoke(target, new[] { value });
        private static void RaiseWorldChanged(C02WorldScreen world) => ((Action)typeof(C02WorldScreen).GetField("WorldStateChanged", BindingFlags.Instance | BindingFlags.NonPublic).GetValue(world))?.Invoke();
    }
}
