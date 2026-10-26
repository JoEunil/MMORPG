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
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.LowLevel;
using UnityEngine.EventSystems;
using UnityEngine.UI;

namespace C02.Tests
{
    public sealed class C09BazaarUiTests
    {
        [Test]
        [Category("CurrencySkillIcons")]
        public void ListingRowsUseAssignedDiamondSpriteWithoutBlockingSelection()
        {
            var icon = Sprite.Create(Texture2D.whiteTexture, new Rect(0, 0, 2, 2), new Vector2(.5f, .5f));
            try
            {
                using (var fixture = new Fixture())
                {
                    Set(fixture.Screen, "diamondCurrencyIcon", icon);
                    fixture.OpenMarket();
                    fixture.Receive(true, new BazaarListing { listingID = 77, itemID = 1, quantity = 1, price = 20 });
                    var row = fixture.Row(77);
                    var mark = row.Find("DiamondMark").GetComponent<UnityEngine.UI.Image>();
                    Assert.That(mark.sprite, Is.SameAs(icon));
                    Assert.That(mark.color, Is.EqualTo(Color.white));
                    Assert.That(mark.preserveAspect, Is.True);
                    Assert.That(mark.rectTransform.sizeDelta, Is.EqualTo(new Vector2(16, 16)));
                    Assert.That(Quaternion.Angle(mark.rectTransform.localRotation, Quaternion.identity), Is.LessThan(.001f));
                    Assert.That(mark.raycastTarget, Is.False);
                    row.GetComponent<UnityEngine.UI.Button>().onClick.Invoke();
                    Assert.That(fixture.Detail.text, Is.EqualTo("20 diamonds · Trading"));
                }
            }
            finally { UnityEngine.Object.DestroyImmediate(icon); }
        }

        [Test]
        [Category("MarketSelectionPolish")]
        public void SelectingListingsClearsOnlyPendingConfirmationAndKeepsTransactionResults()
        {
            foreach (bool cancellation in new[] { false, true })
                using (var fixture = new Fixture())
                {
                    fixture.OpenMarket();
                    if (cancellation) { fixture.Receive(true); fixture.MyListings.onClick.Invoke(); }
                    var listings = new[] {
                        new BazaarListing { listingID = 100, itemID = 1, quantity = 1, price = 100 },
                        new BazaarListing { listingID = 900, itemID = 1, quantity = 2, price = 900 }
                    };
                    var net = (IViewModelNet)fixture.Login.Model;
                    if (cancellation) net.BazaarMyListReceived(fixture.Network.LastRequestID, 1, listings);
                    else fixture.Receive(true, listings);
                    var action = cancellation ? fixture.CancelListing : fixture.Buy;
                    string confirmationField = cancellation ? "cancelConfirmation" : "buyConfirmation";
                    fixture.Row(100).GetComponent<Button>().onClick.Invoke();
                    action.onClick.Invoke();
                    Assert.That(Get<bool>(fixture.Screen, confirmationField), Is.True);
                    fixture.Row(900).GetComponent<Button>().onClick.Invoke();
                    Assert.That(Get<bool>(fixture.Screen, confirmationField), Is.False);
                    Assert.That(fixture.Status.text, Is.EqualTo("Selected listing: 900 diamonds · Trading"));

                    action.onClick.Invoke();
                    Assert.That(Get<bool>(fixture.Screen, confirmationField), Is.True);
                    Assert.That(fixture.Status.text, Does.Contain(cancellation ? "Press again to cancel" : "deduct 900 diamonds"));
                    Assert.That(fixture.Network.Mutations, Is.Zero);
                    Assert.That(fixture.Network.CancelListingRequests, Is.Zero);
                    fixture.Row(900).GetComponent<Button>().onClick.Invoke();
                    Assert.That(Get<bool>(fixture.Screen, confirmationField), Is.False,
                        "Reselecting the same row also cancels its pending confirmation.");
                    Assert.That(fixture.Status.text, Is.EqualTo("Selected listing: 900 diamonds · Trading"));

                    action.onClick.Invoke();
                    Assert.That(fixture.Network.Mutations, Is.Zero);
                    Assert.That(fixture.Network.CancelListingRequests, Is.Zero);
                    action.onClick.Invoke();
                    if (cancellation) net.BazaarCancelListingReceived(fixture.Network.LastWriteID, 900, 0);
                    else net.BazaarBuyReceived(fixture.Network.LastWriteID, 900, 4, 0, 0, 0);
                    string failure = fixture.Status.text;
                    Assert.That(failure, Does.Contain(cancellation ? "Sale could not be cancelled" : "Not enough diamonds"));
                    Assert.That(Get<bool>(fixture.Screen, "requestPending"), Is.False);
                    fixture.Row(100).GetComponent<Button>().onClick.Invoke();
                    Assert.That(fixture.Status.text, Is.EqualTo(failure), "Selecting a row preserves an actual failure result.");

                    action.onClick.Invoke(); action.onClick.Invoke();
                    if (cancellation) net.BazaarCancelListingReceived(fixture.Network.LastWriteID, 100, 1);
                    else net.BazaarBuyReceived(fixture.Network.LastWriteID, 100, 1, 1, 1, 100);
                    string completion = fixture.Status.text;
                    Assert.That(completion, Does.Contain(cancellation ? "Sale cancelled." : "Purchased 1"));
                    Assert.That(Get<bool>(fixture.Screen, "requestPending"), Is.False);
                    fixture.Row(900).GetComponent<Button>().onClick.Invoke();
                    Assert.That(fixture.Status.text, Is.EqualTo(completion), "Selecting a row preserves an actual completed result.");
                    Assert.That(cancellation ? fixture.Network.CancelListingRequests : fixture.Network.Mutations, Is.EqualTo(2));
                }
        }

        [Test]
        [Category("MarketSelectionPolish")]
        public void SaleSlotSelectionHighlightsOneRowAndClearsWithItsRows()
        {
            using (var fixture = new Fixture())
            {
                fixture.OpenMarket();
                fixture.Receive(true, new BazaarListing { listingID = 77, itemID = 4, quantity = 1, price = 20 });
                fixture.SellItems.onClick.Invoke();
                fixture.ReceiveInventory(
                    new InventoryItem { itemID = 4, quantity = 3, slot = 2 },
                    new InventoryItem { itemID = 4, quantity = 8, slot = 7 });
                var first = fixture.Rows.GetChild(fixture.Rows.childCount - 2).GetComponent<Button>();
                var second = fixture.Rows.GetChild(fixture.Rows.childCount - 1).GetComponent<Button>();
                var firstColor = first.GetComponent<Image>().color;
                var selectedColor = new Color(.20f, .29f, .34f);
                first.onClick.Invoke();
                Assert.That(first.GetComponent<Image>().color, Is.EqualTo(selectedColor));
                fixture.Price.text = "95";
                fixture.Register.onClick.Invoke();
                Assert.That(Get<bool>(fixture.Screen, "registerConfirmation"), Is.True);
                second.onClick.Invoke();
                Assert.That(first.GetComponent<Image>().color, Is.EqualTo(firstColor));
                Assert.That(second.GetComponent<Image>().color, Is.EqualTo(selectedColor));
                int highlighted = 0;
                foreach (Transform row in fixture.Rows)
                    if (row.gameObject.activeSelf && row.GetComponent<Image>().color == selectedColor) highlighted++;
                Assert.That(highlighted, Is.EqualTo(1));
                Assert.That(Get<Button>(fixture.Screen, "selectedRowButton"), Is.SameAs(second));
                Assert.That(fixture.Detail.text, Is.EqualTo("Slot 7 selected"));
                Assert.That(fixture.SelectedQuantity.text, Is.EqualTo("× 8"));
                Assert.That(Get<bool>(fixture.Screen, "registerConfirmation"), Is.False);
                fixture.Quantity.text = "2";
                fixture.Register.onClick.Invoke();
                Assert.That(fixture.Network.RegisterRequests, Is.Zero);
                fixture.Register.onClick.Invoke();
                Assert.That(fixture.Network.LastRegisterSlot, Is.EqualTo((byte)7));
                Assert.That(fixture.Network.LastRegisterQuantity, Is.EqualTo((ushort)2));
                Assert.That(fixture.Network.LastRegisterPrice, Is.EqualTo(95UL));

                var net = (IViewModelNet)fixture.Login.Model;
                net.BazaarRegisterReceived(fixture.Network.LastWriteID, 0);
                fixture.Search.onClick.Invoke();
                Assert.That(Get<Button>(fixture.Screen, "selectedRowButton"), Is.Null);
                Assert.That(Get<InventoryItemView>(fixture.Screen, "saleItem").ItemID, Is.Zero);
                Assert.That(second.gameObject.activeSelf, Is.False);
                net.InventoryReceived(0, Array.Empty<InventoryItem>());
                Assert.That(fixture.ActiveRowCount, Is.Zero);
                Assert.That(Get<Button>(fixture.Screen, "selectedRowButton"), Is.Null);
                Assert.That(fixture.Register.interactable, Is.False);

                fixture.Search.onClick.Invoke();
                fixture.ReceiveInventory(new InventoryItem { itemID = 4, quantity = 5, slot = 9 });
                var refreshed = fixture.Rows.GetChild(fixture.Rows.childCount - 1).GetComponent<Button>();
                refreshed.onClick.Invoke();
                Assert.That(fixture.Detail.text, Is.EqualTo("Slot 9 selected"));
                Assert.That(refreshed.GetComponent<Image>().color, Is.EqualTo(selectedColor));
                fixture.MyListings.onClick.Invoke();
                Assert.That(Get<Button>(fixture.Screen, "selectedRowButton"), Is.Null);
                Assert.That(Get<InventoryItemView>(fixture.Screen, "saleItem").ItemID, Is.Zero);
                Assert.That(refreshed.gameObject.activeSelf, Is.False);
                fixture.Receive(true, new BazaarListing { listingID = 77, itemID = 4, quantity = 1, price = 20 });
                fixture.Row(77).GetComponent<Button>().onClick.Invoke();
                fixture.Close.onClick.Invoke();
                Assert.That(Get<Button>(fixture.Screen, "selectedRowButton"), Is.Null);
                Assert.That(fixture.ActiveRowCount, Is.Zero);
                Assert.That(fixture.Network.RegisterRequests, Is.EqualTo(1));
            }
        }

        [Test]
        [Category("InventoryMarketHandoff")]
        public void InventoryReadsShareAcrossBothPanelDirectionsAndWaitForFreshDeliveryResult()
        {
            foreach (bool inventoryFirst in new[] { false, true })
                using (var fixture = new Fixture())
                {
                    fixture.ActivateInventory(inventoryFirst);
                    fixture.Bag.onClick.Invoke();
                    Assert.That(fixture.Network.InventoryRequests, Is.EqualTo(1));
                    fixture.OpenMarket(); fixture.Receive(true); fixture.SellItems.onClick.Invoke();
                    Assert.That(fixture.Network.InventoryRequests, Is.EqualTo(1), "The selling view joins the existing bag read.");
                    fixture.ReceiveInventory(new InventoryItem { itemID = 1, quantity = 3, slot = 2 });
                    Invoke(fixture.Screen, "Update");
                    Assert.That(fixture.ActiveRowCount, Is.EqualTo(1));
                    Assert.That(fixture.FirstSaleQuantity, Is.EqualTo("3"));
                    Assert.That(Get<bool>(fixture.Screen, "requestPending"), Is.False);

                    fixture.Close.onClick.Invoke();
                    fixture.OpenMarket(); fixture.Receive(true); fixture.SellItems.onClick.Invoke();
                    Assert.That(fixture.Network.InventoryRequests, Is.EqualTo(2));
                    fixture.Bag.onClick.Invoke();
                    Assert.That(fixture.Network.InventoryRequests, Is.EqualTo(2), "Opening the bag joins the sale view's read too.");
                    fixture.ReceiveInventory(new InventoryItem { itemID = 1, quantity = 6, slot = 2 });
                    Assert.That(fixture.InventoryQuantity, Is.EqualTo("6"));
                    Assert.That(fixture.InventoryScreen.IsOpen, Is.True);
                    Assert.That(fixture.Panel.activeSelf, Is.False);

                    fixture.InventoryScreen.ClosePanel();
                    fixture.OpenMarket(); fixture.Receive(true); fixture.SellItems.onClick.Invoke();
                    Assert.That(fixture.Network.InventoryRequests, Is.EqualTo(3));
                    fixture.InventoryScreen.RefreshAfterDelivery();
                    ((IViewModelNet)fixture.Login.Model).InventoryReceived(0, Array.Empty<InventoryItem>());
                    Invoke(fixture.Screen, "Update");
                    Assert.That(fixture.Network.InventoryRequests, Is.EqualTo(4), "The delivery follow-up survives the older read's failure.");
                    Assert.That(Get<bool>(fixture.Screen, "requestPending"), Is.True);
                    Assert.That(fixture.ActiveRowCount, Is.Zero);
                    fixture.ReceiveInventory(new InventoryItem { itemID = 1, quantity = 9, slot = 2 });
                    Invoke(fixture.Screen, "Update");
                    Assert.That(fixture.FirstSaleQuantity, Is.EqualTo("9"));
                    Assert.That(Get<bool>(fixture.Screen, "requestPending"), Is.False);
                    Assert.That(fixture.Network.InventoryRequests, Is.EqualTo(4));
                }
        }

        [Test]
        [Category("InventoryMarketHandoff")]
        public void ClosedItemMutationsRefreshAnAlreadyOpenBagWithoutRepeatingTheMutation()
        {
            foreach (bool cancellation in new[] { false, true })
                using (var fixture = new Fixture())
                {
                    fixture.ActivateInventory(false);
                    fixture.OpenMarket(); fixture.Receive(true);
                    var net = (IViewModelNet)fixture.Login.Model;
                    if (cancellation)
                    {
                        fixture.MyListings.onClick.Invoke();
                        net.BazaarMyListReceived(fixture.Network.LastRequestID, 1,
                            new[] { new BazaarListing { listingID = 77, itemID = 1, quantity = 1, price = 20, status = 0 } });
                        fixture.Row(77).GetComponent<Button>().onClick.Invoke();
                        fixture.CancelListing.onClick.Invoke(); fixture.CancelListing.onClick.Invoke();
                    }
                    else fixture.CheckOutbox.onClick.Invoke();
                    uint request = cancellation ? fixture.Network.LastWriteID : fixture.Network.LastCheckOutboxRequestID;
                    Assert.That(cancellation ? fixture.Network.CancelListingRequests : fixture.Network.CheckOutboxRequests, Is.EqualTo(1));
                    fixture.Close.onClick.Invoke(); fixture.Close.onClick.Invoke();
                    Assert.That(Get<bool>(fixture.Screen, "requestPending"), Is.True);
                    fixture.Bag.onClick.Invoke();
                    fixture.ReceiveInventory(new InventoryItem { itemID = 1, quantity = 1, slot = 2 });
                    Assert.That(fixture.InventoryQuantity, Is.EqualTo("1"));
                    Assert.That(fixture.Network.InventoryRequests, Is.EqualTo(1));
                    if (cancellation) net.BazaarCancelListingReceived(request, 77, 1);
                    else net.BazaarCheckOutboxReceived(request, 1, 1, 0, 0);
                    Assert.That(fixture.Network.InventoryRequests, Is.EqualTo(2), "The completed write invalidates the bag even after its first read finished.");
                    fixture.ReceiveInventory(new InventoryItem { itemID = 1, quantity = 2, slot = 2 });
                    Assert.That(fixture.InventoryQuantity, Is.EqualTo("2"));
                    Assert.That(fixture.InventoryScreen.IsOpen, Is.True);
                    Assert.That(fixture.Panel.activeSelf, Is.False);
                    Assert.That(Get<bool>(fixture.Screen, "requestPending"), Is.False);
                    if (cancellation) net.BazaarCancelListingReceived(request, 77, 1);
                    else net.BazaarCheckOutboxReceived(request, 1, 1, 0, 0);
                    Assert.That(fixture.Network.InventoryRequests, Is.EqualTo(2), "A stale duplicate completion is ignored.");
                    Assert.That(cancellation ? fixture.Network.CancelListingRequests : fixture.Network.CheckOutboxRequests, Is.EqualTo(1));
                }
        }

        [Test]
        public void SearchRendersThirtyRealRowsAndEmptySuccessHasNoInventedRows()
        {
            using (var fixture = new Fixture())
            {
                fixture.OpenMarket();
                fixture.ReceiveWallet(fixture.Network.LastWalletRequestID, 1, 100, 1, 0);
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
                Assert.That(lastRow.Find("Price").GetComponent<TMP_Text>().text, Is.EqualTo("9,029"));
                Assert.That(lastRow.Find("DiamondMark").GetComponent<Image>(), Is.Not.Null);
                Assert.That(lastRow.Find("State").GetComponent<TMP_Text>().text, Is.EqualTo("Trading"));
                Assert.That(lastRow.Find("StateBadge").GetComponent<Image>(), Is.Not.Null);
                Assert.That(lastRow.GetComponent<UnityEngine.UI.LayoutElement>().preferredHeight, Is.EqualTo(74));
                lastRow.GetComponent<Button>().onClick.Invoke();
                Assert.That(lastRow.GetComponent<UnityEngine.UI.Image>().color, Is.EqualTo(new Color(.20f, .29f, .34f)));
                Assert.That(fixture.SelectedName.text, Is.EqualTo("Item #729"));
                Assert.That(fixture.SelectedQuantity.text, Is.EqualTo("× 30"));
                Assert.That(fixture.Detail.text, Does.Contain("9,029 diamonds"));
                Assert.That(fixture.Detail.text, Does.Contain("buyer pays"));
                fixture.Row(1000).GetComponent<Button>().onClick.Invoke();
                Assert.That(lastRow.GetComponent<UnityEngine.UI.Image>().color, Is.EqualTo(new Color(.09f, .11f, .14f)));
                Assert.That(fixture.Row(1000).GetComponent<UnityEngine.UI.Image>().color, Is.EqualTo(new Color(.20f, .29f, .34f)));
                Assert.That(fixture.Detail.text, Does.Contain("9,000 diamonds"));

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

        [Test]
        public void WalletReadRecovery_StartupFailuresAndTimeoutsRetryWithoutReplayingWrites()
        {
            using (var fixture = new Fixture())
            {
                fixture.ReceiveWallet(fixture.Network.LastWalletRequestID, 1, 100, 1, 200);
                fixture.Network.ThrowWalletRequest = true;
                Invoke(fixture.Screen, "RefreshWallet");
                Assert.That(fixture.WalletGold.text, Is.EqualTo("--"));
                Set(fixture.Screen, "walletRetryAt", Time.unscaledTime - .001f);
                Invoke(fixture.Screen, "Update");
                fixture.Network.ThrowWalletRequest = false;
                Set(fixture.Screen, "walletRetryAt", Time.unscaledTime - .001f);
                Invoke(fixture.Screen, "Update");
                fixture.ReceiveWallet(fixture.Network.LastWalletRequestID, 1, 90, 1, 150);
                Assert.That(fixture.WalletGold.text, Is.EqualTo("90"));
                Assert.That(fixture.WalletDiamond.text, Is.EqualTo("150"));
                Assert.That(fixture.Network.WalletRequests, Is.EqualTo(4));
                Assert.That(fixture.Network.Mutations, Is.Zero);
            }

            using (var fixture = new Fixture())
            {
                uint expired = fixture.Network.LastWalletRequestID;
                Set(fixture.Screen, "walletDeadline", Time.unscaledTime - 1);
                Invoke(fixture.Screen, "Update");
                Assert.That(fixture.WalletGold.text, Is.EqualTo("--"));
                // The UI does not cancel ClientCore's request: let its correlated timeout finish first.
                Set(fixture.Login.Model, "_walletPendingDeadline", 0L);
                Invoke(fixture.Login.Model, "QueueExpiredEconomyRequests");
                Set(fixture.Screen, "walletRetryAt", Time.unscaledTime - .001f);
                Invoke(fixture.Screen, "Update");
                uint retry = fixture.Network.LastWalletRequestID;
                Assert.That(retry, Is.Not.EqualTo(expired));
                fixture.ReceiveWallet(expired, 1, 999, 1, 999);
                Assert.That(fixture.WalletGold.text, Is.EqualTo("…"));
                fixture.ReceiveWallet(retry, 0, 0, 0, 0);
                Set(fixture.Screen, "walletRetryAt", Time.unscaledTime - .001f);
                Invoke(fixture.Screen, "Update");
                fixture.ReceiveWallet(fixture.Network.LastWalletRequestID, 0, 0, 0, 0);
                Assert.That(fixture.Network.WalletRequests, Is.EqualTo(3));
                Assert.That(fixture.WalletGold.text, Is.EqualTo("--"));
                Assert.That(fixture.WalletDiamond.text, Is.EqualTo("--"));
                Assert.That(typeof(C09BazaarScreen).GetField("walletRetryAt", BindingFlags.Instance | BindingFlags.NonPublic)
                    .GetValue(fixture.Screen), Is.EqualTo(0f), "Exhausted reads wait for an explicit refresh or new world.");
                Assert.That(fixture.Network.Mutations, Is.Zero);
            }
        }

        [Test]
        public void WalletShowsEachSuccessfulBalanceAndIgnoresLatePreviousRequest()
        {
            using (var fixture = new Fixture())
            {
                uint initialRequest = fixture.Network.LastWalletRequestID;
                fixture.ReceiveWallet(initialRequest, 1, 123456, 0, 0);
                Assert.That(fixture.WalletGold.text, Is.EqualTo("123,456"));
                Assert.That(fixture.WalletDiamond.text, Is.EqualTo("--"));

                fixture.OpenMarket();
                uint currentRequest = fixture.Network.LastWalletRequestID;
                Assert.That(currentRequest, Is.Not.EqualTo(initialRequest));
                fixture.ReceiveWallet(initialRequest, 1, 1, 1, 2);
                Assert.That(fixture.WalletGold.text, Is.EqualTo("…"), "A late pair cannot replace the pending wallet request.");
                fixture.ReceiveWallet(currentRequest, 1, 9876543210, 1, 7654321);
                Assert.That(fixture.WalletGold.text, Is.EqualTo("9,876,543,210"));
                Assert.That(fixture.WalletDiamond.text, Is.EqualTo("7,654,321"));
                Invoke(fixture.Screen, "RefreshWallet");
                uint beforeTrade = fixture.Network.LastWalletRequestID;
                Invoke(fixture.Screen, "RefreshWallet"); // A completed trade requests a fresh balance while the old read is pending.
                fixture.ReceiveWallet(beforeTrade, 1, 100, 1, 200);
                Set(fixture.Screen, "walletRetryAt", Time.unscaledTime - .001f);
                Invoke(fixture.Screen, "Update");
                uint afterTrade = fixture.Network.LastWalletRequestID;
                Assert.That(afterTrade, Is.Not.EqualTo(beforeTrade), "A pending read cannot swallow the post-trade refresh.");
                fixture.ReceiveWallet(afterTrade, 1, 90, 1, 150);
                Assert.That(fixture.WalletGold.text, Is.EqualTo("90"));
                Assert.That(fixture.WalletDiamond.text, Is.EqualTo("150"));
            }
        }

        [Test]
        public void SellingBagStackShowsFeeAndBlocksRetryAfterUnknownResult()
        {
            using (var fixture = new Fixture())
            {
                fixture.OpenMarket();
                fixture.ReceiveWallet(fixture.Network.LastWalletRequestID, 1, 100, 1, 0);
                fixture.Receive(true, new BazaarListing { listingID = 77, itemID = 4, quantity = 2, price = 123 });
                fixture.Row(77).GetComponent<Button>().onClick.Invoke();
                fixture.Buy.onClick.Invoke();
                Assert.That(fixture.Status.text, Does.Contain("deduct 123 diamonds"));
                Assert.That(fixture.Network.Mutations, Is.Zero, "Reviewing a purchase must not send the transaction.");
                fixture.SellItems.onClick.Invoke();
                Assert.That(fixture.Network.InventoryRequests, Is.EqualTo(1));
                fixture.ReceiveInventory(new InventoryItem { itemID = 4, quantity = 5, slot = 7 });

                var item = fixture.Rows.Find("Listing-0");
                Assert.That(item, Is.Not.Null, "The sale list is populated from the returned bag slots.");
                item.GetComponent<Button>().onClick.Invoke();
                fixture.SaleGoldAfterValue.text = "unchanged";
                fixture.Quantity.text = "2";
                fixture.Price.text = "95";
                Assert.That(fixture.SaleFeeValue.text, Is.EqualTo("10"));
                Assert.That(fixture.SaleGoldAfterValue.text, Is.EqualTo("unchanged"), "The hidden legacy balance field must not be updated.");
                Assert.That(fixture.SaleSummary.text, Is.EqualTo("Gold fee only."));

                fixture.Register.onClick.Invoke();
                Assert.That(fixture.Register.interactable, Is.True, "The first press reviews the sale without sending it.");
                fixture.Register.onClick.Invoke();
                Assert.That(fixture.Network.RegisterRequests, Is.EqualTo(1));
                Assert.That(fixture.Network.LastRegisterSlot, Is.EqualTo((byte)7));
                Assert.That(fixture.Network.LastRegisterQuantity, Is.EqualTo((ushort)2));
                Assert.That(fixture.Network.LastRegisterPrice, Is.EqualTo(95UL));

                Set(fixture.Screen, "requestDeadline", Time.unscaledTime - 1);
                Invoke(fixture.Screen, "Update");
                Assert.That(fixture.Status.text, Does.Contain("Sale result unknown"));
                Assert.That(fixture.Register.interactable, Is.False);
                fixture.Register.onClick.Invoke();
                Assert.That(fixture.Network.RegisterRequests, Is.EqualTo(1), "An unknown result cannot submit the same sale again blindly.");
            }
        }

        [TestCase("register")]
        [TestCase("buy")]
        [TestCase("claim")]
        public void ClosedMarketReconcilesBalancesAfterRegisterBuyAndClaim(string operation)
        {
            using (var fixture = new Fixture())
            {
                fixture.ReceiveWallet(fixture.Network.LastWalletRequestID, 1, 100, 1, 200);
                fixture.OpenMarket();
                fixture.ReceiveWallet(fixture.Network.LastWalletRequestID, 1, 100, 1, 200);
                var net = (IViewModelNet)fixture.Login.Model;
                if (operation == "register")
                {
                    fixture.Receive(true);
                    fixture.SellItems.onClick.Invoke();
                    fixture.ReceiveInventory(new InventoryItem { slot = 7, itemID = 1, quantity = 5 });
                    fixture.Rows.Find("Listing-0").GetComponent<Button>().onClick.Invoke();
                    fixture.Quantity.text = "2";
                    fixture.Price.text = "95";
                    fixture.Register.onClick.Invoke();
                    fixture.Register.onClick.Invoke();
                }
                else
                {
                    if (operation == "claim") fixture.MyListings.onClick.Invoke();
                    var listing = new BazaarListing { listingID = 91, itemID = 1, quantity = 2, price = 50, status = (byte)(operation == "claim" ? 1 : 0) };
                    if (operation == "claim") net.BazaarMyListReceived(fixture.Network.LastRequestID, 1, new[] { listing });
                    else fixture.Receive(true, listing);
                    fixture.ReceiveWallet(fixture.Network.LastWalletRequestID, 1, 100, 1, 200);
                    fixture.Row(91).GetComponent<Button>().onClick.Invoke();
                    if (operation == "buy") { fixture.Buy.onClick.Invoke(); fixture.Buy.onClick.Invoke(); }
                    else fixture.Claim.onClick.Invoke();
                }
                Assert.That(fixture.Network.Mutations, Is.EqualTo(1), operation);
                uint writeID = fixture.Network.LastWriteID;
                uint oldWallet = fixture.Network.LastWalletRequestID;
                fixture.Close.onClick.Invoke();
                Assert.That(fixture.Panel.activeSelf, Is.False);
                // Reopening while the write is pending must not replace its correlation or selection.
                fixture.OpenMarket();
                fixture.Close.onClick.Invoke();
                if (operation == "register") net.BazaarRegisterReceived(writeID, 1);
                else if (operation == "buy") net.BazaarBuyReceived(writeID, 91, 1, 1, 2, 50);
                else net.BazaarClaimReceived(writeID, 91, 1, 50);
                Assert.That(fixture.Network.LastWalletRequestID, Is.Not.EqualTo(oldWallet), operation);
                fixture.ReceiveWallet(fixture.Network.LastWalletRequestID, 1, 90, 1, 150);
                Assert.That(fixture.WalletGold.text, Is.EqualTo("90"), operation);
                Assert.That(fixture.WalletDiamond.text, Is.EqualTo("150"), operation);
                Assert.That(fixture.Panel.activeSelf, Is.False);
                Assert.That(fixture.Network.Mutations, Is.EqualTo(1), "Only balance reads may be retried.");
            }
        }

        [Test]
        public void ZoneTransitionRestartsCanceledWalletAfterBootstrapWithoutOpeningMarket()
        {
            using (var fixture = new Fixture())
            {
                uint canceled = fixture.Network.LastWalletRequestID;
                fixture.Login.Model.ZoneChange(0); // Real ViewModel cancellation path; fake transport only.
                SetProperty(fixture.World, "Stage", C02Stage.TransitionPending);
                RaiseWorldChanged(fixture.World);
                fixture.ReceiveWallet(canceled, 1, 999, 1, 999);
                Assert.That(fixture.WalletGold.text, Is.EqualTo("--"));
                SetProperty(fixture.World, "Stage", C02Stage.InWorld);
                RaiseWorldChanged(fixture.World); // UI notification precedes ViewModel readiness.
                Set(fixture.Login.Model, "_worldReady", true);
                Invoke(fixture.Screen, "Update");
                uint fresh = fixture.Network.LastWalletRequestID;
                Assert.That(fresh, Is.Not.EqualTo(canceled));
                fixture.ReceiveWallet(canceled, 1, 999, 1, 999);
                Assert.That(fixture.WalletGold.text, Is.EqualTo("…"));
                fixture.ReceiveWallet(fresh, 1, 80, 1, 120);
                Assert.That(fixture.WalletGold.text, Is.EqualTo("80"));
                Assert.That(fixture.WalletDiamond.text, Is.EqualTo("120"));
                Assert.That(fixture.Panel.activeSelf, Is.False);
                Assert.That(fixture.Network.Mutations, Is.Zero);
            }
        }

        [TestCase("close")]
        [TestCase("timeout")]
        public void AbandonedOutboxRefreshesInventoryBeforeNextOpen(string abandonment)
        {
            using (var fixture = new Fixture())
            {
                fixture.OpenMarket();
                fixture.Receive(true);
                fixture.CheckOutbox.onClick.Invoke();
                Assert.That(fixture.Network.CheckOutboxRequests, Is.EqualTo(1));

                if (abandonment == "close") fixture.Close.onClick.Invoke();
                else
                {
                    Set(fixture.Screen, "requestDeadline", Time.unscaledTime - 1);
                    Invoke(fixture.Screen, "Update");
                }

                Assert.That(Get<bool>(fixture.Screen, "requestPending"), Is.EqualTo(abandonment == "close"));
                if (abandonment == "timeout")
                    Assert.That(Get<bool>(fixture.InventoryScreen, "refreshOnOpenAfterUncertainDelivery"), Is.True,
                        "A timed-out delivery remains uncertain; a closed panel still observes its completion.");
                ((IViewModelNet)fixture.Login.Model).BazaarCheckOutboxReceived(
                    fixture.Network.LastCheckOutboxRequestID, 1, 1, 0, 0);
                Assert.That(fixture.Network.CheckOutboxRequests, Is.EqualTo(1), "A stale response must never cause an automatic delivery retry.");
            }
        }

        [Test]
        public void EscapeClosesBagAndPendingOutboxWithoutRepeatingDelivery()
        {
            Assert.That(Application.isFocused, Is.True, "Keyboard-path verification requires the Editor to have focus.");
            var keyboard = InputSystem.AddDevice<Keyboard>();
            try
            {
                using (var fixture = new Fixture())
                {
                    var bagPanel = Get<GameObject>(fixture.InventoryScreen, "inventoryPanel");
                    bagPanel.SetActive(true);
                    InputSystem.QueueStateEvent(keyboard, new KeyboardState(Key.Escape));
                    InputSystem.Update();
                    Invoke(fixture.InventoryScreen, "Update");
                    Assert.That(bagPanel.activeSelf, Is.False);
                    InputSystem.QueueStateEvent(keyboard, new KeyboardState());
                    InputSystem.Update();

                    fixture.OpenMarket();
                    fixture.Receive(true);
                    fixture.CheckOutbox.onClick.Invoke();
                    Assert.That(fixture.Network.CheckOutboxRequests, Is.EqualTo(1));
                    InputSystem.QueueStateEvent(keyboard, new KeyboardState(Key.Escape));
                    InputSystem.Update();
                    Invoke(fixture.Screen, "Update");
                    Assert.That(fixture.Panel.activeSelf, Is.False);
                    Assert.That(Get<bool>(fixture.Screen, "requestPending"), Is.True,
                        "Escape closes the panel while item delivery completion remains observed.");
                    ((IViewModelNet)fixture.Login.Model).BazaarCheckOutboxReceived(
                        fixture.Network.LastCheckOutboxRequestID, 1, 1, 0, 0);
                    Assert.That(fixture.Network.CheckOutboxRequests, Is.EqualTo(1));
                }
            }
            finally
            {
                InputSystem.QueueStateEvent(keyboard, new KeyboardState());
                InputSystem.Update();
                InputSystem.RemoveDevice(keyboard);
            }
        }

        private sealed class Fixture : IDisposable
        {
            private readonly GameObject root;
            private readonly GameObject inventoryRoot;
            private readonly List<Initializer> clients = new List<Initializer>();
            public readonly C01LoginScreen Login;
            public readonly C02WorldScreen World;
            public readonly C05CombatScreen Combat;
            public readonly C09BazaarScreen Screen;
            public readonly GameObject Panel;
            public readonly Button Market, Search, Close, Next;
            public readonly Button SellItems, Register;
            public readonly Button Buy, Claim, MyListings, CheckOutbox, CancelListing;
            public Button Bag;
            public RectTransform InventoryGrid;
            public string InventoryQuantity => FirstActiveQuantity(InventoryGrid, "ItemQuantity");
            public string FirstSaleQuantity => FirstActiveQuantity(Rows, "Quantity");
            private static string FirstActiveQuantity(Transform rows, string name)
            {
                foreach (Transform row in rows)
                    if (row.gameObject.activeSelf) return row.Find(name).GetComponent<TMP_Text>().text;
                return null;
            }
            public readonly C07InventoryScreen InventoryScreen;
            public readonly Button[] TypeButtons;
            public readonly Transform Rows;
            public readonly TMP_Text EmptyState, Detail, Status;
            public readonly TMP_Text WalletGold, WalletDiamond, SaleSummary;
            public readonly TMP_Text SaleStackValue, SaleFeeValue, SaleGoldAfterValue, SelectedName, SelectedQuantity;
            public readonly TMP_InputField Quantity, Price;
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
                inventoryRoot = new GameObject("C07 inventory fixture");
                inventoryRoot.SetActive(false);
                InventoryScreen = inventoryRoot.AddComponent<C07InventoryScreen>();
                var inventoryPanel = new GameObject("C07 inventory panel");
                inventoryPanel.transform.SetParent(inventoryRoot.transform, false);
                inventoryPanel.SetActive(false);
                Set(InventoryScreen, "inventoryPanel", inventoryPanel);
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
                Buy = ButtonObject("buy");
                Claim = ButtonObject("claim");
                MyListings = ButtonObject("my listings");
                CheckOutbox = ButtonObject("receive items");
                CancelListing = ButtonObject("cancel listing");
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
                WalletGold = Text("wallet gold");
                WalletDiamond = Text("wallet diamonds");
                SaleSummary = Text("sale summary");
                SaleStackValue = Text("stack total");
                SaleFeeValue = Text("gold fee");
                SaleGoldAfterValue = Text("gold after");
                SelectedName = Text("selected item name");
                SelectedQuantity = Text("selected item quantity");
                var selectedIconObject = Child("selected item icon");
                var selectedIcon = selectedIconObject.AddComponent<Image>();
                Quantity = Input("sale quantity");
                Price = Input("sale price");
                SellItems = ButtonObject("sell items");
                Register = ButtonObject("register sale");
                var saleForm = Child("sale form");
                var walletHud = Child("wallet HUD");
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
                Set(Screen, "buyButton", Buy);
                Set(Screen, "claimButton", Claim);
                Set(Screen, "myListingsButton", MyListings);
                Set(Screen, "checkOutboxButton", CheckOutbox);
                Set(Screen, "cancelListingButton", CancelListing);
                Set(Screen, "inventoryScreen", InventoryScreen);
                Set(Screen, "pageText", page);
                Set(Screen, "listingRows", Rows);
                Set(Screen, "emptyStateText", EmptyState);
                Set(Screen, "selectedDetailText", Detail);
                Set(Screen, "selectedItemIcon", selectedIcon);
                Set(Screen, "selectedItemNameText", SelectedName);
                Set(Screen, "selectedItemQuantityText", SelectedQuantity);
                Set(Screen, "statusText", Status);
                Set(Screen, "sellItemsButton", SellItems);
                Set(Screen, "registerButton", Register);
                Set(Screen, "saleForm", saleForm);
                Set(Screen, "quantityInput", Quantity);
                Set(Screen, "priceInput", Price);
                Set(Screen, "saleSummaryText", SaleSummary);
                Set(Screen, "saleStackValueText", SaleStackValue);
                Set(Screen, "saleFeeValueText", SaleFeeValue);
                Set(Screen, "saleGoldAfterValueText", SaleGoldAfterValue);
                Set(Screen, "walletHud", walletHud);
                Set(Screen, "walletGoldText", WalletGold);
                Set(Screen, "walletDiamondText", WalletDiamond);
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
                Invoke(Screen, "Update");
            }

            public void ActivateInventory(bool inventoryFirst)
            {
                Set(InventoryScreen, "loginScreen", Login); Set(InventoryScreen, "worldScreen", World);
                Set(InventoryScreen, "bazaarScreen", Screen);
                Bag = ButtonObject("bag");
                Set(InventoryScreen, "bagButton", Bag);
                Set(InventoryScreen, "refreshButton", ButtonObject("bag refresh"));
                Set(InventoryScreen, "closeButton", ButtonObject("bag close"));
                var scrollObject = Child("bag scroll");
                scrollObject.transform.SetParent(Get<GameObject>(InventoryScreen, "inventoryPanel").transform, false);
                var scroll = scrollObject.AddComponent<ScrollRect>();
                var viewport = Child("bag viewport"); viewport.transform.SetParent(scrollObject.transform, false);
                viewport.AddComponent<Image>(); viewport.AddComponent<Mask>();
                scroll.viewport = (RectTransform)viewport.transform;
                var grid = Child("bag grid"); grid.transform.SetParent(viewport.transform, false);
                InventoryGrid = (RectTransform)grid.transform;
                grid.AddComponent<GridLayoutGroup>().cellSize = new Vector2(120, 110);
                scroll.content = InventoryGrid;
                Set(InventoryScreen, "inventoryScroll", scroll); Set(InventoryScreen, "inventoryGridContent", InventoryGrid);
                Set(InventoryScreen, "detailBadge", Text("bag badge")); Set(InventoryScreen, "detailName", Text("bag name"));
                Set(InventoryScreen, "detailSlot", Text("bag slot")); Set(InventoryScreen, "detailQuantity", Text("bag quantity"));
                Set(InventoryScreen, "inventoryStatus", Text("bag status"));
                inventoryRoot.SetActive(true);
                if (inventoryFirst)
                {
                    var callback = (Action<bool>)Delegate.CreateDelegate(typeof(Action<bool>), Screen,
                        typeof(C09BazaarScreen).GetMethod("SaleInventoryReceived", BindingFlags.Instance | BindingFlags.NonPublic));
                    Login.Model.OnInventoryReceived -= callback;
                    Login.Model.OnInventoryReceived += callback;
                }
            }

            public void OpenMarket() => Market.onClick.Invoke();
            public Transform Row(ulong id) => Rows.Find("Listing-" + id);
            public void Receive(bool success, params BazaarListing[] rows) => Receive(Network.LastRequestID, success, rows);
            public void Receive(uint requestID, bool success, params BazaarListing[] rows) =>
                ((IViewModelNet)Login.Model).BazaarReceived(requestID, (byte)(success ? 1 : 0), rows ?? Array.Empty<BazaarListing>());
            public void ReceiveWallet(uint requestID, byte goldStatus, ulong gold, byte diamondStatus, ulong diamonds)
            {
                var network = (IViewModelNet)Login.Model;
                network.WalletGoldReceived(requestID, goldStatus, gold);
                network.WalletDiamondReceived(requestID, diamondStatus, diamonds);
            }
            public void ReceiveInventory(params InventoryItem[] items) =>
                ((IViewModelNet)Login.Model).InventoryReceived(1, items);

            private (Initializer Initializer, IViewModelUI Model, FakeNetwork Network) CreateClient()
            {
                var assembly = typeof(IViewModelUI).Assembly;
                var dataType = assembly.GetType("ClientCore.ViewData", true);
                var data = Activator.CreateInstance(dataType, true);
                Network = new FakeNetwork();
                var modelType = assembly.GetType("ClientCore.ViewModel", true);
                var model = (IViewModelUI)Activator.CreateInstance(modelType, BindingFlags.Instance | BindingFlags.NonPublic, null,
                    new object[] { Network, data, new NullLogger(), new InlineDispatcher() }, null);
                Set(model, "_worldReady", true);
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
                var label = Text("Label");
                label.transform.SetParent(go.transform, false);
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
                if (inventoryRoot != null) UnityEngine.Object.DestroyImmediate(inventoryRoot);
                if (root != null) UnityEngine.Object.DestroyImmediate(root);
                foreach (var client in clients) client.Dispose();
            }
        }

        private sealed class FakeNetwork : INetworkService
        {
            public int RegisterRequests, InventoryRequests, WalletRequests, CheckOutboxRequests, CancelListingRequests;
            public int Mutations;
            public uint LastWriteID, LastCheckOutboxRequestID;
            public uint LastWalletRequestID;
            public bool ThrowWalletRequest;
            public byte LastRegisterSlot;
            public ushort LastRegisterQuantity;
            public ulong LastRegisterPrice;
            public void RequestBazaarRegister(uint requestID, byte inventorySlot, ushort quantity, ulong totalDiamondPrice)
            { RegisterRequests++; Mutations++; LastWriteID = requestID; LastRegisterSlot = inventorySlot; LastRegisterQuantity = quantity; LastRegisterPrice = totalDiamondPrice; }
            public void RequestWallet(uint requestID)
            {
                WalletRequests++;
                LastWalletRequestID = requestID;
                if (ThrowWalletRequest) throw new InvalidOperationException("Simulated wallet transport startup failure.");
            }
            public int Requests;
            public uint LastRequestID;
            public ushort LastItemType, LastPage;
            public void RequestBazaarBuy(uint requestID, ulong listingID) { Mutations++; LastWriteID = requestID; }
            public void RequestBazaarCheckOutbox(uint requestID) { CheckOutboxRequests++; LastCheckOutboxRequestID = requestID; }
            public void RequestBazaarCancelListing(uint requestID, ulong listingID) { CancelListingRequests++; LastWriteID = requestID; }
            public void RequestBazaarClaim(uint requestID, ulong listingID) { Mutations++; LastWriteID = requestID; }
            public void RequestBazaarMyList(uint requestID) { LastRequestID = requestID; }
            public void RequestBazaar(uint requestID, ushort itemType, ushort page)
            { Requests++; LastRequestID = requestID; LastItemType = itemType; LastPage = page; }
            public Task Connect(string address, int port) => Task.CompletedTask;
            public void CharacterList() { }
            public void Enter(ulong characterId) { }
            public void Chat(string message, byte scope, ulong targetId) { }
            public void Profiles(uint[] profileIds) { }
            public void RequestInventory() { InventoryRequests++; }
            public void Action(byte direction, float x, float y, byte skillSlot, ulong ownershipEpoch) { }
            public void Pong(ulong serverTimeMs) { }
            public void ZoneChange(byte op) { } public void ZoneResync(ulong ownershipEpoch) { } public void RespawnToTown(ulong ownershipEpoch) { }
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
