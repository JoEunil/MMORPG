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
                fixture.Receive(true, new InventoryItem { itemID = 1, quantity = 4, slot = 2 });
                Button knownCard = fixture.Cards[0];
                var itemIcon = knownCard.transform.Find("ItemAccent");
                var quantity = knownCard.transform.Find("ItemQuantity").GetComponent<TMP_Text>();
                Assert.That(fixture.Cards.Length, Is.EqualTo(1));
                UnityEngine.UI.LayoutRebuilder.ForceRebuildLayoutImmediate(fixture.Grid);
                Assert.That(((RectTransform)knownCard.transform).rect.size, Is.EqualTo(new Vector2(120, 110)));
                Assert.That(((RectTransform)itemIcon).sizeDelta, Is.EqualTo(new Vector2(56, 56)));
                Assert.That(quantity.text, Is.EqualTo("4"));
                Assert.That(itemIcon.GetComponent<UnityEngine.UI.Image>().sprite, Is.SameAs(C11ItemPresentation.Icon(1)));
                knownCard.onClick.Invoke();
                Assert.That(knownCard.GetComponent<UnityEngine.UI.Image>().color, Is.EqualTo(new Color(.38f, .30f, .20f, 1f)));
                Assert.That(knownCard.transition, Is.EqualTo(Selectable.Transition.None));
                Assert.That(((RectTransform)fixture.DetailBadge.rectTransform.GetChild(0)).rect.size, Is.EqualTo(new Vector2(96, 96)));

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
        [Category("PR55Followup")]
        public void RefreshRestoresSelectionOnlyForTheSameSlotAndItem()
        {
            using (var fixture = new Fixture())
            {
                fixture.OpenBag();
                fixture.Receive(true, new InventoryItem { itemID = 1, quantity = 3, slot = 2 });
                fixture.Cards[0].onClick.Invoke();
                Assert.That(fixture.DetailQuantity.text, Is.EqualTo("Quantity 3"));

                fixture.Refresh.onClick.Invoke();
                Assert.That(fixture.Cards, Is.Empty, "A pending read clears stale cards and details.");
                Assert.That(fixture.DetailName.text, Is.EqualTo("Select an item"));
                fixture.Receive(true, new InventoryItem { itemID = 1, quantity = 8, slot = 2 });
                Assert.That(fixture.Cards[0].GetComponent<Image>().color, Is.EqualTo(new Color(.38f, .30f, .20f, 1f)));
                Assert.That(fixture.DetailName.text, Is.EqualTo("HP Potion"));
                Assert.That(fixture.DetailQuantity.text, Is.EqualTo("Quantity 8"));

                fixture.Refresh.onClick.Invoke();
                fixture.Close.onClick.Invoke();
                fixture.Receive(true, new InventoryItem { itemID = 1, quantity = 9, slot = 2 });
                Assert.That(fixture.Panel.activeSelf, Is.False, "A closed bag stays closed when its response arrives.");
                Assert.That(fixture.DetailQuantity.text, Is.EqualTo("Quantity 9"));
                fixture.Bag.onClick.Invoke();

                fixture.Refresh.onClick.Invoke();
                fixture.Receive(true, new InventoryItem { itemID = 2, quantity = 6, slot = 2 });
                Assert.That(fixture.Cards[0].GetComponent<Image>().color, Is.EqualTo(new Color(.16f, .18f, .20f, 1f)));
                Assert.That(fixture.DetailName.text, Is.EqualTo("Select an item"), "A slot reused for another item must not inherit selection.");
                Assert.That(fixture.DetailQuantity.text, Is.Empty);

                fixture.Cards[0].onClick.Invoke();
                fixture.Refresh.onClick.Invoke();
                fixture.Receive(true);
                Assert.That(fixture.Cards, Is.Empty, "A removed item clears the selected details.");
                Assert.That(fixture.DetailName.text, Is.EqualTo("Select an item"));
                fixture.Refresh.onClick.Invoke();
                fixture.Receive(true, new InventoryItem { itemID = 2, quantity = 6, slot = 2 });
                Assert.That(fixture.DetailName.text, Is.EqualTo("Select an item"), "A reappearing item is not automatically selected.");
                fixture.Cards[0].onClick.Invoke();
                fixture.Refresh.onClick.Invoke();
                fixture.Receive(false);
                Assert.That(fixture.Cards, Is.Empty);
                Assert.That(fixture.DetailName.text, Is.EqualTo("Select an item"), "A failed read clears selected details.");
                Assert.That(fixture.DetailSlot.text, Is.Empty);
                SetPrivate(fixture.Screen, "failureRetryDeadline", Time.unscaledTime - 1f);
                fixture.InvokeScreen("Update");
                fixture.Receive(true, new InventoryItem { itemID = 2, quantity = 5, slot = 2 });
                Assert.That(fixture.Cards[0].GetComponent<Image>().color, Is.EqualTo(new Color(.16f, .18f, .20f, 1f)),
                    "A later retry does not revive a selection cleared by failure.");
                Assert.That(fixture.DetailName.text, Is.EqualTo("Select an item"));
            }
        }

        [Test]
        public void FailureRetriesOnceThenShowsTheServerConfirmedEmptyInventoryOrStopsAfterASecondFailure()
        {
            using (var fixture = new Fixture())
            {
                fixture.OpenBag();
                fixture.Receive(false);
                Assert.That(fixture.Network.Requests, Is.EqualTo(1));
                Assert.That(fixture.Status.text, Does.Contain("Checking once more"));

                SetPrivate(fixture.Screen, "failureRetryDeadline", Time.unscaledTime - 1f);
                fixture.InvokeScreen("Update");
                Assert.That(fixture.Network.Requests, Is.EqualTo(2), "The scheduled retry sends exactly one real inventory request.");
                fixture.Receive(true);
                Assert.That(fixture.Status.text, Is.EqualTo("No items."), "Only the successful empty response is shown as an empty bag.");
                Assert.That(fixture.Cards, Is.Empty);

                fixture.Refresh.onClick.Invoke();
                Assert.That(fixture.Network.Requests, Is.EqualTo(3), "Manual refresh starts a new one-retry budget.");
                fixture.Receive(false);
                SetPrivate(fixture.Screen, "failureRetryDeadline", Time.unscaledTime - 1f);
                fixture.InvokeScreen("Update");
                Assert.That(fixture.Network.Requests, Is.EqualTo(4));
                fixture.Receive(false);
                Assert.That(fixture.Status.text, Is.EqualTo("Inventory request failed. Select Refresh to try again."));
                fixture.InvokeScreen("Update");
                Assert.That(fixture.Network.Requests, Is.EqualTo(4), "A second failure does not schedule another request.");
            }
        }

        [Test]
        public void ClosingOrReplacingTheClientCancelsTheScheduledFailureRetryAndIgnoresOldReplies()
        {
            using (var fixture = new Fixture())
            {
                fixture.OpenBag();
                IViewModelUI originalModel = fixture.Login.Model;
                fixture.Receive(false);
                fixture.Close.onClick.Invoke();
                SetPrivate(fixture.Screen, "failureRetryDeadline", Time.unscaledTime - 1f);
                fixture.InvokeScreen("Update");
                Assert.That(fixture.Network.Requests, Is.EqualTo(1), "Closing the bag cancels its scheduled retry.");

                fixture.OpenBag();
                fixture.Receive(false);
                var replacement = fixture.CreateClient();
                SetPrivate(fixture.Login, "client", replacement.Initializer);
                fixture.RaiseClientChanged(replacement.Model);
                fixture.SetWorldReady();
                ((IViewModelNet)originalModel).InventoryReceived(0, Array.Empty<InventoryItem>());
                SetPrivate(fixture.Screen, "failureRetryDeadline", Time.unscaledTime - 1f);
                fixture.InvokeScreen("Update");
                Assert.That(fixture.Network.Requests, Is.EqualTo(2), "Replacing the client cancels the retry and an old-model reply is ignored.");

                fixture.OpenBag();
                Assert.That(replacement.Network.Requests, Is.EqualTo(1));
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
                Assert.That(fixture.Cards[0].transform.Find("ItemQuantity").GetComponent<TMP_Text>().text, Is.EqualTo("4"));
                Assert.That(fixture.Cards[1].transform.Find("ItemName").GetComponent<TMP_Text>().text, Is.EqualTo("Leather Armor"));
                fixture.Cards[0].onClick.Invoke();
                Assert.That(fixture.DetailBadge.text, Is.Empty);
                Assert.That(C11ItemPresentation.Icon(1), Is.Not.Null, "The authored potion must import as a Sprite.");
                Assert.That(fixture.Cards[0].GetComponentsInChildren<Image>(true),
                    Has.Some.Matches<Image>(image => image.sprite == C11ItemPresentation.Icon(1)));
                var detailIcon = fixture.DetailBadge.GetComponentInChildren<Image>(true);
                Assert.That(detailIcon, Is.Not.Null);
                Assert.That(detailIcon.gameObject, Is.Not.SameAs(fixture.DetailBadge.gameObject));
                Assert.That(detailIcon.sprite, Is.SameAs(C11ItemPresentation.Icon(1)));
                Assert.That(detailIcon.enabled, Is.True);
                var distinctIcons = new System.Collections.Generic.HashSet<Sprite>();
                for (uint itemID = 1; itemID <= 5; itemID++)
                {
                    var imported = C11ItemPresentation.Icon(itemID);
                    Assert.That(imported, Is.Not.Null, "Each supported server item needs its own imported Sprite.");
                    Assert.That(imported.texture.width, Is.LessThanOrEqualTo(256));
                    distinctIcons.Add(imported);
                }
                Assert.That(distinctIcons.Count, Is.EqualTo(5), "Different item types must not reuse one icon.");
                Assert.That(C11ItemPresentation.Icon(0), Is.Null);
                Assert.That(C11ItemPresentation.Icon(42), Is.Null, "Unknown IDs must keep their explicit fallback.");
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

        [Test, Category("MarketCancel")]
        public void UnknownReturnKeepsManualBagOpensFreshEvenAfterAnEarlierReadCompletes()
        {
            using (var fixture = new Fixture())
            {
                fixture.OpenBag();
                fixture.Receive(true);
                fixture.Close.onClick.Invoke();
                fixture.Screen.RequireFreshInventoryOnOpen();
                fixture.OpenBag();
                Assert.That(fixture.Network.Requests, Is.EqualTo(2));
                fixture.Close.onClick.Invoke();
                fixture.Receive(true); // This read can predate the still-running cancellation's return.
                fixture.OpenBag();
                Assert.That(fixture.Network.Requests, Is.EqualTo(3), "A pre-return response must not restore the stale-cache shortcut.");
                fixture.Receive(true, new InventoryItem { itemID = 1, quantity = 2, slot = 0 });
                Assert.That(fixture.Cards.Length, Is.EqualTo(1));
                fixture.Cards[0].onClick.Invoke();
                Assert.That(fixture.DetailName.text, Is.EqualTo("HP Potion"));
                Assert.That(fixture.DetailQuantity.text, Is.EqualTo("Quantity 2"));
            }
        }

        [Test]
        [Category("MarketPurchase")]
        public void DeliveryRefreshesCachedInventoryAndWaitsForAnOlderReadBeforeOneFreshRequest()
        {
            using (var fixture = new Fixture())
            {
                fixture.OpenBag();
                fixture.Receive(true, new InventoryItem { itemID = 1, quantity = 3, slot = 0 });
                Assert.That(fixture.Network.Requests, Is.EqualTo(1));
                Assert.That(fixture.Cards[0].transform.Find("ItemName").GetComponent<TMP_Text>().text, Is.EqualTo("HP Potion"));
                fixture.Close.onClick.Invoke();

                fixture.Screen.RefreshAfterDelivery();
                Assert.That(fixture.Network.Requests, Is.EqualTo(2), "Delivery invalidates a previously viewed bag and reads fresh inventory even while closed.");
                Assert.That(fixture.Cards, Is.Empty, "Old cards cannot remain the delivery result.");
                fixture.OpenBag();
                Assert.That(fixture.Network.Requests, Is.EqualTo(2), "Opening during the delivery refresh must not issue another uncorrelated inventory read.");
                fixture.Receive(true, new InventoryItem { itemID = 2, quantity = 7, slot = 0 });
                Assert.That(fixture.Cards.Length, Is.EqualTo(1));
                fixture.Cards[0].onClick.Invoke();
                Assert.That(fixture.DetailName.text, Is.EqualTo("MP Potion"));
                Assert.That(fixture.DetailQuantity.text, Is.EqualTo("Quantity 7"));

                fixture.Refresh.onClick.Invoke();
                Assert.That(fixture.Network.Requests, Is.EqualTo(3));
                fixture.Screen.RefreshAfterDelivery();
                fixture.Screen.RefreshAfterDelivery();
                Assert.That(fixture.Network.Requests, Is.EqualTo(3), "Delivery notifications must wait for the older inventory response instead of overlapping reads.");
                fixture.Receive(true, new InventoryItem { itemID = 1, quantity = 1, slot = 0 });
                Assert.That(fixture.Network.Requests, Is.EqualTo(4), "Repeated delivery notifications coalesce into exactly one fresh read after the old response.");
                Assert.That(fixture.Cards, Is.Empty, "The older response must not become the displayed post-delivery inventory.");
                fixture.Receive(true, new InventoryItem { itemID = 5, quantity = 2, slot = 1 });
                Assert.That(fixture.Network.Requests, Is.EqualTo(4));
                Assert.That(fixture.Cards.Length, Is.EqualTo(1));
                fixture.Cards[0].onClick.Invoke();
                Assert.That(fixture.DetailName.text, Is.EqualTo("Leather Armor"));
                Assert.That(fixture.DetailQuantity.text, Is.EqualTo("Quantity 2"));
                Assert.That(fixture.Refresh.interactable, Is.True);
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
                gridObject.AddComponent<GridLayoutGroup>().cellSize = new Vector2(120, 110);
                scroll.content = Grid;
                DetailName = Text("detail name");
                DetailBadge = Text("detail badge");
                DetailBadge.rectTransform.sizeDelta = new Vector2(96, 96);
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
                SetPrivate(model, "_worldReady", true);
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
            public void RequestBazaarBuy(uint requestID, ulong listingID) { }
            public void RequestBazaarCheckOutbox(uint requestID) { } public void RequestBazaarCancelListing(uint requestID, ulong listingID) { }
            public void RequestBazaarClaim(uint requestID, ulong listingID) { }
            public void RequestBazaarMyList(uint requestID) { }
            public void RequestBazaar(uint requestID, ushort itemType, ushort page) { }
            public void RequestBazaarRegister(uint requestID, byte inventorySlot, ushort quantity, ulong totalDiamondPrice) { }
            public void RequestWallet(uint requestID) { }
            public void RequestInventory() => Requests++;
            public Task Connect(string address, int port) => Task.CompletedTask;
            public void CharacterList() { }
            public void Enter(ulong characterId) { }
            public void Chat(string message, byte scope, ulong targetId) { }
            public void Profiles(uint[] profileIds) { }
            public void Action(byte direction, float x, float y, byte skillSlot, ulong ownershipEpoch) { }
            public void Pong(ulong serverTimeMs) { }
            public void ZoneChange(byte op) { } public void ZoneResync(ulong ownershipEpoch) { } public void RespawnToTown(ulong ownershipEpoch) { }
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
