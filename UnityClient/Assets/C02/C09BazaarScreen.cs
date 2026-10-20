using System;
using System.Collections.Generic;
using System.Globalization;
using C01;
using ClientCore;
using TMPro;
using UnityEngine;
using UnityEngine.UI;

namespace C02
{
    public sealed class C09BazaarScreen : MonoBehaviour
    {
        private const float RequestTimeoutSeconds = 8f;
        [SerializeField] private C01LoginScreen loginScreen;
        [SerializeField] private C02WorldScreen worldScreen;
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
        [SerializeField] private C07InventoryScreen inventoryScreen;
        [SerializeField] private TMP_Text statusText;
        private IViewModelUI model;
        private ushort itemType = 1;
        private ushort page;
        private bool myListings;
        private float requestDeadline;
        private bool requestPending;
        private enum WriteRequest { None, Claim, Buy, CheckOutbox, CancelListing }
        private WriteRequest writeRequest;
        private BazaarListingView selectedListing;
        private bool buyConfirmation;
        private bool cancelConfirmation;
        private readonly Dictionary<ulong, byte> cancelOutcomes = new Dictionary<ulong, byte>();
        private readonly Dictionary<ulong, byte> buyOutcomes = new Dictionary<ulong, byte>();
        private readonly Dictionary<ulong, byte> claimOutcomes = new Dictionary<ulong, byte>();
        private readonly HashSet<ulong> unresolvedClaims = new HashSet<ulong>();
        private UnityEngine.UI.Button selectedListingButton;
        public bool IsOpen => bazaarPanel != null && bazaarPanel.activeSelf;

        private bool WorldAvailable => worldScreen != null && worldScreen.isActiveAndEnabled &&
            worldScreen.Stage == C02Stage.InWorld && worldScreen.HasOwnFull && worldScreen.IsWorldVisible;

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
            loginScreen.ClientChanged += ChangeClient;
            worldScreen.WorldStateChanged += WorldChanged;
            ChangeClient(loginScreen.Model);
        }

        private void Update()
        {
            if (!requestPending || Time.unscaledTime < requestDeadline) return;
            WriteRequest previousWrite = writeRequest;
            CancelPending();
            if (previousWrite == WriteRequest.None) ClearRows();
            statusText.text = previousWrite == WriteRequest.Claim ? "Collection result unknown. Check again after reconnecting; no automatic retry."
                : previousWrite == WriteRequest.Buy ? "Purchase result unknown. Reconnect to check; no automatic retry."
                : previousWrite == WriteRequest.CancelListing ? "Cancellation result unknown. Refresh my listings and receive items; no automatic retry."
                : previousWrite == WriteRequest.CheckOutbox ? "Receipt result incomplete; items may have arrived. Check your bag, then retry manually."
                : myListings
                ? "My listings timed out. Select Refresh to retry."
                : "Search timed out. Select Search to retry.";
            UpdateControls();
        }

        private void ChangeClient(IViewModelUI next)
        {
            if (model != null)
            {
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
            page = 0;
            myListings = false;
            ClearResults();
            bazaarPanel.SetActive(false);
            if (model != null)
            {
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
            if (!available)
            {
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
            myListings = false;
            bazaarPanel.SetActive(true);
            ClearResults();
            Search();
        }

        private void SelectSearchListings()
        {
            SelectView(false);
        }

        private void SelectMyListings()
        {
            SelectView(true);
        }

        private void SelectView(bool showMyListings)
        {
            if (!bazaarPanel.activeSelf || !WorldAvailable || model == null || loginScreen.Model != model)
                return;
            if (writeRequest != WriteRequest.None) return;
            if (myListings == showMyListings) return;
            CancelPending();
            myListings = showMyListings;
            ClearResults();
            UpdateControls();
            RefreshCurrentView();
        }

        private void RefreshCurrentView()
        {
            if (myListings)
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
            if (myListings || !bazaarPanel.activeSelf || !WorldAvailable || model == null || loginScreen.Model != model || requestPending)
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
            CancelPending();
            myListings = false;
            bazaarPanel.SetActive(false);
            ClearResults();
            UpdateControls();
        }

        private void BazaarReceived(bool success)
        {
            if (!requestPending || writeRequest != WriteRequest.None || model == null || loginScreen.Model != model || !WorldAvailable || !bazaarPanel.activeSelf)
                return;
            requestPending = false;
            List<BazaarListingView> rows = success && loginScreen.Data != null
                ? loginScreen.Data.GetBazaar()
                : new List<BazaarListingView>();
            if (!success)
            {
                ClearRows();
                statusText.text = myListings
                    ? "My listings failed. Select Refresh to retry."
                    : "Search failed. Select Search to retry.";
                UpdateControls();
                return;
            }
            if (rows == null || rows.Count > 30)
            {
                ClearRows();
                statusText.text = myListings
                    ? "My listings response was invalid. Select Refresh to retry."
                    : "Search response was invalid. Select Search to retry.";
                UpdateControls();
                return;
            }

            ClearRows();
            for (int i = 0; i < rows.Count; i++) CreateListingRow(rows[i], i);
            emptyStateText.gameObject.SetActive(rows.Count == 0);
            emptyStateText.text = myListings ? "You have no listings." : "No listings on this page.";
            statusText.text = myListings
                ? rows.Count == 0 ? "You have no listings." : "My listings: " + rows.Count.ToString(CultureInfo.InvariantCulture) + " listings · up to 30 shown"
                : rows.Count == 0 ? "No listings in " + ItemTypeName(itemType) + " on this page." :
                    rows.Count.ToString(CultureInfo.InvariantCulture) + " listings · " + ItemTypeName(itemType);
            UpdateControls();
        }

        private void CreateListingRow(BazaarListingView listing, int index)
        {
            var rowObject = new GameObject("Listing-" + listing.ListingID.ToString(CultureInfo.InvariantCulture),
                typeof(RectTransform), typeof(UnityEngine.UI.Image), typeof(UnityEngine.UI.Button), typeof(LayoutElement));
            rowObject.transform.SetParent(listingRows, false);
            var image = rowObject.GetComponent<UnityEngine.UI.Image>();
            image.color = index % 2 == 0 ? new Color(.97f, .93f, .82f) : new Color(.92f, .87f, .73f);
            var button = rowObject.GetComponent<UnityEngine.UI.Button>();
            button.targetGraphic = image;
            var layout = rowObject.GetComponent<LayoutElement>();
            layout.minHeight = layout.preferredHeight = 52;

            var accent = new GameObject("ItemAccent", typeof(RectTransform), typeof(UnityEngine.UI.Image));
            accent.transform.SetParent(rowObject.transform, false);
            var accentRect = accent.GetComponent<RectTransform>();
            accentRect.anchorMin = new Vector2(0, 0);
            accentRect.anchorMax = new Vector2(0, 1);
            accentRect.pivot = new Vector2(0, .5f);
            accentRect.sizeDelta = new Vector2(5, 0);
            accent.GetComponent<UnityEngine.UI.Image>().color = C11ItemPresentation.Accent(listing.ItemID);
            accent.GetComponent<UnityEngine.UI.Image>().raycastTarget = false;

            AddCell(rowObject.transform, "Badge", C11ItemPresentation.Badge(listing.ItemID), 0.02f, 0.09f,
                TextAlignmentOptions.Center, 10, C11ItemPresentation.Accent(listing.ItemID));
            AddCell(rowObject.transform, "Item", C11ItemPresentation.Name(listing.ItemID), 0.10f, 0.48f,
                TextAlignmentOptions.MidlineLeft, 13, new Color(.22f, .17f, .10f));
            AddCell(rowObject.transform, "Quantity", listing.Quantity.ToString(CultureInfo.InvariantCulture), 0.49f, 0.61f,
                TextAlignmentOptions.Center, 12, new Color(.26f, .22f, .15f));
            AddCell(rowObject.transform, "Price", listing.Price.ToString(CultureInfo.InvariantCulture), 0.62f, 0.81f,
                TextAlignmentOptions.MidlineRight, 12, new Color(.20f, .25f, .16f));
            AddCell(rowObject.transform, "State", ListingStatusName(listing.Status), 0.82f, 0.99f,
                TextAlignmentOptions.Center, 11, new Color(.30f, .23f, .14f));
            button.onClick.AddListener(() => SelectListing(listing, button));
        }

        private void SelectListing(BazaarListingView listing, UnityEngine.UI.Button button)
        {
            if (writeRequest != WriteRequest.None) return;
            if (selectedListingButton != null)
            {
                var previous = selectedListingButton.GetComponent<UnityEngine.UI.Image>();
                if (previous != null) previous.color = new Color(.97f, .93f, .82f);
            }
            selectedListingButton = button;
            selectedListing = listing;
            buyConfirmation = false;
            cancelConfirmation = false;
            button.GetComponent<UnityEngine.UI.Image>().color = new Color(.82f, .77f, .62f);
            selectedDetailText.text = C11ItemPresentation.Name(listing.ItemID) + "  ·  " + ListingStatusName(listing.Status) +
                "\nQuantity " + listing.Quantity.ToString(CultureInfo.InvariantCulture) +
                "  ·  Price " + listing.Price.ToString(CultureInfo.InvariantCulture) +
                "\nListing #" + listing.ListingID.ToString(CultureInfo.InvariantCulture) +
                "  ·  Seller #" + listing.SellerCharacterID.ToString(CultureInfo.InvariantCulture) +
                "  ·  Registered " + listing.RegisteredAt.ToString(CultureInfo.InvariantCulture);
            if (claimOutcomes.TryGetValue(listing.ListingID, out byte outcome)) selectedDetailText.text += outcome == 1 ? "\nProceeds collected." : "\nNo pending proceeds were found.";
            else if (unresolvedClaims.Contains(listing.ListingID)) selectedDetailText.text += "\nCollection result unknown. Reconnect to check.";
            if (buyOutcomes.TryGetValue(listing.ListingID, out byte buyOutcome))
                selectedDetailText.text += buyOutcome == 1 ? "\nPurchased. Use Receive purchases, then check your bag." : "\nPurchase result unknown. Reconnect to check.";
            if (cancelOutcomes.TryGetValue(listing.ListingID, out byte cancelOutcome))
                selectedDetailText.text += cancelOutcome == 1 ? "\nSale cancelled. Receive items and check your bag." : "\nCancellation result unknown. Refresh my listings and receive items.";
            UpdateControls();
        }

        private void CancelSelectedListing()
        {
            if (!myListings || !IsOpen || !WorldAvailable || model == null || loginScreen.Model != model ||
                requestPending || selectedListing.ListingID == 0 || selectedListing.Status != 0 ||
                cancelOutcomes.ContainsKey(selectedListing.ListingID)) return;
            if (!cancelConfirmation)
            {
                cancelConfirmation = true;
                selectedDetailText.text = "Cancel sale of " + C11ItemPresentation.Name(selectedListing.ItemID) + " × " +
                    selectedListing.Quantity.ToString(CultureInfo.InvariantCulture) + "?\nThe listing fee is not refunded.\nCancellation and item return are separate.\nSelect CONFIRM CANCELLATION to continue.";
                statusText.text = "Confirm cancellation of your selected sale.";
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
                model == null || loginScreen.Model != model || !WorldAvailable || !IsOpen) return;
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
            if (myListings || !IsOpen || !WorldAvailable || model == null || loginScreen.Model != model ||
                requestPending || selectedListing.ListingID == 0 || selectedListing.Status != 0 ||
                buyOutcomes.ContainsKey(selectedListing.ListingID)) return;
            if (!buyConfirmation)
            {
                buyConfirmation = true;
                selectedDetailText.text = "Buy " + C11ItemPresentation.Name(selectedListing.ItemID) + " × " +
                    selectedListing.Quantity.ToString(CultureInfo.InvariantCulture) + "?\nPrice " +
                    selectedListing.Price.ToString(CultureInfo.InvariantCulture) + " diamonds.\nPurchase and item receipt are separate.\nSelect CONFIRM PURCHASE to continue.";
                statusText.text = "Confirm the selected price before buying.";
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
                model == null || loginScreen.Model != model || !WorldAvailable || !IsOpen) return;
            requestPending = false;
            writeRequest = WriteRequest.None;
            if (status == 1 || status == 5) buyOutcomes[listingID] = status;
            else buyOutcomes.Remove(listingID);
            statusText.text = status == 1 ? "Purchased " + quantity.ToString(CultureInfo.InvariantCulture) + " × " +
                    C11ItemPresentation.Name(itemID) + " for " + amount.ToString(CultureInfo.InvariantCulture) +
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
                loginScreen.Model != model || !WorldAvailable || !IsOpen) return;
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
            if (!myListings || !IsOpen || !WorldAvailable || model == null || loginScreen.Model != model ||
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
                loginScreen.Model != model || !WorldAvailable || !IsOpen) return;
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
            buyConfirmation = false;
            cancelConfirmation = false;
            if (!requestPending) return;
            if (writeRequest == WriteRequest.CancelListing && inventoryScreen != null)
                inventoryScreen.RequireFreshInventoryOnOpen();
            requestPending = false;
            writeRequest = WriteRequest.None;
            if (model != null) model.CancelBazaar();
        }

        private void ClearRows()
        {
            selectedListingButton = null;
            selectedListing = default;
            buyConfirmation = false;
            cancelConfirmation = false;
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
            if (selectedDetailText != null) selectedDetailText.text = myListings
                ? "Select a trading listing to cancel its sale, or a sold listing to collect proceeds."
                : "Select a listing to see its details.";
        }

        private void ClearResults()
        {
            ClearRows();
            statusText.text = string.Empty;
            pageText.text = "Page " + ((uint)page + 1).ToString(CultureInfo.InvariantCulture);
        }

        private void UpdateControls()
        {
            bool available = WorldAvailable && model != null && loginScreen != null && loginScreen.Model == model;
            if (marketButton != null) marketButton.interactable = available;
            if (cancelListingButton != null)
            {
                cancelListingButton.gameObject.SetActive(myListings);
                cancelListingButton.interactable = available && IsOpen && !requestPending && selectedListing.ListingID != 0 &&
                    selectedListing.Status == 0 && !cancelOutcomes.ContainsKey(selectedListing.ListingID);
                var label = cancelListingButton.GetComponentInChildren<TMP_Text>(true);
                if (label != null) label.text = writeRequest == WriteRequest.CancelListing ? "CANCELLING…"
                    : cancelOutcomes.TryGetValue(selectedListing.ListingID, out byte outcome) ? (outcome == 1 ? "CANCELLED" : "RESULT UNKNOWN")
                    : cancelConfirmation ? "CONFIRM CANCELLATION" : "CANCEL SALE";
            }
            if (buyButton != null)
            {
                buyButton.gameObject.SetActive(!myListings);
                buyButton.interactable = available && IsOpen && !requestPending && selectedListing.ListingID != 0 &&
                    selectedListing.Status == 0 && !buyOutcomes.ContainsKey(selectedListing.ListingID);
                var label = buyButton.GetComponentInChildren<TMP_Text>(true);
                if (label != null) label.text = writeRequest == WriteRequest.Buy ? "PURCHASING…"
                    : buyOutcomes.TryGetValue(selectedListing.ListingID, out byte outcome) ? (outcome == 1 ? "PURCHASED" : "RESULT UNKNOWN")
                    : buyConfirmation ? "CONFIRM PURCHASE" : "BUY SELECTED";
            }
            if (checkOutboxButton != null)
            {
                checkOutboxButton.interactable = available && IsOpen && !requestPending;
                var label = checkOutboxButton.GetComponentInChildren<TMP_Text>(true);
                if (label != null) label.text = writeRequest == WriteRequest.CheckOutbox ? "RECEIVING…" : "RECEIVE ITEMS";
            }
            if (claimButton != null)
            {
                claimButton.gameObject.SetActive(myListings);
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
                if (refreshLabel != null) refreshLabel.text = myListings ? "REFRESH" : "SEARCH";
            }
            if (previousPageButton != null)
            {
                previousPageButton.gameObject.SetActive(!myListings);
                previousPageButton.interactable = available && bazaarPanel.activeSelf && !myListings && !requestPending && page > 0;
            }
            if (nextPageButton != null)
            {
                nextPageButton.gameObject.SetActive(!myListings);
                nextPageButton.interactable = available && bazaarPanel.activeSelf && !myListings && !requestPending && page < ushort.MaxValue;
            }
            if (pageText != null)
            {
                pageText.gameObject.SetActive(!myListings);
                pageText.text = "Page " + ((uint)page + 1).ToString(CultureInfo.InvariantCulture);
            }
            if (searchListingsButton != null)
            {
                searchListingsButton.interactable = available && bazaarPanel.activeSelf && writeRequest == WriteRequest.None;
                var label = searchListingsButton.GetComponentInChildren<TMP_Text>(true);
                if (label != null) label.text = (myListings ? string.Empty : "> ") + "SALES";
            }
            if (myListingsButton != null)
            {
                myListingsButton.interactable = available && bazaarPanel.activeSelf && writeRequest == WriteRequest.None;
                var label = myListingsButton.GetComponentInChildren<TMP_Text>(true);
                if (label != null) label.text = (myListings ? "> " : string.Empty) + "MY LISTINGS";
            }
            if (itemTypeButtons != null)
                for (int i = 0; i < itemTypeButtons.Length; i++)
                    if (itemTypeButtons[i] != null)
                    {
                        itemTypeButtons[i].gameObject.SetActive(!myListings);
                        itemTypeButtons[i].interactable = available && bazaarPanel != null && bazaarPanel.activeSelf && !requestPending;
                        var label = itemTypeButtons[i].GetComponentInChildren<TMP_Text>(true);
                        if (label != null) label.text = (itemType == i + 1 ? "> " : string.Empty) +
                            (i == 0 ? "POTIONS" : i == 1 ? "WEAPONS / SHIELDS" : "ARMOR");
                    }
            if (closeButton != null) closeButton.interactable = true;
        }

        private static string ItemTypeName(ushort value) => value == 1 ? "potions" : value == 2 ? "weapons / shields" : "armor";

        private static string ListingStatusName(byte status) => status == 0 ? "Trading" : status == 1 ? "Sold" : status == 2 ? "Claimed" : "Status #" + status.ToString(CultureInfo.InvariantCulture);

        private void OnDisable()
        {
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
