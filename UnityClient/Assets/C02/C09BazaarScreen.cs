using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text;
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
        [SerializeField] private Button searchButton;
        [SerializeField] private Button previousPageButton;
        [SerializeField] private Button nextPageButton;
        [SerializeField] private Button closeButton;
        [SerializeField] private TMP_Text pageText;
        [SerializeField] private TMP_Text rowsText;
        [SerializeField] private TMP_Text statusText;
        private IViewModelUI model;
        private ushort itemType = 1;
        private ushort page;
        private float requestDeadline;
        private bool requestPending;
        public bool IsOpen => bazaarPanel != null && bazaarPanel.activeSelf;

        private bool WorldAvailable => worldScreen != null && worldScreen.isActiveAndEnabled &&
            worldScreen.Stage == C02Stage.InWorld && worldScreen.HasOwnFull && worldScreen.IsWorldVisible;

        private void OnEnable()
        {
            if (loginScreen == null || worldScreen == null || bazaarPanel == null || marketButton == null ||
                itemTypeButtons == null || itemTypeButtons.Length != 3 || searchButton == null ||
                previousPageButton == null || nextPageButton == null || closeButton == null ||
                pageText == null || rowsText == null || statusText == null)
            {
                Debug.LogError("C09 bazaar references are missing.", this);
                enabled = false;
                return;
            }

            marketButton.onClick.AddListener(TogglePanel);
            itemTypeButtons[0].onClick.AddListener(SelectPotion);
            itemTypeButtons[1].onClick.AddListener(SelectWeaponShield);
            itemTypeButtons[2].onClick.AddListener(SelectArmor);
            searchButton.onClick.AddListener(Search);
            previousPageButton.onClick.AddListener(PreviousPage);
            nextPageButton.onClick.AddListener(NextPage);
            closeButton.onClick.AddListener(ClosePanel);
            loginScreen.ClientChanged += ChangeClient;
            worldScreen.WorldStateChanged += WorldChanged;
            ChangeClient(loginScreen.Model);
        }

        private void Update()
        {
            if (!requestPending || Time.unscaledTime < requestDeadline) return;
            CancelPending();
            rowsText.text = string.Empty;
            statusText.text = "Search timed out. Select Search to retry.";
            UpdateControls();
        }

        private void ChangeClient(IViewModelUI next)
        {
            if (model != null) model.OnBazaarReceived -= BazaarReceived;
            CancelPending();
            model = next;
            page = 0;
            ClearResults();
            bazaarPanel.SetActive(false);
            if (model != null) model.OnBazaarReceived += BazaarReceived;
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
            bazaarPanel.SetActive(true);
            ClearResults();
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
            if (!bazaarPanel.activeSelf || !WorldAvailable || model == null || loginScreen.Model != model || requestPending)
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

        private void ClosePanel()
        {
            CancelPending();
            bazaarPanel.SetActive(false);
            ClearResults();
            UpdateControls();
        }

        private void BazaarReceived(bool success)
        {
            if (!requestPending || model == null || loginScreen.Model != model || !WorldAvailable || !bazaarPanel.activeSelf)
                return;
            requestPending = false;
            List<BazaarListingView> rows = success && loginScreen.Data != null
                ? loginScreen.Data.GetBazaar()
                : new List<BazaarListingView>();
            if (!success)
            {
                ClearRows();
                statusText.text = "Search failed. Select Search to retry.";
                UpdateControls();
                return;
            }
            if (rows == null || rows.Count > 30)
            {
                ClearRows();
                statusText.text = "Search response was invalid. Select Search to retry.";
                UpdateControls();
                return;
            }

            var text = new StringBuilder(rows.Count * 100);
            foreach (BazaarListingView listing in rows)
            {
                if (text.Length != 0) text.Append('\n');
                text.Append("LISTING ").Append(listing.ListingID.ToString(CultureInfo.InvariantCulture))
                    .Append("   ITEM ").Append(listing.ItemID.ToString(CultureInfo.InvariantCulture))
                    .Append("   QTY ").Append(listing.Quantity.ToString(CultureInfo.InvariantCulture))
                    .Append("   PRICE ").Append(listing.Price.ToString(CultureInfo.InvariantCulture))
                    .Append("   SELLER ").Append(listing.SellerCharacterID.ToString(CultureInfo.InvariantCulture))
                    .Append("   REGISTERED ").Append(listing.RegisteredAt.ToString(CultureInfo.InvariantCulture))
                    .Append("   STATUS ").Append(listing.Status.ToString(CultureInfo.InvariantCulture));
            }
            rowsText.text = text.ToString();
            statusText.text = rows.Count == 0 ? "No listings in " + ItemTypeName(itemType) + " on this page." :
                rows.Count.ToString(CultureInfo.InvariantCulture) + " listings · " + ItemTypeName(itemType);
            UpdateControls();
        }

        private void CancelPending()
        {
            if (!requestPending) return;
            requestPending = false;
            if (model != null) model.CancelBazaar();
        }

        private void ClearRows() => rowsText.text = string.Empty;

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
            if (searchButton != null) searchButton.interactable = available && bazaarPanel.activeSelf && !requestPending;
            if (previousPageButton != null) previousPageButton.interactable = available && bazaarPanel.activeSelf && !requestPending && page > 0;
            if (nextPageButton != null) nextPageButton.interactable = available && bazaarPanel.activeSelf && !requestPending && page < ushort.MaxValue;
            if (itemTypeButtons != null)
                for (int i = 0; i < itemTypeButtons.Length; i++)
                    if (itemTypeButtons[i] != null)
                    {
                        itemTypeButtons[i].interactable = available && bazaarPanel != null && bazaarPanel.activeSelf && !requestPending;
                        var label = itemTypeButtons[i].GetComponentInChildren<TMP_Text>(true);
                        if (label != null) label.text = (itemType == i + 1 ? "> " : string.Empty) +
                            (i == 0 ? "POTIONS" : i == 1 ? "WEAPONS / SHIELDS" : "ARMOR");
                    }
            if (closeButton != null) closeButton.interactable = true;
            if (pageText != null) pageText.text = "Page " + ((uint)page + 1).ToString(CultureInfo.InvariantCulture);
        }

        private static string ItemTypeName(ushort value) => value == 1 ? "potions" : value == 2 ? "weapons / shields" : "armor";

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
            if (searchButton != null) searchButton.onClick.RemoveListener(Search);
            if (previousPageButton != null) previousPageButton.onClick.RemoveListener(PreviousPage);
            if (nextPageButton != null) nextPageButton.onClick.RemoveListener(NextPage);
            if (closeButton != null) closeButton.onClick.RemoveListener(ClosePanel);
            if (model != null) model.OnBazaarReceived -= BazaarReceived;
            CancelPending();
            if (rowsText != null && statusText != null && pageText != null) ClearResults();
            if (bazaarPanel != null) bazaarPanel.SetActive(false);
        }
    }
}
