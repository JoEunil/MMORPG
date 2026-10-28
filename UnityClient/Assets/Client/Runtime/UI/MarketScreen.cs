using System;
using System.Collections.Generic;
using System.Globalization;
using Mmorpg.Client;
using ClientCore;
using TMPro;
using UnityEngine;
using UnityEngine.UI;
using UnityEngine.InputSystem;

namespace Mmorpg.Client
{
    [UnityEngine.Scripting.APIUpdating.MovedFrom(true, sourceNamespace: "C02", sourceAssembly: "C02.Runtime", sourceClassName: "C09BazaarScreen")]
    public sealed class MarketScreen : MonoBehaviour
    {
        private const float RequestTimeoutSeconds = 8f;
        [SerializeField] private LoginScreen loginScreen;
        [SerializeField] private WorldScreen worldScreen;
        [SerializeField] private GameObject bazaarPanel;
        [SerializeField] private Button marketButton;
        [SerializeField] private Button[] itemTypeButtons;
        [SerializeField] private Button searchListingsButton;
        [SerializeField] private Button myListingsButton;
        [SerializeField] private Button searchButton;
        [SerializeField] private Button previousPageButton;
        [SerializeField] private Button nextPageButton;
        [SerializeField] private Button closeButton;
        [SerializeField] private Button claimButton;
        [SerializeField] private Button buyButton;
        [SerializeField] private Button cancelListingButton;
        [SerializeField] private Button checkOutboxButton;
        [SerializeField] private TMP_Text pageText;
        [SerializeField] private Transform listingRows;
        [SerializeField] private TMP_Text emptyStateText;
        [SerializeField] private TMP_Text selectedDetailText;
        [SerializeField] private InventoryScreen inventoryScreen;
        [SerializeField] private TMP_Text statusText;
        [SerializeField] private Button sellItemsButton;
        [SerializeField] private Button registerButton;
        [SerializeField] private GameObject saleForm;
        [SerializeField] private TMP_InputField quantityInput;
        [SerializeField] private TMP_InputField priceInput;
        [SerializeField] private TMP_Text saleSummaryText;
        [SerializeField] private TMP_Text saleStackValueText;
        [SerializeField] private TMP_Text saleFeeValueText;
        [SerializeField] private TMP_Text saleGoldAfterValueText;
        [SerializeField] private Image selectedItemIcon;
        [SerializeField] private TMP_Text selectedItemNameText;
        [SerializeField] private TMP_Text selectedItemQuantityText;
        [SerializeField] private GameObject walletHud;
        [SerializeField] private TMP_Text walletGoldText;
        [SerializeField] private TMP_Text walletDiamondText;
        [SerializeField] private Sprite diamondCurrencyIcon;
        private enum MarketView { Browse, MyListings, Sell }
        private MarketView view;
        private bool saleInventoryReplyReady;
        private bool registerConfirmation;
        private bool registrationUncertain;
        private InventoryItemView saleItem;
        private bool walletRequested, walletPending, goldKnown, diamondKnown;
        private float walletDeadline;
        private float walletRetryAt;
        private int walletRetries;
        private bool walletRefreshQueued;
        private ulong gold, diamonds;
        private IViewModelUI model;
        private ushort itemType = 1;
        private ushort page;
        private float requestDeadline;
        private bool requestPending;
        private enum WriteRequest { None, Claim, Buy, CheckOutbox, CancelListing, Register }
        private WriteRequest writeRequest;
        private BazaarListingView selectedListing;
        private bool buyConfirmation;
        private bool cancelConfirmation;
        private readonly Dictionary<ulong, byte> cancelOutcomes = new Dictionary<ulong, byte>();
        private readonly Dictionary<ulong, byte> buyOutcomes = new Dictionary<ulong, byte>();
        private readonly Dictionary<ulong, byte> claimOutcomes = new Dictionary<ulong, byte>();
        private readonly HashSet<ulong> unresolvedClaims = new HashSet<ulong>();
        private UnityEngine.UI.Button selectedRowButton;
        private Color selectedRowBaseColor;
        public bool IsOpen => bazaarPanel != null && bazaarPanel.activeSelf;

        private bool WorldAvailable => worldScreen != null && worldScreen.isActiveAndEnabled &&
            worldScreen.Stage == WorldStage.InWorld && worldScreen.HasOwnFull && worldScreen.IsWorldVisible;

        private void OnEnable()
        {
            if (loginScreen == null || worldScreen == null || bazaarPanel == null || marketButton == null ||
                itemTypeButtons == null || itemTypeButtons.Length != 3 || searchButton == null ||
                previousPageButton == null || nextPageButton == null || closeButton == null ||
                pageText == null || listingRows == null || emptyStateText == null || selectedDetailText == null || statusText == null)
            {
                Debug.LogError("C09 bazaar references are missing.", this);
                enabled = false;
                return;
            }

            marketButton.onClick.AddListener(TogglePanel);
            itemTypeButtons[0].onClick.AddListener(SelectPotion);
            itemTypeButtons[1].onClick.AddListener(SelectWeaponShield);
            itemTypeButtons[2].onClick.AddListener(SelectArmor);
            if (searchListingsButton != null) searchListingsButton.onClick.AddListener(SelectSearchListings);
            if (myListingsButton != null) myListingsButton.onClick.AddListener(SelectMyListings);
            searchButton.onClick.AddListener(RefreshCurrentView);
            previousPageButton.onClick.AddListener(PreviousPage);
            nextPageButton.onClick.AddListener(NextPage);
            closeButton.onClick.AddListener(ClosePanel);
            if (claimButton != null) claimButton.onClick.AddListener(ClaimSelectedListing);
            if (buyButton != null) buyButton.onClick.AddListener(BuySelectedListing);
            if (cancelListingButton != null) cancelListingButton.onClick.AddListener(CancelSelectedListing);
            if (checkOutboxButton != null) checkOutboxButton.onClick.AddListener(CheckOutbox);
            if (sellItemsButton != null) sellItemsButton.onClick.AddListener(SelectSellItems);
            if (registerButton != null) registerButton.onClick.AddListener(RegisterSale);
            if (quantityInput != null) quantityInput.onValueChanged.AddListener(SaleInputChanged);
            if (priceInput != null) priceInput.onValueChanged.AddListener(SaleInputChanged);
            loginScreen.ClientChanged += ChangeClient;
            worldScreen.WorldStateChanged += WorldChanged;
            ChangeClient(loginScreen.Model);
        }

        private void Update()
        {
            // Bootstrap callbacks run before ClientCore marks the world ready; request on the next frame.
            if (!walletRequested && WorldAvailable) RefreshWallet();
            if (walletRetryAt > 0 && Time.unscaledTime >= walletRetryAt && WorldAvailable && !walletPending)
            {
                walletRetryAt = 0;
                RefreshWallet();
            }
            if (Application.isFocused && Keyboard.current != null)
            {
                if (GameUiInput.EscapeAvailableForPanel() && IsOpen) ClosePanel();
                else if (Keyboard.current.bKey.wasPressedThisFrame && GameUiInput.CanUseWorldShortcuts()) TogglePanel();
            }

            if (walletPending && Time.unscaledTime >= walletDeadline)
            {
                walletPending = false;
                goldKnown = diamondKnown = false;
                ShowWallet();
                ScheduleWalletRetry();
            }
            // Let the inventory owner consume this callback first, including any queued post-delivery read.
            if (saleInventoryReplyReady && (inventoryScreen == null || !inventoryScreen.InventoryRequestPending))
            {
                saleInventoryReplyReady = false;
                SaleInventoryReceived(inventoryScreen == null || inventoryScreen.HasInventoryResponse);
            }
            if (!requestPending || Time.unscaledTime < requestDeadline) return;
            WriteRequest previousWrite = writeRequest;
            CancelPending();
            if (previousWrite == WriteRequest.Register || previousWrite == WriteRequest.Buy || previousWrite == WriteRequest.Claim) RefreshWallet();
            if (previousWrite == WriteRequest.None) ClearRows();
            statusText.text = previousWrite == WriteRequest.Register ? "Sale result unknown. Check My listings and your bag before trying again; no automatic retry."
                : previousWrite == WriteRequest.Claim ? "Collection result unknown. Check again after reconnecting; no automatic retry."
                : previousWrite == WriteRequest.Buy ? "Purchase result unknown. Reconnect to check; no automatic retry."
                : previousWrite == WriteRequest.CancelListing ? "Cancellation result unknown. Refresh my listings and receive items; no automatic retry."
                : previousWrite == WriteRequest.CheckOutbox ? "Receipt result incomplete; items may have arrived. Check your bag, then retry manually."
                : view == MarketView.MyListings
                ? "My listings timed out. Select Refresh to retry."
                : "Search timed out. Select Search to retry.";
            UpdateControls();
        }

        private void ChangeClient(IViewModelUI next)
        {
            if (model != null)
            {
                model.OnWalletReceived -= WalletReceived;
                model.OnInventoryReceived -= SaleInventoryReceived;
                model.OnBazaarRegisterReceived -= RegisterReceived;
                model.OnBazaarReceived -= BazaarReceived;
                model.OnBazaarClaimReceived -= ClaimReceived;
                model.OnBazaarBuyReceived -= BuyReceived;
                model.OnBazaarCancelListingReceived -= CancelListingReceived;
                model.OnBazaarCheckOutboxReceived -= CheckOutboxReceived;
            }
            CancelPending();
            claimOutcomes.Clear();
            unresolvedClaims.Clear();
            buyOutcomes.Clear();
            cancelOutcomes.Clear();
            model = next;
            view = MarketView.Browse;
            registerConfirmation = registrationUncertain = false;
            walletRequested = walletPending = goldKnown = diamondKnown = false;
            walletRetryAt = 0;
            walletRetries = 0;
            walletRefreshQueued = false;
            saleItem = default;
            ShowWallet();
            page = 0;
            ClearResults();
            bazaarPanel.SetActive(false);
            if (model != null)
            {
                model.OnWalletReceived += WalletReceived;
                model.OnInventoryReceived += SaleInventoryReceived;
                model.OnBazaarRegisterReceived += RegisterReceived;
                model.OnBazaarReceived += BazaarReceived;
                model.OnBazaarClaimReceived += ClaimReceived;
                model.OnBazaarBuyReceived += BuyReceived;
                model.OnBazaarCancelListingReceived += CancelListingReceived;
                model.OnBazaarCheckOutboxReceived += CheckOutboxReceived;
            }
            WorldChanged();
        }

        private void WorldChanged()
        {
            bool available = WorldAvailable && model != null && loginScreen.Model == model;
            marketButton.gameObject.SetActive(available);
            if (walletHud != null) walletHud.SetActive(available);
            if (!available)
            {
                // ClientCore cancels wallet reads on transfer; restart after destination bootstrap.
                walletRequested = walletPending = goldKnown = diamondKnown = false;
                walletRetryAt = 0;
                walletRetries = 0;
                walletRefreshQueued = false;
                ShowWallet();
                CancelPending();
                bazaarPanel.SetActive(false);
                ClearResults();
            }
            UpdateControls();
        }

        private void TogglePanel()
        {
            if (!WorldAvailable || model == null || loginScreen.Model != model) return;
            if (bazaarPanel.activeSelf) { ClosePanel(); return; }
            worldScreen.StopActions();
            if (inventoryScreen != null) inventoryScreen.ClosePanel();
            if (requestPending && writeRequest != WriteRequest.None)
            {
                bazaarPanel.SetActive(true);
                bazaarPanel.transform.SetAsLastSibling();
                UpdateControls();
                return;
            }
            view = MarketView.Browse;
            bazaarPanel.SetActive(true);
            bazaarPanel.transform.SetAsLastSibling();
            RefreshWallet();
            ClearResults();
            Search();
        }

        private void SelectSearchListings()
        {
            SelectView(MarketView.Browse);
        }

        private void SelectMyListings()
        {
            SelectView(MarketView.MyListings);
        }

        private void SelectView(MarketView nextView)
        {
            if (!bazaarPanel.activeSelf || !WorldAvailable || model == null || loginScreen.Model != model)
                return;
            if (writeRequest != WriteRequest.None) return;
            if (view == nextView) return;
            CancelPending();
            view = nextView;
            ClearResults();
            UpdateControls();
            RefreshCurrentView();
        }

        private void RefreshCurrentView()
        {
            RefreshWallet();
            if (view == MarketView.Sell) { LoadSaleItems(); return; }
            if (view == MarketView.MyListings)
            {
                if (!bazaarPanel.activeSelf || !WorldAvailable || model == null || loginScreen.Model != model || requestPending)
                    return;
                worldScreen.StopActions();
                ClearRows();
                requestPending = true;
                requestDeadline = Time.unscaledTime + RequestTimeoutSeconds;
                statusText.text = "Loading my listings…";
                UpdateControls();
                try { model.RequestBazaarMyList(); }
                catch (Exception)
                {
                    CancelPending();
                    statusText.text = "My listings failed. Select Refresh to retry.";
                    UpdateControls();
                }
                return;
            }
            Search();
        }

        private void SelectItemType(int value)
        {
            if (!bazaarPanel.activeSelf || !WorldAvailable || requestPending || value < 1 || value > 3) return;
            if (itemType == value) return;
            itemType = (ushort)value;
            page = 0;
            ClearResults();
            Search();
        }

        private void SelectPotion() => SelectItemType(1);
        private void SelectWeaponShield() => SelectItemType(2);
        private void SelectArmor() => SelectItemType(3);

        private void Search()
        {
            if (view != MarketView.Browse || !bazaarPanel.activeSelf || !WorldAvailable || model == null || loginScreen.Model != model || requestPending)
                return;
            if (itemType < 1 || itemType > 3) return;
            worldScreen.StopActions();
            ClearRows();
            requestPending = true;
            requestDeadline = Time.unscaledTime + RequestTimeoutSeconds;
            statusText.text = "Searching " + ItemTypeName(itemType) + " listings…";
            UpdateControls();
            try { model.RequestBazaar(itemType, page); }
            catch (Exception)
            {
                CancelPending();
                statusText.text = "Search failed. Select Search to retry.";
                UpdateControls();
            }
        }

        private void PreviousPage()
        {
            if (requestPending || page == 0) return;
            page--;
            Search();
        }

        private void NextPage()
        {
            if (requestPending || page == ushort.MaxValue) return;
            page++;
            Search();
        }

        public void ClosePanel()
        {
            bazaarPanel.SetActive(false);
            // A closed panel must still observe item delivery and balance-changing writes.
            if (requestPending && writeRequest != WriteRequest.None) return;
            CancelPending();
            if (view == MarketView.MyListings) view = MarketView.Browse;
            ClearResults();
            UpdateControls();
        }

        private void BazaarReceived(bool success)
        {
            if (view == MarketView.Sell || !requestPending || writeRequest != WriteRequest.None || model == null || loginScreen.Model != model || !WorldAvailable || !bazaarPanel.activeSelf)
                return;
            requestPending = false;
            List<BazaarListingView> rows = success && loginScreen.Data != null
                ? loginScreen.Data.GetBazaar()
                : new List<BazaarListingView>();
            if (!success)
            {
                ClearRows();
                statusText.text = view == MarketView.MyListings
                    ? "My listings failed. Select Refresh to retry."
                    : "Search failed. Select Search to retry.";
                UpdateControls();
                return;
            }
            if (rows == null || rows.Count > 30)
            {
                ClearRows();
                statusText.text = view == MarketView.MyListings
                    ? "My listings response was invalid. Select Refresh to retry."
                    : "Search response was invalid. Select Search to retry.";
                UpdateControls();
                return;
            }

            ClearRows();
            for (int i = 0; i < rows.Count; i++) CreateListingRow(rows[i], i);
            emptyStateText.gameObject.SetActive(rows.Count == 0);
            emptyStateText.text = view == MarketView.MyListings ? "You have no listings." : "No listings on this page.";
            statusText.text = view == MarketView.MyListings
                ? rows.Count == 0 ? "You have no listings." : "My listings: " + rows.Count.ToString(CultureInfo.InvariantCulture) + " listings · up to 30 shown"
                : rows.Count == 0 ? "No listings in " + ItemTypeName(itemType) + " on this page." :
                    rows.Count.ToString(CultureInfo.InvariantCulture) + " listings · " + ItemTypeName(itemType);
            UpdateControls();
        }

        private GameObject CreateItemRow(uint itemID, ushort quantity, int index)
        {
            var rowObject = new GameObject("Listing-" + index.ToString(CultureInfo.InvariantCulture),
                typeof(RectTransform), typeof(UnityEngine.UI.Image), typeof(UnityEngine.UI.Button), typeof(LayoutElement));
            rowObject.transform.SetParent(listingRows, false);
            var image = rowObject.GetComponent<UnityEngine.UI.Image>();
            image.color = new Color32(21, 42, 48, 255);
            var edge = rowObject.AddComponent<UnityEngine.UI.Outline>();
            edge.effectColor = new Color32(107, 146, 144, 90);
            edge.effectDistance = new Vector2(.7f, -.7f);
            var button = rowObject.GetComponent<UnityEngine.UI.Button>();
            button.targetGraphic = image;
            var layout = rowObject.GetComponent<LayoutElement>();
            layout.minHeight = layout.preferredHeight = 74;

            var accent = new GameObject("ItemAccent", typeof(RectTransform), typeof(UnityEngine.UI.Image));
            accent.transform.SetParent(rowObject.transform, false);
            var accentRect = accent.GetComponent<RectTransform>();
            accentRect.anchorMin = new Vector2(0, 0);
            accentRect.anchorMax = new Vector2(0, 1);
            accentRect.pivot = new Vector2(0, .5f);
            accentRect.sizeDelta = new Vector2(5, 0);
            accent.GetComponent<UnityEngine.UI.Image>().color = ItemPresentation.Accent(itemID);
            accent.GetComponent<UnityEngine.UI.Image>().raycastTarget = false;

            TMP_Text badge = AddCell(rowObject.transform, "Badge", ItemPresentation.Badge(itemID), 0.02f, 0.13f,
                TextAlignmentOptions.Center, 14, ItemPresentation.Accent(itemID));
            Sprite icon = ItemPresentation.Icon(itemID);
            if (icon != null)
            {
                badge.text = string.Empty;
                var iconObject = new GameObject("ItemIcon", typeof(RectTransform), typeof(CanvasRenderer), typeof(UnityEngine.UI.Image));
                iconObject.transform.SetParent(rowObject.transform, false);
                var iconRect = iconObject.GetComponent<RectTransform>();
                iconRect.anchorMin = iconRect.anchorMax = new Vector2(0.075f, 0.5f);
                iconRect.pivot = new Vector2(0.5f, 0.5f);
                iconRect.anchoredPosition = Vector2.zero;
                iconRect.sizeDelta = new Vector2(50, 50);
                var iconImage = iconObject.GetComponent<UnityEngine.UI.Image>();
                iconImage.sprite = icon;
                iconImage.preserveAspect = true;
                iconImage.raycastTarget = false;
            }
            AddCell(rowObject.transform, "Item", ItemPresentation.Name(itemID), 0.14f, 0.40f,
                TextAlignmentOptions.MidlineLeft, 16, new Color32(237, 242, 231, 255));
            AddCell(rowObject.transform, "Quantity", quantity.ToString(CultureInfo.InvariantCulture), 0.41f, 0.50f,
                TextAlignmentOptions.Center, 14, new Color32(184, 202, 198, 255));
            return rowObject;
        }

        private void CreateListingRow(BazaarListingView listing, int index)
        {
            var rowObject = CreateItemRow(listing.ItemID, listing.Quantity, index);
            rowObject.name = "Listing-" + listing.ListingID.ToString(CultureInfo.InvariantCulture);
            var button = rowObject.GetComponent<UnityEngine.UI.Button>();
            AddCurrencyMark(rowObject.transform, "DiamondMark", false, .525f, diamondCurrencyIcon);
            AddCell(rowObject.transform, "Price", listing.Price.ToString("N0", CultureInfo.InvariantCulture), .56f, .77f,
                TextAlignmentOptions.MidlineRight, 14, new Color32(169, 220, 229, 255));
            var stateBadge = new GameObject("StateBadge", typeof(RectTransform), typeof(UnityEngine.UI.Image));
            stateBadge.transform.SetParent(rowObject.transform, false);
            var stateRect = stateBadge.GetComponent<RectTransform>();
            stateRect.anchorMin = stateRect.anchorMax = new Vector2(.885f, .5f);
            stateRect.pivot = new Vector2(.5f, .5f);
            stateRect.sizeDelta = new Vector2(60, 27);
            stateBadge.GetComponent<UnityEngine.UI.Image>().color = listing.Status == 0
                ? new Color32(23, 49, 57, 255) : new Color32(21, 42, 48, 255);
            stateBadge.GetComponent<UnityEngine.UI.Image>().raycastTarget = false;
            var state = AddCell(rowObject.transform, "State", ListingStatusName(listing.Status), .78f, .99f,
                TextAlignmentOptions.Center, 13, new Color(.84f, .88f, .87f));
            stateRect.SetAsFirstSibling();
            button.onClick.AddListener(() => SelectListing(listing, button));
        }

        private void SelectRow(UnityEngine.UI.Button button)
        {
            if (selectedRowButton != null)
            {
                var previous = selectedRowButton.GetComponent<UnityEngine.UI.Image>();
                if (previous != null) previous.color = selectedRowBaseColor;
                var previousEdge = selectedRowButton.GetComponent<UnityEngine.UI.Outline>();
                if (previousEdge != null) previousEdge.effectColor = new Color32(107, 146, 144, 90);
            }
            selectedRowButton = button;
            var image = button.GetComponent<UnityEngine.UI.Image>();
            selectedRowBaseColor = image.color;
            image.color = new Color32(40, 86, 79, 255);
            var edge = button.GetComponent<UnityEngine.UI.Outline>();
            if (edge != null) edge.effectColor = new Color32(229, 211, 163, 255);
        }

        private void SelectListing(BazaarListingView listing, UnityEngine.UI.Button button)
        {
            if (writeRequest != WriteRequest.None) return;
            bool confirmationCleared = buyConfirmation || cancelConfirmation;
            SelectRow(button);
            selectedListing = listing;
            buyConfirmation = false;
            cancelConfirmation = false;
            ShowSelectedItem(listing.ItemID, listing.Quantity);
            selectedDetailText.text = listing.Price.ToString("N0", CultureInfo.InvariantCulture) + " diamonds · " + ListingStatusName(listing.Status);
            if (claimOutcomes.TryGetValue(listing.ListingID, out byte outcome)) selectedDetailText.text = outcome == 1 ? "Proceeds collected." : "No proceeds to collect.";
            else if (unresolvedClaims.Contains(listing.ListingID)) selectedDetailText.text = "Collection result unknown · reconnect to check.";
            if (buyOutcomes.TryGetValue(listing.ListingID, out byte buyOutcome))
                selectedDetailText.text = buyOutcome == 1 ? "Purchased · receive items to your bag." : "Purchase result unknown · reconnect to check.";
            if (cancelOutcomes.TryGetValue(listing.ListingID, out byte cancelOutcome))
                selectedDetailText.text = cancelOutcome == 1 ? "Sale cancelled · receive returned item." : "Cancellation result unknown · refresh listings.";
            if (confirmationCleared) statusText.text = "Selected listing: " + selectedDetailText.text;
            UpdateControls();
        }

        private void CancelSelectedListing()
        {
            if (view != MarketView.MyListings || !IsOpen || !WorldAvailable || model == null || loginScreen.Model != model ||
                requestPending || selectedListing.ListingID == 0 || selectedListing.Status != 0 ||
                cancelOutcomes.ContainsKey(selectedListing.ListingID)) return;
            if (!cancelConfirmation)
            {
                cancelConfirmation = true;
                selectedDetailText.text = "Fee is not refunded.\nReceive returned items separately.";
                statusText.text = "Press again to cancel the sale.";
                UpdateControls();
                return;
            }
            cancelConfirmation = false;
            cancelOutcomes[selectedListing.ListingID] = 5;
            requestPending = true;
            writeRequest = WriteRequest.CancelListing;
            requestDeadline = Time.unscaledTime + RequestTimeoutSeconds;
            statusText.text = "Cancelling sale…";
            UpdateControls();
            try { model.RequestBazaarCancelListing(selectedListing.ListingID); }
            catch (Exception)
            {
                CancelPending();
                statusText.text = "Cancellation result unknown. Refresh my listings and receive items; no automatic retry.";
                UpdateControls();
            }
        }

        private void CancelListingReceived(ulong listingID, byte status)
        {
            if (writeRequest != WriteRequest.CancelListing || !requestPending || listingID != selectedListing.ListingID ||
                model == null || loginScreen.Model != model || !WorldAvailable) return;
            requestPending = false;
            writeRequest = WriteRequest.None;
            if (status == 0) cancelOutcomes.Remove(listingID);
            else cancelOutcomes[listingID] = status == 1 ? (byte)1 : (byte)5;
            // The existing server can attempt return delivery before replying, even if the result is unknown.
            if (status != 0 && inventoryScreen != null)
            {
                if (status != 1) inventoryScreen.RequireFreshInventoryOnOpen();
                inventoryScreen.RefreshAfterDelivery();
            }
            statusText.text = status == 1 ? "Sale cancelled. Receive items, then check your bag. The listing fee is not refunded."
                : status == 0 ? "Sale could not be cancelled. Refresh my listings to check its current state."
                : "Cancellation result unknown. Refresh my listings and receive items; no automatic retry.";
            selectedDetailText.text = statusText.text;
            UpdateControls();
        }

        private void BuySelectedListing()
        {
            if (view == MarketView.MyListings || !IsOpen || !WorldAvailable || model == null || loginScreen.Model != model ||
                requestPending || selectedListing.ListingID == 0 || selectedListing.Status != 0 ||
                buyOutcomes.ContainsKey(selectedListing.ListingID)) return;
            if (!buyConfirmation)
            {
                buyConfirmation = true;
                statusText.text = "Confirm purchase: deduct " + selectedListing.Price.ToString("N0", CultureInfo.InvariantCulture) + " diamonds. Press again to buy.";
                UpdateControls();
                return;
            }
            buyConfirmation = false;
            buyOutcomes[selectedListing.ListingID] = 5; // A sent transaction can outlive its UI request.
            requestPending = true;
            writeRequest = WriteRequest.Buy;
            requestDeadline = Time.unscaledTime + RequestTimeoutSeconds;
            statusText.text = "Purchasing…";
            UpdateControls();
            try { model.RequestBazaarBuy(selectedListing.ListingID); }
            catch (Exception)
            {
                CancelPending();
                statusText.text = "Purchase result unknown. Reconnect to check; no automatic retry.";
                UpdateControls();
            }
        }

        private void BuyReceived(ulong listingID, byte status, uint itemID, uint quantity, ulong amount)
        {
            if (writeRequest != WriteRequest.Buy || !requestPending || listingID != selectedListing.ListingID ||
                model == null || loginScreen.Model != model || !WorldAvailable) return;
            RefreshWallet();
            requestPending = false;
            writeRequest = WriteRequest.None;
            if (status == 1 || status == 5) buyOutcomes[listingID] = status;
            else buyOutcomes.Remove(listingID);
            statusText.text = status == 1 ? "Purchased " + quantity.ToString(CultureInfo.InvariantCulture) + " × " +
                    ItemPresentation.Name(itemID) + " for " + amount.ToString(CultureInfo.InvariantCulture) +
                    " diamonds. Select Receive purchases, then check your bag."
                : status == 0 ? "This listing is no longer available. No purchase was made."
                : status == 4 ? "Not enough diamonds. No purchase was made."
                : status == 3 ? "Purchase failed. No purchase was made; try again later."
                : "Purchase result unknown. Reconnect to check; no automatic retry.";
            UpdateControls();
        }

        private void CheckOutbox()
        {
            if (!IsOpen || !WorldAvailable || model == null || loginScreen.Model != model || requestPending) return;
            buyConfirmation = false;
            cancelConfirmation = false;
            requestPending = true;
            writeRequest = WriteRequest.CheckOutbox;
            requestDeadline = Time.unscaledTime + RequestTimeoutSeconds;
            selectedDetailText.text = "Receiving pending purchases and returned items. Open your bag after receiving to check its contents.";
            statusText.text = "Receiving pending items…";
            UpdateControls();
            try { model.RequestBazaarCheckOutbox(); }
            catch (Exception)
            {
                CancelPending();
                statusText.text = "Receipt result incomplete; items may have arrived. Check your bag, then retry manually.";
                UpdateControls();
            }
        }

        private void CheckOutboxReceived(byte status, uint delivered, uint duplicated, uint blocked)
        {
            if (writeRequest != WriteRequest.CheckOutbox || !requestPending || model == null ||
                loginScreen.Model != model || !WorldAvailable) return;
            requestPending = false;
            writeRequest = WriteRequest.None;
            if (inventoryScreen != null) inventoryScreen.RefreshAfterDelivery();
            statusText.text = status == 1 ? "Received " + delivered.ToString(CultureInfo.InvariantCulture) +
                " · already present " + duplicated.ToString(CultureInfo.InvariantCulture) + " · pending " +
                blocked.ToString(CultureInfo.InvariantCulture) + ". Check your bag."
                : "Receipt result incomplete; items may have arrived. Check your bag, then retry manually.";
            UpdateControls();
        }

        private void ClaimSelectedListing()
        {
            if (view != MarketView.MyListings || !IsOpen || !WorldAvailable || model == null || loginScreen.Model != model ||
                requestPending || selectedListing.ListingID == 0 || selectedListing.Status != 1 ||
                claimOutcomes.ContainsKey(selectedListing.ListingID) || unresolvedClaims.Contains(selectedListing.ListingID)) return;
            requestPending = true;
            writeRequest = WriteRequest.Claim;
            unresolvedClaims.Add(selectedListing.ListingID);
            requestDeadline = Time.unscaledTime + RequestTimeoutSeconds;
            statusText.text = "Collecting sale proceeds…";
            UpdateControls();
            try { model.RequestBazaarClaim(selectedListing.ListingID); }
            catch (Exception)
            {
                CancelPending();
                statusText.text = "Collection result unknown. Reconnect to check; no automatic retry.";
                UpdateControls();
            }
        }

        private void ClaimReceived(ulong listingID, byte status, ulong amount)
        {
            if (writeRequest != WriteRequest.Claim || !requestPending || listingID != selectedListing.ListingID || model == null ||
                loginScreen.Model != model || !WorldAvailable) return;
            RefreshWallet();
            requestPending = false;
            writeRequest = WriteRequest.None;
            if (status != 0) unresolvedClaims.Remove(listingID);
            if (status == 1 || status == 4) claimOutcomes[listingID] = status;
            statusText.text = status == 1 ? "Collected " + amount.ToString(CultureInfo.InvariantCulture) + " diamonds."
                : status == 4 ? "Already collected or no pending proceeds were found."
                : status == 2 ? "No sold listing owned by you was found."
                : status == 3 ? "Collection failed. No proceeds were paid; try again later."
                : "Collection result unknown. Reconnect to check; no automatic retry.";
            UpdateControls();
        }

        private static TMP_Text AddCell(Transform parent, string name, string value, float left, float right,
            TextAlignmentOptions alignment, int fontSize, Color color)
        {
            var cellObject = new GameObject(name, typeof(RectTransform), typeof(TextMeshProUGUI));
            cellObject.transform.SetParent(parent, false);
            var rect = cellObject.GetComponent<RectTransform>();
            rect.anchorMin = new Vector2(left, 0);
            rect.anchorMax = new Vector2(right, 1);
            rect.offsetMin = rect.offsetMax = Vector2.zero;
            var text = cellObject.GetComponent<TextMeshProUGUI>();
            text.font = TMP_Settings.defaultFontAsset;
            text.fontSize = fontSize;
            text.color = color;
            text.text = value;
            text.alignment = alignment;
            text.enableWordWrapping = false;
            text.overflowMode = TextOverflowModes.Ellipsis;
            text.raycastTarget = false;
            return text;
        }

        private void CancelPending()
        {
            saleInventoryReplyReady = false;
            buyConfirmation = false;
            cancelConfirmation = false;
            if (!requestPending) return;
            if (writeRequest == WriteRequest.Register)
            {
                registrationUncertain = true;
                if (inventoryScreen != null) inventoryScreen.RequireFreshInventoryOnOpen();
            }
            if ((writeRequest == WriteRequest.CancelListing || writeRequest == WriteRequest.CheckOutbox) && inventoryScreen != null)
                inventoryScreen.RequireFreshInventoryOnOpen();
            requestPending = false;
            writeRequest = WriteRequest.None;
            if (model != null) model.CancelBazaar();
        }

        private void ClearRows()
        {
            selectedRowButton = null;
            selectedListing = default;
            saleItem = default;
            buyConfirmation = false;
            cancelConfirmation = false;
            registerConfirmation = false;
            if (listingRows == null) return;
            for (int i = listingRows.childCount - 1; i >= 0; i--)
            {
                GameObject row = listingRows.GetChild(i).gameObject;
                row.SetActive(false);
                Destroy(row);
            }
            var scroll = listingRows.GetComponentInParent<ScrollRect>();
            if (scroll != null) scroll.verticalNormalizedPosition = 1;
            if (emptyStateText != null) emptyStateText.gameObject.SetActive(false);
            if (selectedDetailText != null) selectedDetailText.text = view == MarketView.MyListings
                ? "Select a trading listing to cancel its sale, or a sold listing to collect proceeds."
                : "Select a listing to see its details.";
            if (selectedItemIcon != null) { selectedItemIcon.sprite = null; selectedItemIcon.enabled = false; }
            if (selectedItemNameText != null) selectedItemNameText.text = view == MarketView.Sell ? "Select an item" : "Select a listing";
            if (selectedItemQuantityText != null) selectedItemQuantityText.text = string.Empty;
        }

        private void ClearResults()
        {
            ClearRows();
            statusText.text = string.Empty;
            pageText.text = "Page " + ((uint)page + 1).ToString(CultureInfo.InvariantCulture);
        }

        private void UpdateControls()
        {
            bool isSearch = view == MarketView.Browse;
            bool isMine = view == MarketView.MyListings;
            bool isSell = view == MarketView.Sell;
            bool available = WorldAvailable && model != null && loginScreen != null && loginScreen.Model == model;
            if (marketButton != null) marketButton.interactable = available;
            if (cancelListingButton != null)
            {
                cancelListingButton.gameObject.SetActive(isMine);
                cancelListingButton.interactable = available && IsOpen && !requestPending && selectedListing.ListingID != 0 &&
                    selectedListing.Status == 0 && !cancelOutcomes.ContainsKey(selectedListing.ListingID);
                var label = cancelListingButton.GetComponentInChildren<TMP_Text>(true);
                if (label != null) label.text = writeRequest == WriteRequest.CancelListing ? "CANCELLING…"
                    : cancelOutcomes.TryGetValue(selectedListing.ListingID, out byte outcome) ? (outcome == 1 ? "CANCELLED" : "RESULT UNKNOWN")
                    : cancelConfirmation ? "CONFIRM CANCELLATION" : "CANCEL SALE";
            }
            if (buyButton != null)
            {
                buyButton.gameObject.SetActive(isSearch);
                buyButton.interactable = available && IsOpen && !requestPending && selectedListing.ListingID != 0 &&
                    selectedListing.Status == 0 && !buyOutcomes.ContainsKey(selectedListing.ListingID);
                var label = buyButton.GetComponentInChildren<TMP_Text>(true);
                if (label != null) label.text = writeRequest == WriteRequest.Buy ? "PURCHASING…"
                    : buyOutcomes.TryGetValue(selectedListing.ListingID, out byte outcome) ? (outcome == 1 ? "PURCHASED" : "RESULT UNKNOWN")
                    : buyConfirmation ? "CONFIRM PURCHASE" : "BUY SELECTED";
            }
            if (checkOutboxButton != null)
            {
                checkOutboxButton.gameObject.SetActive(!isSell);
                checkOutboxButton.interactable = available && IsOpen && !requestPending;
                var label = checkOutboxButton.GetComponentInChildren<TMP_Text>(true);
                if (label != null) label.text = writeRequest == WriteRequest.CheckOutbox ? "RECEIVING…" : "RECEIVE ITEMS";
            }
            if (claimButton != null)
            {
                claimButton.gameObject.SetActive(isMine);
                claimButton.interactable = available && IsOpen && !requestPending && selectedListing.ListingID != 0 &&
                    selectedListing.Status == 1 && !claimOutcomes.ContainsKey(selectedListing.ListingID) &&
                    !unresolvedClaims.Contains(selectedListing.ListingID);
                var label = claimButton.GetComponentInChildren<TMP_Text>(true);
                if (label != null) label.text = writeRequest == WriteRequest.Claim ? "COLLECTING…"
                    : claimOutcomes.TryGetValue(selectedListing.ListingID, out byte outcome) ? (outcome == 1 ? "COLLECTED" : "NO PROCEEDS")
                    : unresolvedClaims.Contains(selectedListing.ListingID) ? "RESULT UNKNOWN" : "COLLECT PROCEEDS";
            }
            if (searchButton != null)
            {
                searchButton.interactable = available && bazaarPanel.activeSelf && !requestPending;
                var refreshLabel = searchButton.GetComponentInChildren<TMP_Text>(true);
                if (refreshLabel != null) refreshLabel.text = isSell ? "REFRESH BAG" : isMine ? "REFRESH" : "SEARCH";
            }
            if (previousPageButton != null)
            {
                previousPageButton.gameObject.SetActive(isSearch);
                previousPageButton.interactable = available && bazaarPanel.activeSelf && !isMine && !requestPending && page > 0;
            }
            if (nextPageButton != null)
            {
                nextPageButton.gameObject.SetActive(isSearch);
                nextPageButton.interactable = available && bazaarPanel.activeSelf && !isMine && !requestPending && page < ushort.MaxValue;
            }
            if (pageText != null)
            {
                pageText.gameObject.SetActive(isSearch);
                pageText.text = "Page " + ((uint)page + 1).ToString(CultureInfo.InvariantCulture);
            }
            if (searchListingsButton != null)
            {
                searchListingsButton.GetComponent<UnityEngine.UI.Image>().color = isSearch
                    ? new Color32(40, 86, 79, 255) : new Color32(23, 49, 57, 255);
                searchListingsButton.interactable = available && bazaarPanel.activeSelf && writeRequest == WriteRequest.None;
                var label = searchListingsButton.GetComponentInChildren<TMP_Text>(true);
                if (label != null) label.text = (isMine ? string.Empty : "> ") + "SALES";
            }
            if (myListingsButton != null)
            {
                myListingsButton.GetComponent<UnityEngine.UI.Image>().color = isMine
                    ? new Color32(40, 86, 79, 255) : new Color32(23, 49, 57, 255);
                myListingsButton.interactable = available && bazaarPanel.activeSelf && writeRequest == WriteRequest.None;
                var label = myListingsButton.GetComponentInChildren<TMP_Text>(true);
                if (label != null) label.text = (isMine ? "> " : string.Empty) + "MY LISTINGS";
            }
            if (itemTypeButtons != null)
                for (int i = 0; i < itemTypeButtons.Length; i++)
                    if (itemTypeButtons[i] != null)
                    {
                        itemTypeButtons[i].gameObject.SetActive(isSearch);
                        itemTypeButtons[i].interactable = available && bazaarPanel != null && bazaarPanel.activeSelf && !requestPending;
                        var label = itemTypeButtons[i].GetComponentInChildren<TMP_Text>(true);
                        if (label != null) label.text = (itemType == i + 1 ? "> " : string.Empty) +
                            (i == 0 ? "POTIONS" : i == 1 ? "WEAPONS / SHIELDS" : "ARMOR");
                    }
            if (saleForm != null) saleForm.SetActive(isSell);
            var priceHeader = bazaarPanel != null ? bazaarPanel.transform.Find("BazaarParchment/BazaarPriceHeader") : null;
            var stateHeader = bazaarPanel != null ? bazaarPanel.transform.Find("BazaarParchment/BazaarStateHeader") : null;
            if (priceHeader != null) priceHeader.GetComponent<TMP_Text>().text = isSell ? "BAG SLOT" : "TOTAL D";
            if (stateHeader != null) stateHeader.gameObject.SetActive(!isSell);
            if (sellItemsButton != null)
            {
                sellItemsButton.GetComponent<UnityEngine.UI.Image>().color = isSell
                    ? new Color32(40, 86, 79, 255) : new Color32(23, 49, 57, 255);
                sellItemsButton.interactable = available && IsOpen && !requestPending;
            }
            if (registerButton != null)
            {
                registerButton.interactable = available && isSell && !requestPending && !registrationUncertain && saleItem.ItemID != 0;
                var label = registerButton.GetComponentInChildren<TMP_Text>(true);
                if (label != null) label.text = registrationUncertain ? "CHECK MY LISTINGS" : registerConfirmation ? "CONFIRM SALE" : "REVIEW SALE";
            }
            if (quantityInput != null) quantityInput.interactable = !requestPending && !registrationUncertain;
            if (priceInput != null) priceInput.interactable = !requestPending && !registrationUncertain;
            if (closeButton != null) closeButton.interactable = true;
        }

        private void RefreshWallet()
        {
            if (!WorldAvailable || model == null || loginScreen.Model != model) return;
            if (walletPending) { walletRefreshQueued = true; return; }
            walletRequested = walletPending = true;
            goldKnown = diamondKnown = false;
            walletDeadline = Time.unscaledTime + RequestTimeoutSeconds;
            ShowWallet();
            try { model.RequestWallet(); }
            catch (Exception)
            {
                walletPending = false;
                ShowWallet();
                ScheduleWalletRetry();
            }
        }

        private void WalletReceived(byte goldStatus, ulong goldBalance, byte diamondStatus, ulong diamondBalance)
        {
            if (!walletPending || model == null || loginScreen.Model != model || !WorldAvailable) return;
            walletPending = false;
            goldKnown = goldStatus == 1;
            diamondKnown = diamondStatus == 1;
            gold = goldBalance;
            diamonds = diamondBalance;
            ShowWallet();
            UpdateSaleSummary();
            if (!goldKnown || !diamondKnown || walletRefreshQueued) ScheduleWalletRetry();
        }

        private void ScheduleWalletRetry()
        {
            // Startup failures and missing responses share the same bounded read retry budget.
            // ClientCore retains ownership of outstanding request IDs and their cancellation.
            if (walletRefreshQueued)
            {
                walletRefreshQueued = false;
                walletRetryAt = Time.unscaledTime + .01f;
            }
            else if ((!goldKnown || !diamondKnown) && walletRetries < 2)
            {
                walletRetries++;
                walletRetryAt = Time.unscaledTime + 1f;
            }
        }

        private void ShowWallet()
        {
            if (walletGoldText != null) walletGoldText.text = goldKnown ? gold.ToString("N0", CultureInfo.InvariantCulture) : walletPending ? "…" : "--";
            if (walletDiamondText != null) walletDiamondText.text = diamondKnown ? diamonds.ToString("N0", CultureInfo.InvariantCulture) : walletPending ? "…" : "--";
        }

        private void SelectSellItems()
        {
            if (!IsOpen || !WorldAvailable || model == null || requestPending) return;
            view = MarketView.Sell;
            saleItem = default;
            registerConfirmation = false;
            LoadSaleItems();
        }

        private void LoadSaleItems()
        {
            if (view != MarketView.Sell || requestPending || !WorldAvailable || model == null || loginScreen.Model != model) return;
            ClearRows();
            saleItem = default;
            requestPending = true;
            requestDeadline = Time.unscaledTime + RequestTimeoutSeconds;
            statusText.text = "Loading items from your bag…";
            UpdateSaleSummary();
            UpdateControls();
            try
            {
                if (inventoryScreen != null && inventoryScreen.isActiveAndEnabled)
                {
                    if (!inventoryScreen.RequestSharedInventory(model))
                        throw new InvalidOperationException("Inventory is unavailable for this session.");
                }
                else model.RequestInventory();
            }
            catch (Exception) { requestPending = false; statusText.text = "Bag lookup failed. Refresh to retry."; UpdateControls(); }
        }

        private void SaleInventoryReceived(bool success)
        {
            if (view != MarketView.Sell || !IsOpen || !requestPending || writeRequest != WriteRequest.None || model == null || loginScreen.Model != model || !WorldAvailable) return;
            if (inventoryScreen != null && inventoryScreen.InventoryRequestPending)
            {
                saleInventoryReplyReady = true;
                requestDeadline = Time.unscaledTime + RequestTimeoutSeconds;
                return;
            }
            saleInventoryReplyReady = false;
            requestPending = false;
            ClearRows();
            var items = success && loginScreen.Data != null ? loginScreen.Data.GetInventory() : null;
            if (items == null) { statusText.text = "Bag lookup failed. Refresh to retry."; UpdateControls(); return; }
            int index = 0;
            foreach (var item in items)
            {
                if (item.ItemID == 0 || item.Quantity == 0) continue;
                var row = CreateItemRow(item.ItemID, item.Quantity, index++);
                AddCell(row.transform, "Slot", "Slot " + item.Slot, .62f, .99f, TextAlignmentOptions.MidlineRight, 15, new Color(.8f, .85f, .9f));
                var selected = item;
                var button = row.GetComponent<UnityEngine.UI.Button>();
                button.onClick.AddListener(() =>
                {
                    if (requestPending || registrationUncertain) return;
                    SelectRow(button);
                    saleItem = selected;
                    quantityInput.SetTextWithoutNotify("1");
                    registerConfirmation = false;
                    ShowSelectedItem(selected.ItemID, selected.Quantity);
                    selectedDetailText.text = "Slot " + selected.Slot.ToString(CultureInfo.InvariantCulture) + " selected";
                    UpdateSaleSummary();
                    UpdateControls();
                });
            }
            emptyStateText.gameObject.SetActive(index == 0);
            emptyStateText.text = "Your bag has no items to sell.";
            statusText.text = registrationUncertain ? "Previous sale outcome unknown. Check My listings and refresh your bag; do not repeat blindly."
                : "Choose an item, quantity and total diamond price.";
            UpdateControls();
        }

        private void SaleInputChanged(string value)
        {
            registerConfirmation = false;
            UpdateSaleSummary();
            UpdateControls();
        }

        private bool TrySaleValues(out ushort quantity, out ulong price)
        {
            quantity = 0; price = 0;
            return saleItem.ItemID != 0 && quantityInput != null && priceInput != null &&
                ushort.TryParse(quantityInput.text, NumberStyles.None, CultureInfo.InvariantCulture, out quantity) && quantity > 0 && quantity <= saleItem.Quantity &&
                ulong.TryParse(priceInput.text, NumberStyles.None, CultureInfo.InvariantCulture, out price) && price > 0;
        }

        private void UpdateSaleSummary()
        {
            if (saleSummaryText == null) return;
            bool valid = TrySaleValues(out _, out ulong price);
            if (!valid)
            {
                if (saleFeeValueText != null) saleFeeValueText.text = "--";
                saleSummaryText.text = saleItem.ItemID == 0 ? "Select an item and enter its total price." : "Enter a valid quantity and total price.";
                return;
            }
            ulong fee = Math.Max(10UL, price / 10UL);
            if (saleFeeValueText != null) saleFeeValueText.text = fee.ToString("N0", CultureInfo.InvariantCulture);
            saleSummaryText.text = goldKnown && gold < fee ? "Not enough gold." :
                !goldKnown ? "Server will check the gold fee." : "Gold fee only.";
        }

        private void ShowSelectedItem(uint itemID, ushort quantity)
        {
            if (selectedItemNameText != null) selectedItemNameText.text = ItemPresentation.Name(itemID);
            if (selectedItemQuantityText != null) selectedItemQuantityText.text = "× " + quantity.ToString(CultureInfo.InvariantCulture);
            if (selectedItemIcon != null)
            {
                selectedItemIcon.sprite = ItemPresentation.Icon(itemID);
                selectedItemIcon.enabled = selectedItemIcon.sprite != null;
            }
        }

        private static void AddCurrencyMark(Transform parent, string name, bool goldMark, float anchorX, Sprite icon)
        {
            var mark = new GameObject(name, typeof(RectTransform), typeof(UnityEngine.UI.Image));
            mark.transform.SetParent(parent, false);
            var rect = mark.GetComponent<RectTransform>();
            rect.anchorMin = rect.anchorMax = new Vector2(anchorX, .5f);
            rect.pivot = new Vector2(.5f, .5f);
            rect.sizeDelta = icon != null ? new Vector2(16, 16) : goldMark ? new Vector2(14, 8) : new Vector2(10, 10);
            rect.localRotation = icon != null || goldMark ? Quaternion.identity : Quaternion.Euler(0, 0, 45);
            var image = mark.GetComponent<UnityEngine.UI.Image>();
            image.sprite = icon;
            image.preserveAspect = true;
            image.color = icon != null ? Color.white : goldMark ? new Color(.91f, .70f, .31f) : new Color(.47f, .79f, .89f);
            image.raycastTarget = false;
        }

        private void RegisterSale()
        {
            if (view != MarketView.Sell || !IsOpen || !WorldAvailable || model == null || loginScreen.Model != model || requestPending || registrationUncertain) return;
            if (!TrySaleValues(out ushort quantity, out ulong price))
            { statusText.text = "Choose a bag item, valid quantity and positive stack total."; return; }
            if (goldKnown && gold < Math.Max(10UL, price / 10UL))
            { statusText.text = "Not enough gold for the registration fee."; return; }
            if (!registerConfirmation)
            { registerConfirmation = true; UpdateSaleSummary(); statusText.text = "Confirm listing: only the displayed gold fee is deducted."; UpdateControls(); return; }
            registrationUncertain = true;
            requestPending = true;
            writeRequest = WriteRequest.Register;
            requestDeadline = Time.unscaledTime + RequestTimeoutSeconds;
            statusText.text = "Registering sale…";
            UpdateControls();
            try { model.RequestBazaarRegister(saleItem.Slot, quantity, price); }
            catch (Exception) { CancelPending(); statusText.text = "Sale outcome unknown. Check My listings and your bag; no automatic retry."; UpdateControls(); }
        }

        private void RegisterReceived(byte status)
        {
            if (!requestPending || writeRequest != WriteRequest.Register || model == null || loginScreen.Model != model || !WorldAvailable) return;
            requestPending = false;
            writeRequest = WriteRequest.None;
            registerConfirmation = false;
            registrationUncertain = status == 5 || status > 5;
            RefreshWallet();
            if (inventoryScreen != null) inventoryScreen.RequireFreshInventoryOnOpen();
            statusText.text = status == 1 ? "Sale listed. Gold fee deducted; diamonds are paid when purchased. Refresh My listings."
                : status == 2 ? "Not enough items in that bag slot. Refresh your bag."
                : status == 3 ? "Not enough gold for the listing fee."
                : status == 4 ? "That item cannot be listed."
                : status == 0 ? "Sale failed and was rolled back. Refresh your bag before retrying."
                : "Sale outcome unknown. Check My listings and your bag; no automatic retry.";
            if (status == 1 && IsOpen) { view = MarketView.MyListings; ClearRows(); RefreshCurrentView(); }
            UpdateControls();
        }

        private static string ItemTypeName(ushort value) => value == 1 ? "potions" : value == 2 ? "weapons / shields" : "armor";

        private static string ListingStatusName(byte status) => status == 0 ? "Trading" : status == 1 ? "Sold" : status == 2 ? "Claimed" : "Status #" + status.ToString(CultureInfo.InvariantCulture);

        private void OnDisable()
        {
            if (sellItemsButton != null) sellItemsButton.onClick.RemoveListener(SelectSellItems);
            if (registerButton != null) registerButton.onClick.RemoveListener(RegisterSale);
            if (quantityInput != null) quantityInput.onValueChanged.RemoveListener(SaleInputChanged);
            if (priceInput != null) priceInput.onValueChanged.RemoveListener(SaleInputChanged);
            if (loginScreen != null) loginScreen.ClientChanged -= ChangeClient;
            if (worldScreen != null) worldScreen.WorldStateChanged -= WorldChanged;
            if (marketButton != null) marketButton.onClick.RemoveListener(TogglePanel);
            if (itemTypeButtons != null && itemTypeButtons.Length == 3)
            {
                if (itemTypeButtons[0] != null) itemTypeButtons[0].onClick.RemoveListener(SelectPotion);
                if (itemTypeButtons[1] != null) itemTypeButtons[1].onClick.RemoveListener(SelectWeaponShield);
                if (itemTypeButtons[2] != null) itemTypeButtons[2].onClick.RemoveListener(SelectArmor);
            }
            if (searchListingsButton != null) searchListingsButton.onClick.RemoveListener(SelectSearchListings);
            if (myListingsButton != null) myListingsButton.onClick.RemoveListener(SelectMyListings);
            if (searchButton != null) searchButton.onClick.RemoveListener(RefreshCurrentView);
            if (previousPageButton != null) previousPageButton.onClick.RemoveListener(PreviousPage);
            if (nextPageButton != null) nextPageButton.onClick.RemoveListener(NextPage);
            if (closeButton != null) closeButton.onClick.RemoveListener(ClosePanel);
            if (claimButton != null) claimButton.onClick.RemoveListener(ClaimSelectedListing);
            if (buyButton != null) buyButton.onClick.RemoveListener(BuySelectedListing);
            if (cancelListingButton != null) cancelListingButton.onClick.RemoveListener(CancelSelectedListing);
            if (checkOutboxButton != null) checkOutboxButton.onClick.RemoveListener(CheckOutbox);
            if (model != null)
            {
                model.OnWalletReceived -= WalletReceived;
                model.OnInventoryReceived -= SaleInventoryReceived;
                model.OnBazaarRegisterReceived -= RegisterReceived;
                model.OnBazaarReceived -= BazaarReceived;
                model.OnBazaarClaimReceived -= ClaimReceived;
                model.OnBazaarBuyReceived -= BuyReceived;
                model.OnBazaarCancelListingReceived -= CancelListingReceived;
                model.OnBazaarCheckOutboxReceived -= CheckOutboxReceived;
            }
            CancelPending();
            if (listingRows != null && statusText != null && pageText != null) ClearResults();
            if (bazaarPanel != null) bazaarPanel.SetActive(false);
        }
    }
}
