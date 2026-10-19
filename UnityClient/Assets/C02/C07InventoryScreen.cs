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
    public sealed class C07InventoryScreen : MonoBehaviour
    {
        private const float RequestTimeoutSeconds = 8f;
        [SerializeField] private C01LoginScreen loginScreen;
        [SerializeField] private C02WorldScreen worldScreen;
        [SerializeField] private GameObject inventoryPanel;
        [SerializeField] private Button bagButton;
        [SerializeField] private Button refreshButton;
        [SerializeField] private Button closeButton;
        [SerializeField] private TMP_Text inventoryRows;
        [SerializeField] private TMP_Text inventoryStatus;
        private IViewModelUI model;
        private bool requestPending;
        private bool requestTimedOut;
        private float requestDeadline;
        private bool hasInventoryResponse;

        private bool WorldAvailable => worldScreen != null && worldScreen.isActiveAndEnabled &&
            worldScreen.Stage == C02Stage.InWorld && worldScreen.HasOwnFull && worldScreen.IsWorldVisible;

        private void OnEnable()
        {
            if (loginScreen == null || worldScreen == null || inventoryPanel == null || bagButton == null ||
                refreshButton == null || closeButton == null || inventoryRows == null || inventoryStatus == null)
            {
                Debug.LogError("C07 inventory references are missing.", this);
                enabled = false;
                return;
            }

            bagButton.onClick.AddListener(TogglePanel);
            refreshButton.onClick.AddListener(RequestInventory);
            closeButton.onClick.AddListener(ClosePanel);
            loginScreen.ClientChanged += ChangeClient;
            worldScreen.WorldStateChanged += WorldChanged;
            ChangeClient(loginScreen.Model);
        }

        private void Update()
        {
            if (!requestPending || Time.unscaledTime < requestDeadline) return;
            requestPending = false;
            requestTimedOut = true;
            inventoryRows.text = string.Empty;
            inventoryStatus.text = "Request timed out. Reconnect to try again.";
            UpdateControls();
        }

        private void ChangeClient(IViewModelUI next)
        {
            if (model == next)
            {
                if (model != null) model.OnInventoryReceived += InventoryReceived;
                if (requestTimedOut) inventoryStatus.text = "Request timed out. Reconnect to try again.";
                WorldChanged();
                return;
            }
            if (model != null) model.OnInventoryReceived -= InventoryReceived;
            model = next;
            requestPending = false;
            requestTimedOut = false;
            hasInventoryResponse = false;
            inventoryRows.text = string.Empty;
            inventoryStatus.text = string.Empty;
            inventoryPanel.SetActive(false);
            if (model != null) model.OnInventoryReceived += InventoryReceived;
            WorldChanged();
        }

        private void WorldChanged()
        {
            bool available = WorldAvailable && model != null && loginScreen.Model == model;
            bagButton.gameObject.SetActive(available);
            if (!available) inventoryPanel.SetActive(false);
            UpdateControls();
        }

        private void TogglePanel()
        {
            if (!WorldAvailable || model == null || loginScreen.Model != model) return;
            bool open = !inventoryPanel.activeSelf;
            inventoryPanel.SetActive(open);
            if (open && !hasInventoryResponse && !requestTimedOut && !requestPending)
                RequestInventory();
            UpdateControls();
        }

        private void ClosePanel()
        {
            inventoryPanel.SetActive(false);
            UpdateControls();
        }

        private void RequestInventory()
        {
            if (!WorldAvailable || model == null || loginScreen.Model != model || requestPending || requestTimedOut)
                return;

            requestPending = true;
            requestDeadline = Time.unscaledTime + RequestTimeoutSeconds;
            inventoryRows.text = string.Empty;
            inventoryStatus.text = "Requesting inventory…";
            UpdateControls();
            try
            {
                model.RequestInventory();
            }
            catch (Exception)
            {
                requestPending = false;
                hasInventoryResponse = false;
                inventoryRows.text = string.Empty;
                inventoryStatus.text = "Request failed. Reconnect to try again.";
                requestTimedOut = true;
                UpdateControls();
            }
        }

        private void InventoryReceived(bool success)
        {
            if (!requestPending || model == null || loginScreen.Model != model) return;
            if (!WorldAvailable && worldScreen.Stage != C02Stage.TransitionPending) return;
            requestPending = false;
            if (!success)
            {
                hasInventoryResponse = false;
                inventoryRows.text = string.Empty;
                inventoryStatus.text = worldScreen.Stage == C02Stage.TransitionPending
                    ? "Inventory request failed during travel. Open the bag and refresh after arrival."
                    : "Inventory request failed. Select Refresh to try again.";
                UpdateControls();
                return;
            }

            List<InventoryItemView> snapshot = loginScreen.Data != null
                ? new List<InventoryItemView>(loginScreen.Data.GetInventory())
                : new List<InventoryItemView>();
            snapshot.Sort((left, right) => left.Slot.CompareTo(right.Slot));
            var text = new StringBuilder(snapshot.Count * 40);
            foreach (var item in snapshot)
            {
                if (text.Length != 0) text.Append('\n');
                text.Append("SLOT ").Append(item.Slot.ToString("00", CultureInfo.InvariantCulture))
                    .Append("     ITEM ").Append(item.ItemID.ToString(CultureInfo.InvariantCulture))
                    .Append("     QTY ").Append(item.Quantity.ToString(CultureInfo.InvariantCulture));
            }
            inventoryRows.text = text.ToString();
            hasInventoryResponse = true;
            inventoryStatus.text = worldScreen.Stage == C02Stage.TransitionPending
                ? "Inventory response saved. Open the bag after arrival."
                : snapshot.Count == 0 ? "No items." : snapshot.Count.ToString(CultureInfo.InvariantCulture) + " items";
            UpdateControls();
        }

        private void UpdateControls()
        {
            bool available = WorldAvailable && model != null && loginScreen != null && loginScreen.Model == model;
            if (bagButton != null) bagButton.interactable = available;
            if (refreshButton != null) refreshButton.interactable = available && !requestPending && !requestTimedOut;
            if (closeButton != null) closeButton.interactable = true;
        }

        private void OnDisable()
        {
            if (loginScreen != null) loginScreen.ClientChanged -= ChangeClient;
            if (worldScreen != null) worldScreen.WorldStateChanged -= WorldChanged;
            if (bagButton != null) bagButton.onClick.RemoveListener(TogglePanel);
            if (refreshButton != null) refreshButton.onClick.RemoveListener(RequestInventory);
            if (closeButton != null) closeButton.onClick.RemoveListener(ClosePanel);
            if (model != null) model.OnInventoryReceived -= InventoryReceived;
            if (requestPending)
            {
                requestPending = false;
                requestTimedOut = true;
            }
            if (inventoryPanel != null) inventoryPanel.SetActive(false);
        }
    }
}
