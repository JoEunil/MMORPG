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
                Assert.That(fixture.ActiveRowCount, Is.Zero, "The previous tab's late response must not populate this view.");
                Assert.That(fixture.Status.text, Is.EqualTo("Loading my listings…"));

                ((IViewModelNet)fixture.Login.Model).BazaarMyListReceived(myListId, 1,
                    new[] { Listing(91, 1), Listing(92, 2) });
                Assert.That(fixture.ActiveRowCount, Is.EqualTo(2));
                Assert.That(fixture.Row(91).Find("State").GetComponent<TMP_Text>().text, Is.EqualTo("Sold"));
                Assert.That(fixture.Row(92).Find("State").GetComponent<TMP_Text>().text, Is.EqualTo("Claimed"));
                Assert.That(fixture.Row(91).Find("Item").GetComponent<TMP_Text>().text, Is.EqualTo("Item #700"));
                Assert.That(fixture.Row(91).Find("Quantity").GetComponent<TMP_Text>().text, Is.EqualTo("2"));
                Assert.That(fixture.Row(91).Find("Price").GetComponent<TMP_Text>().text, Is.EqualTo("9,000"));
                fixture.Row(91).GetComponent<Button>().onClick.Invoke();
                Assert.That(fixture.Detail.text, Does.Contain("9,000 diamonds"));
                Assert.That(fixture.Detail.text, Does.Contain("Sold"));
                Assert.That(fixture.Detail.text, Does.Not.Contain("Available to claim"));
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
                Assert.That(fixture.ActiveRowCount, Is.Zero);
            }
        }

        [Test]
        public void MyListingsShowsEmptyStateAndRendersOneAndThirtyRows()
        {
            using (var fixture = new Fixture())
            {
                fixture.OpenMarket();
                fixture.MyListings.onClick.Invoke();
                fixture.ReceiveMyList();
                Assert.That(fixture.ActiveRowCount, Is.Zero);
                Assert.That(fixture.EmptyState.gameObject.activeSelf, Is.True);

                fixture.Search.onClick.Invoke();
                ((IViewModelNet)fixture.Login.Model).BazaarMyListReceived(fixture.Network.LastMyListID, 1, new[] { Listing(201, 0) });
                Assert.That(fixture.ActiveRowCount, Is.EqualTo(1));
                Assert.That(fixture.EmptyState.gameObject.activeSelf, Is.False);

                var rows = new BazaarListing[30];
                for (int i = 0; i < rows.Length; i++) rows[i] = Listing((ulong)(300 + i), (byte)(i % 3));
                fixture.Search.onClick.Invoke();
                ((IViewModelNet)fixture.Login.Model).BazaarMyListReceived(fixture.Network.LastMyListID, 1, rows);
                Assert.That(fixture.ActiveRowCount, Is.EqualTo(30));
                Assert.That(fixture.Row(329).Find("State").GetComponent<TMP_Text>().text, Is.EqualTo("Claimed"));
            }
        }

        [Test]
        [Category("MarketClaim")]
        public void CollectProceedsCorrelatesSelectionAndLeavesTimeoutUnresolvedWithoutRetry()
        {
            using (var fixture = new Fixture())
            {
                fixture.OpenMarket();
                Assert.That(fixture.Claim.gameObject.activeSelf, Is.False);
                fixture.MyListings.onClick.Invoke();
                ((IViewModelNet)fixture.Login.Model).BazaarMyListReceived(fixture.Network.LastMyListID, 1,
                    new[] { Listing(901, 1), Listing(902, 1) });
                Assert.That(fixture.Claim.interactable, Is.False, "A sold listing must be selected first.");
                fixture.Row(901).GetComponent<Button>().onClick.Invoke();
                Assert.That(fixture.Claim.interactable, Is.True);
                fixture.Claim.onClick.Invoke();
                fixture.Claim.onClick.Invoke();
                Assert.That(fixture.Network.ClaimRequests, Is.EqualTo(1));
                Assert.That(fixture.Network.LastClaimListingID, Is.EqualTo(901UL));
                Assert.That(fixture.Claim.interactable, Is.False);
                Assert.That(fixture.Status.text, Is.EqualTo("Collecting sale proceeds…"));

                uint firstRequestID = fixture.Network.LastClaimID;
                ((IViewModelNet)fixture.Login.Model).BazaarClaimReceived(firstRequestID, 902, 1, 9000);
                Assert.That(fixture.Status.text, Is.EqualTo("Collecting sale proceeds…"),
                    "A response for a different listing must not consume the pending selection.");
                ((IViewModelNet)fixture.Login.Model).BazaarClaimReceived(firstRequestID, 901, 1, 4294967296UL);
                Assert.That(fixture.Status.text, Is.EqualTo("Collected 4294967296 diamonds."));
                Assert.That(fixture.Claim.interactable, Is.False);
                fixture.Claim.onClick.Invoke();
                Assert.That(fixture.Network.ClaimRequests, Is.EqualTo(1), "A completed listing cannot be collected again in this session.");

                fixture.Row(902).GetComponent<Button>().onClick.Invoke();
                Assert.That(fixture.Claim.interactable, Is.True);
                fixture.Claim.onClick.Invoke();
                uint timedOutRequestID = fixture.Network.LastClaimID;
                Assert.That(fixture.Network.ClaimRequests, Is.EqualTo(2));
                Set(fixture.Screen, "requestDeadline", Time.unscaledTime - 1);
                typeof(C09BazaarScreen).GetMethod("Update", BindingFlags.Instance | BindingFlags.NonPublic)
                    .Invoke(fixture.Screen, null);
                Assert.That(fixture.Status.text, Does.Contain("result unknown"));
                Assert.That(fixture.Status.text, Does.Contain("no automatic retry"));
                Assert.That(fixture.Claim.interactable, Is.False);
                string unknownResult = fixture.Status.text;
                ((IViewModelNet)fixture.Login.Model).BazaarClaimReceived(timedOutRequestID, 902, 1, 9000);
                fixture.Claim.onClick.Invoke();
                typeof(C09BazaarScreen).GetMethod("Update", BindingFlags.Instance | BindingFlags.NonPublic)
                    .Invoke(fixture.Screen, null);
                Assert.That(fixture.Status.text, Is.EqualTo(unknownResult));
                Assert.That(fixture.Network.ClaimRequests, Is.EqualTo(2), "Timeout cancels presentation, not the DB transaction, and must not retry.");
            }
        }

        [Test]
        [Category("MarketPurchase")]
        public void PurchaseRequiresPriceConfirmationAndExplicitDeliveryAndDoesNotRetryUnknownResults()
        {
            using (var fixture = new Fixture())
            {
                fixture.OpenMarket();
                var first = Listing(1001, 0);
                first.price = 4294967296UL;
                ((IViewModelNet)fixture.Login.Model).BazaarReceived(fixture.Network.LastSearchID, 1,
                    new[] { first, Listing(1002, 0) });
                Assert.That(fixture.CheckOutbox.gameObject.activeSelf, Is.True);
                fixture.Row(1001).GetComponent<Button>().onClick.Invoke();
                fixture.Buy.onClick.Invoke();
                Assert.That(fixture.Network.BuyRequests, Is.Zero, "The first click must display a price confirmation without sending a purchase.");
                Assert.That(fixture.Status.text + fixture.Detail.text, Does.Contain("4,294,967,296"));
                fixture.Buy.onClick.Invoke();
                fixture.Buy.onClick.Invoke();
                Assert.That(fixture.Network.BuyRequests, Is.EqualTo(1));
                Assert.That(fixture.Network.LastBuyListingID, Is.EqualTo(1001UL));
                string pendingSelectionDetail = fixture.Detail.text;
                fixture.Row(1002).GetComponent<Button>().onClick.Invoke();
                Assert.That(fixture.Detail.text, Is.EqualTo(pendingSelectionDetail), "Selection cannot change while its purchase is pending.");
                Assert.That(fixture.Network.LastBuyListingID, Is.EqualTo(1001UL));

                ((IViewModelNet)fixture.Login.Model).BazaarBuyReceived(fixture.Network.LastBuyID, 1001, 1, 700, 2, 4294967297UL);
                Assert.That(fixture.Status.text, Does.Contain("Purchased"));
                Assert.That(fixture.Status.text, Does.Contain("4294967297"), "Display the server-confirmed u64 amount rather than the quoted price.");
                Assert.That(fixture.Status.text, Does.Contain("Receive purchases"));
                Assert.That(fixture.Network.CheckOutboxRequests, Is.Zero, "Purchase confirmation must not imply delivery or automatically request it.");
                fixture.Buy.onClick.Invoke();
                Assert.That(fixture.Network.BuyRequests, Is.EqualTo(1));

                fixture.CheckOutbox.onClick.Invoke();
                fixture.CheckOutbox.onClick.Invoke();
                Assert.That(fixture.Network.CheckOutboxRequests, Is.EqualTo(1));
                ((IViewModelNet)fixture.Login.Model).BazaarCheckOutboxReceived(fixture.Network.LastCheckOutboxID, 1, 2, 1, 1);
                Assert.That(fixture.Status.text, Does.Contain("Received 2"));
                Assert.That(fixture.Status.text, Does.Contain("already present 1"));
                Assert.That(fixture.Status.text, Does.Contain("pending 1"));
                Assert.That(fixture.Status.text.ToLowerInvariant(), Does.Contain("bag"), "A delivery pass can be partially reflected and must direct the player to inspect the bag.");

                fixture.Row(1002).GetComponent<Button>().onClick.Invoke();
                fixture.Buy.onClick.Invoke();
                Assert.That(fixture.Network.BuyRequests, Is.EqualTo(1));
                fixture.Buy.onClick.Invoke();
                Assert.That(fixture.Network.BuyRequests, Is.EqualTo(2));
                ((IViewModelNet)fixture.Login.Model).BazaarBuyReceived(fixture.Network.LastBuyID, 1002, 5, 0, 0, 0);
                Assert.That(fixture.Status.text.ToLowerInvariant(), Does.Contain("unknown"));
                Assert.That(fixture.Buy.interactable, Is.False);
                fixture.Buy.onClick.Invoke();
                fixture.Buy.onClick.Invoke();
                Assert.That(fixture.Network.BuyRequests, Is.EqualTo(2), "An unknown committed result must not trigger another purchase.");
                Assert.That(fixture.Network.CheckOutboxRequests, Is.EqualTo(1), "Delivery also remains an explicit action.");
            }
        }

        [Test, Category("MarketCancel")]
        public void CancellationConfirmsOwnedSaleAndSeparatesReturnWhileRejectingLateAndUnknownResults()
        {
            using (var fixture = new Fixture())
            {
                fixture.OpenMarket();
                Assert.That(fixture.CancelListing.gameObject.activeSelf, Is.False);
                fixture.MyListings.onClick.Invoke();
                ((IViewModelNet)fixture.Login.Model).BazaarMyListReceived(fixture.Network.LastMyListID, 1,
                    new[] { Listing(1101, 0), Listing(1102, 1), Listing(1103, 0), Listing(1104, 0) });
                Assert.That(fixture.CancelListing.interactable, Is.False);
                fixture.Row(1102).GetComponent<Button>().onClick.Invoke();
                Assert.That(fixture.CancelListing.interactable, Is.False, "Sold listings cannot be cancelled.");
                fixture.Row(1101).GetComponent<Button>().onClick.Invoke();
                fixture.CancelListing.onClick.Invoke();
                Assert.That(fixture.Network.CancelListingRequests, Is.Zero, "Confirmation must not send a cancellation.");
                Assert.That(fixture.Detail.text, Does.Contain("not refunded"));
                Assert.That(fixture.Detail.text, Does.Contain("separate"));
                fixture.CancelListing.onClick.Invoke();
                fixture.CancelListing.onClick.Invoke();
                Assert.That(fixture.Network.CancelListingRequests, Is.EqualTo(1));
                Assert.That(fixture.Network.LastCancelListing, Is.EqualTo(1101UL));
                string pendingDetail = fixture.Detail.text;
                fixture.Row(1103).GetComponent<Button>().onClick.Invoke();
                Assert.That(fixture.Detail.text, Is.EqualTo(pendingDetail));

                var handlerType = typeof(IViewModelUI).Assembly.GetType("ClientCore.Network.Handler", true);
                var handler = (ClientCore.Network.IHandlerSock)Activator.CreateInstance(handlerType, true);
                handlerType.GetMethod("Inject", BindingFlags.Instance | BindingFlags.NonPublic)
                    .Invoke(handler, new object[] { fixture.Login.Model, null });
                byte[] malformed = CancelListingFrame(fixture.Network.LastCancelID, 1101, 1);
                malformed[7] = 1;
                Assert.Throws<ArgumentException>(() => handler.HandleResponse(malformed));
                handler.HandleResponse(CancelListingFrame(fixture.Network.LastCancelID, 1103, 1));
                Assert.That(fixture.Status.text, Is.EqualTo("Cancelling sale…"));
                handler.HandleResponse(CancelListingFrame(fixture.Network.LastCancelID, 1101, 1));
                Assert.That(fixture.Status.text, Does.Contain("Sale cancelled"));
                Assert.That(fixture.Status.text, Does.Contain("check your bag"));
                Assert.That(fixture.Network.CheckOutboxRequests, Is.Zero, "A cancellation reply does not prove item return or request another delivery.");
                Assert.That(fixture.CancelListing.interactable, Is.False);
                fixture.CancelListing.onClick.Invoke();
                Assert.That(fixture.Network.CancelListingRequests, Is.EqualTo(1));

                fixture.Row(1103).GetComponent<Button>().onClick.Invoke();
                fixture.CancelListing.onClick.Invoke();
                fixture.CancelListing.onClick.Invoke();
                uint unknownID = fixture.Network.LastCancelID;
                handler.HandleResponse(CancelListingFrame(unknownID, 1103, 5));
                Assert.That(fixture.Status.text, Does.Contain("unknown"));
                fixture.CancelListing.onClick.Invoke();
                fixture.CancelListing.onClick.Invoke();
                Assert.That(fixture.Network.CancelListingRequests, Is.EqualTo(2));

                fixture.Row(1104).GetComponent<Button>().onClick.Invoke();
                fixture.CancelListing.onClick.Invoke();
                fixture.CancelListing.onClick.Invoke();
                uint lateID = fixture.Network.LastCancelID;
                fixture.Close.onClick.Invoke();
                handler.HandleResponse(CancelListingFrame(lateID, 1104, 1));
                Assert.That(fixture.Panel.activeSelf, Is.False);
                Assert.That(fixture.Status.text, Is.Empty, "A closed presentation must ignore a late cancellation reply.");
                Assert.That(fixture.Network.CancelListingRequests, Is.EqualTo(3));
            }
        }

        private static byte[] CancelListingFrame(uint requestID, ulong listingID, byte status)
        {
            var bytes = new byte[21];
            BitConverter.GetBytes((ushort)0xabcd).CopyTo(bytes, 0);
            BitConverter.GetBytes(21U).CopyTo(bytes, 2);
            bytes[6] = 37;
            BitConverter.GetBytes(requestID).CopyTo(bytes, 8);
            BitConverter.GetBytes(listingID).CopyTo(bytes, 12);
            bytes[20] = status;
            return bytes;
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
            public readonly Button Market, Search, Close, Next, SearchListings, MyListings, Claim, Buy, CheckOutbox, CancelListing;
            public readonly Button[] TypeButtons;
            public readonly Transform ScreenRows;
            public readonly TMP_Text EmptyState, Detail, Status;
            public int ActiveRowCount
            {
                get
                {
                    int count = 0;
                    for (int i = 0; i < ScreenRows.childCount; i++) if (ScreenRows.GetChild(i).gameObject.activeSelf) count++;
                    return count;
                }
            }
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
                Claim = ButtonObject("collect proceeds");
                Buy = ButtonObject("buy selection");
                CancelListing = ButtonObject("cancel sale");
                CheckOutbox = ButtonObject("receive purchases");
                TypeButtons = new[] { ButtonObject("type 1"), ButtonObject("type 2"), ButtonObject("type 3") };
                var previous = ButtonObject("previous");
                Next = ButtonObject("next");
                var rowsObject = Child("rows");
                ScreenRows = rowsObject.transform;
                ScreenRows.gameObject.AddComponent<VerticalLayoutGroup>();
                ScreenRows.gameObject.AddComponent<ContentSizeFitter>();
                EmptyState = Text("empty state");
                Detail = Text("selected detail");
                Status = Text("status");
                Set(Screen, "loginScreen", Login);
                Set(Screen, "worldScreen", World);
                Set(Screen, "bazaarPanel", Panel);
                Set(Screen, "marketButton", Market);
                Set(Screen, "searchListingsButton", SearchListings);
                Set(Screen, "myListingsButton", MyListings);
                Set(Screen, "claimButton", Claim);
                Set(Screen, "buyButton", Buy);
                Set(Screen, "cancelListingButton", CancelListing);
                Set(Screen, "checkOutboxButton", CheckOutbox);
                Set(Screen, "itemTypeButtons", TypeButtons);
                Set(Screen, "searchButton", Search);
                Set(Screen, "previousPageButton", previous);
                Set(Screen, "nextPageButton", Next);
                Set(Screen, "closeButton", Close);
                Set(Screen, "pageText", Text("page"));
                Set(Screen, "listingRows", ScreenRows);
                Set(Screen, "emptyStateText", EmptyState);
                Set(Screen, "selectedDetailText", Detail);
                Set(Screen, "statusText", Status);
                root.AddComponent<EventSystem>();

                var dataType = typeof(IViewModelUI).Assembly.GetType("ClientCore.ViewData", true);
                var data = Activator.CreateInstance(dataType, true);
                Network = new FakeNetwork();
                var modelType = typeof(IViewModelUI).Assembly.GetType("ClientCore.ViewModel", true);
                var model = (IViewModelUI)Activator.CreateInstance(modelType, BindingFlags.Instance | BindingFlags.NonPublic, null,
                    new object[] { Network, data, new NullLogger(), new InlineDispatcher() }, null);
                Set(model, "_worldReady", true);
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
            public Transform Row(ulong id) => ScreenRows.Find("Listing-" + id);
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
            public int MyListRequests, ClaimRequests, BuyRequests, CheckOutboxRequests, CancelListingRequests;
            public uint LastMyListID, LastSearchID, LastClaimID, LastBuyID, LastCheckOutboxID, LastCancelID;
            public ulong LastClaimListingID, LastBuyListingID, LastCancelListing;
            public void RequestBazaarCancelListing(uint requestID, ulong listingID)
            { CancelListingRequests++; LastCancelID = requestID; LastCancelListing = listingID; }
            public ushort LastItemType, LastPage;
            public void RequestBazaarMyList(uint requestID) { MyListRequests++; LastMyListID = requestID; }
            public void RequestBazaarClaim(uint requestID, ulong listingID)
            { ClaimRequests++; LastClaimID = requestID; LastClaimListingID = listingID; }
            public void RequestBazaarBuy(uint requestID, ulong listingID)
            { BuyRequests++; LastBuyID = requestID; LastBuyListingID = listingID; }
            public void RequestBazaarCheckOutbox(uint requestID)
            { CheckOutboxRequests++; LastCheckOutboxID = requestID; }
            public void RequestBazaar(uint requestID, ushort itemType, ushort page)
            { LastSearchID = requestID; LastItemType = itemType; LastPage = page; }
            public void RequestBazaarRegister(uint requestID, byte inventorySlot, ushort quantity, ulong totalDiamondPrice) { }
            public void RequestWallet(uint requestID) { }
            public Task Connect(string address, int port) => Task.CompletedTask;
            public void CharacterList() { }
            public void Enter(ulong characterId) { }
            public void Chat(string message, byte scope, ulong targetId) { }
            public void Profiles(uint[] profileIds) { }
            public void RequestInventory() { }
            public void Action(byte direction, float x, float y, byte skillSlot, ulong ownershipEpoch) { }
            public void Pong(ulong serverTimeMs) { }
            public void ZoneChange(byte op) { } public void ZoneResync(ulong ownershipEpoch) { } public void RespawnToTown(ulong ownershipEpoch) { }
            public void Log(string message) { }
        }

        private sealed class InlineDispatcher : IMainThreadDispatcher { public void Post(Action job) => job(); }
        private sealed class NullLogger : ClientCore.ILogger { public void Log(string message) { } }
        private static void Set(object target, string name, object value) => target.GetType().GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).SetValue(target, value);
        private static void SetProperty(object target, string name, object value) => target.GetType().GetProperty(name, BindingFlags.Instance | BindingFlags.Public).GetSetMethod(true).Invoke(target, new[] { value });
        private static void RaiseWorldChanged(C02WorldScreen world) => ((Action)typeof(C02WorldScreen).GetField("WorldStateChanged", BindingFlags.Instance | BindingFlags.NonPublic).GetValue(world))?.Invoke();
    }
}
