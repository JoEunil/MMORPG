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
    [UnityEngine.Scripting.APIUpdating.MovedFrom(true, sourceNamespace: "C02", sourceAssembly: "C02.Runtime", sourceClassName: "C07InventoryScreen")]
    public sealed class InventoryScreen : MonoBehaviour
    {
        private const float RequestTimeoutSeconds = 8f;
        private const float FailureRetryDelaySeconds = .35f;
        private static readonly Color CardColor = new Color32(21, 42, 48, 255);
        private static readonly Color SelectedCardColor = new Color32(40, 86, 79, 255);
        [SerializeField] private LoginScreen loginScreen;
        [SerializeField] private WorldScreen worldScreen;
        [SerializeField] private MarketScreen bazaarScreen;
        [SerializeField] private GameObject inventoryPanel;
        [SerializeField] private Button bagButton;
        [SerializeField] private Button refreshButton;
        [SerializeField] private Button closeButton;
        [SerializeField] private ScrollRect inventoryScroll;
        [SerializeField] private RectTransform inventoryGridContent;
        [SerializeField] private TMP_Text detailBadge;
        private UnityEngine.UI.Image detailIcon;
        [SerializeField] private TMP_Text detailName;
        [SerializeField] private TMP_Text detailSlot;
        [SerializeField] private TMP_Text detailQuantity;
        [SerializeField] private TMP_Text inventoryStatus;
        [SerializeField] private UnityEngine.UI.Button useButton;
        [SerializeField] private TMP_Text useStatus;
        [SerializeField] private TMP_Text lootNotice;
        private IViewModelUI model;
        private float lootNoticeDeadline;
        private bool usePending;
        private bool useTimedOut;
        private float useDeadline;
        private float useCooldownDeadline;
        private bool requestPending;
        private bool requestTimedOut;
        private float requestDeadline;
        private bool failureRetryScheduled;
        private bool failureRetryAttempted;
        private float failureRetryDeadline;
        private bool hasInventoryResponse;
        private bool refreshAfterCurrentRequest;
        private bool refreshOnOpenAfterUncertainDelivery;
        private bool hasSelectedItem;
        private byte selectedSlot;
        private uint selectedItemID;

        public bool IsOpen => inventoryPanel != null && inventoryPanel.activeSelf;
        public bool InventoryRequestPending => requestPending;
        public bool HasInventoryResponse => hasInventoryResponse;

        // Both panels consume the same uncorrelated inventory reply; keep one existing read owner.
        public bool RequestSharedInventory(IViewModelUI source)
        {
            if (source != model || !WorldAvailable || requestTimedOut) return false;
            if (!requestPending) RequestInventory();
            return requestPending;
        }

        private bool WorldAvailable => worldScreen != null && worldScreen.isActiveAndEnabled &&
            worldScreen.Stage == WorldStage.InWorld && worldScreen.HasOwnFull && worldScreen.IsWorldVisible;

        private void OnEnable()
        {
            if (loginScreen == null || worldScreen == null || inventoryPanel == null || bagButton == null ||
                refreshButton == null || closeButton == null || inventoryScroll == null ||
                inventoryGridContent == null || detailBadge == null || detailName == null ||
                detailSlot == null || detailQuantity == null || inventoryStatus == null)
            {
                Debug.LogError("C07 inventory references are missing.", this);
                enabled = false;
                return;
            }

            bagButton.onClick.AddListener(TogglePanel);
            refreshButton.onClick.AddListener(RequestInventory);
            closeButton.onClick.AddListener(ClosePanel);
            if (useButton != null) useButton.onClick.AddListener(UseSelectedItem);
            loginScreen.ClientChanged += ChangeClient;
            worldScreen.WorldStateChanged += WorldChanged;
            ChangeClient(loginScreen.Model);
        }

        private void Update()
        {
            if (lootNoticeDeadline > 0 && (!WorldAvailable || model == null || loginScreen == null ||
                loginScreen.Model != model || Time.unscaledTime >= lootNoticeDeadline))
                ClearLootNotice();
            if (usePending && !useTimedOut && Time.unscaledTime >= useDeadline)
            {
                useTimedOut = true;
                SetUseStatus("Use result unknown. Checking the bag; reconnect before trying again.");
                RequireFreshInventoryOnOpen();
                RefreshAfterDelivery();
                UpdateControls();
            }
            if (useCooldownDeadline > 0 && Time.unscaledTime >= useCooldownDeadline)
            {
                useCooldownDeadline = 0;
                UpdateControls();
            }
            if (Application.isFocused && Keyboard.current != null)
            {
                if (GameUiInput.EscapeAvailableForPanel() && IsOpen) ClosePanel();
                else if (Keyboard.current.iKey.wasPressedThisFrame && GameUiInput.CanUseWorldShortcuts()) TogglePanel();
            }

            if (failureRetryScheduled)
            {
                if (!IsOpen || !WorldAvailable || model == null || loginScreen.Model != model)
                {
                    CancelFailureRetry();
                }
                else if (Time.unscaledTime >= failureRetryDeadline)
                {
                    CancelFailureRetry();
                    SendInventoryRequest();
                }
            }

            if (!requestPending || Time.unscaledTime < requestDeadline) return;
            requestPending = false;
            requestTimedOut = true;
            ClearInventoryCards();
            inventoryStatus.text = "Request timed out. Reconnect to try again.";
            UpdateControls();
        }

        private void ChangeClient(IViewModelUI next)
        {
            SetModelSubscriptions(false);
            if (model == next)
            {
                SetModelSubscriptions(true);
                if (requestTimedOut) inventoryStatus.text = "Request timed out. Reconnect to try again.";
                WorldChanged();
                return;
            }
            CancelFailureRetry();
            model = next;
            ResetItemUse();
            requestPending = false;
            requestTimedOut = false;
            hasInventoryResponse = false;
            refreshAfterCurrentRequest = false;
            refreshOnOpenAfterUncertainDelivery = false;
            ClearInventoryCards();
            inventoryStatus.text = string.Empty;
            inventoryPanel.SetActive(false);
            SetModelSubscriptions(true);
            WorldChanged();
        }

        private void SetModelSubscriptions(bool subscribe)
        {
            if (model == null) return;
            model.OnInventoryReceived -= InventoryReceived;
            model.OnItemUseReceived -= ItemUseReceived;
            model.OnItemRewardReceived -= ItemRewardReceived;
            model.OnZoneTransitionStarted -= ItemUseZoneTransitionStarted;
            model.OnConnectionFailed -= ItemUseConnectionFailed;
            if (!subscribe) return;
            model.OnInventoryReceived += InventoryReceived;
            model.OnItemUseReceived += ItemUseReceived;
            model.OnItemRewardReceived += ItemRewardReceived;
            model.OnZoneTransitionStarted += ItemUseZoneTransitionStarted;
            model.OnConnectionFailed += ItemUseConnectionFailed;
        }

        private void ResetItemUse()
        {
            usePending = false;
            useTimedOut = false;
            useCooldownDeadline = 0;
            SetUseStatus(string.Empty);
            ClearLootNotice();
        }

        private void ItemUseZoneTransitionStarted(ulong ownershipEpoch, ushort zoneID)
        {
            ResetItemUse();
            hasInventoryResponse = false;
            RequireFreshInventoryOnOpen();
            if (requestPending) refreshAfterCurrentRequest = true;
            UpdateControls();
        }

        private void ItemUseConnectionFailed(string message)
        {
            ResetItemUse();
            hasInventoryResponse = false;
            RequireFreshInventoryOnOpen();
            UpdateControls();
        }

        private bool CanUseSelectedItem()
        {
            if (!IsOpen || !WorldAvailable || model == null || loginScreen == null || loginScreen.Model != model ||
                !hasInventoryResponse || requestPending || requestTimedOut || usePending ||
                Time.unscaledTime < useCooldownDeadline || !hasSelectedItem || selectedItemID != 1 || loginScreen.Data == null)
                return false;
            foreach (InventoryItemView item in loginScreen.Data.GetInventory())
                if (item.Slot == selectedSlot && item.ItemID == selectedItemID && item.Quantity > 0) return true;
            return false;
        }

        private void UseSelectedItem()
        {
            if (!CanUseSelectedItem()) return;
            usePending = true;
            useTimedOut = false;
            useDeadline = Time.unscaledTime + RequestTimeoutSeconds;
            SetUseStatus("Using HP potion…");
            UpdateControls();
            try
            {
                if (model.RequestItemUse(selectedItemID)) return;
                usePending = false;
                SetUseStatus("Cannot use this item now. Reconnect if another use is unresolved.");
            }
            catch (Exception)
            {
                // A send failure does not prove whether the server received the mutation.
                useTimedOut = true;
                SetUseStatus("Use result unknown. Checking the bag; reconnect before trying again.");
                RequireFreshInventoryOnOpen();
                RefreshAfterDelivery();
            }
            UpdateControls();
        }

        private void ItemUseReceived(ulong requestID, ulong ownershipEpoch, uint itemID, byte status,
            ushort remainingQuantity, uint cooldownRemainingTicks)
        {
            // ClientCore matches request ID and ownership before dispatching this event.
            if (!usePending || model == null || loginScreen.Model != model || itemID != 1) return;
            usePending = false;
            useTimedOut = false;
            useCooldownDeadline = Time.unscaledTime + cooldownRemainingTicks * .05f;
            string result;
            switch (status)
            {
                case 0: result = "Invalid item use request."; break;
                case 1: result = "HP potion used."; break;
                case 2: result = "Cannot use a potion while dead."; break;
                case 3: result = "HP is already full."; break;
                case 4: result = "Potion is on cooldown."; break;
                case 5: result = "No HP potions remain."; break;
                case 6: result = "Another item operation is in progress."; break;
                case 7: result = "World changed. Open the bag again."; break;
                case 8: result = "Item storage failed. Try again later."; break;
                case 9: result = "This item cannot be used."; break;
                default: result = "Unknown item use result."; break;
            }
            if (cooldownRemainingTicks > 0)
                result += " Cooldown " + (cooldownRemainingTicks * .05f).ToString("0.0", CultureInfo.InvariantCulture) + "s.";
            SetUseStatus(result);
            if (status == 1 || status == 5 || status == 7)
            {
                if (!IsOpen) RequireFreshInventoryOnOpen();
                if (IsOpen || requestPending) RefreshAfterDelivery();
            }
            UpdateControls();
        }

        private void ItemRewardReceived(uint itemID, ushort grantedQuantity, ushort remainingQuantity, byte status)
        {
            if (model == null || loginScreen.Model != model) return;
            switch (status)
            {
                case 1: SetUseStatus("Received " + ItemPresentation.Name(itemID) + " ×" + grantedQuantity.ToString(CultureInfo.InvariantCulture) + "."); break;
                case 2: ShowLootFailure("Bag full. Item reward could not be added."); break;
                case 3: ShowLootFailure("Item reward failed."); break;
                default: SetUseStatus("Unknown item reward result."); break;
            }
            if (status == 1)
            {
                if (!IsOpen) RequireFreshInventoryOnOpen();
                if (IsOpen || requestPending) RefreshAfterDelivery();
            }
            UpdateControls();
        }

        private void SetUseStatus(string message)
        {
            if (useStatus != null) useStatus.text = message;
        }

        private void ShowLootFailure(string message)
        {
            SetUseStatus(message);
            if (lootNotice == null || !WorldAvailable) return;
            lootNotice.text = message;
            lootNoticeDeadline = Time.unscaledTime + 6f;
        }

        private void ClearLootNotice()
        {
            lootNoticeDeadline = 0;
            if (lootNotice != null) lootNotice.text = string.Empty;
        }

        private void WorldChanged()
        {
            bool available = WorldAvailable && model != null && loginScreen.Model == model;
            if (!available || !IsOpen) CancelFailureRetry();
            bagButton.gameObject.SetActive(available);
            if (!available)
            {
                ClearLootNotice();
                inventoryPanel.SetActive(false);
                if (worldScreen == null || worldScreen.Stage != WorldStage.TransitionPending)
                {
                    hasInventoryResponse = false;
                    ClearInventoryCards();
                }
            }
            UpdateControls();
        }

        private void TogglePanel()
        {
            if (!WorldAvailable || model == null || loginScreen.Model != model) return;
            bool open = !inventoryPanel.activeSelf;
            if (open)
            {
                if (bazaarScreen != null) bazaarScreen.ClosePanel();
                worldScreen.StopActions();
            }
            else CancelFailureRetry();
            inventoryPanel.SetActive(open);
            if (open) inventoryPanel.transform.SetAsLastSibling();
            if (open && (!hasInventoryResponse || refreshOnOpenAfterUncertainDelivery) && !requestTimedOut && !requestPending)
                RequestInventory();
            UpdateControls();
        }

        public void ClosePanel()
        {
            CancelFailureRetry();
            inventoryPanel.SetActive(false);
            UpdateControls();
        }

        public void RefreshAfterDelivery()
        {
            hasInventoryResponse = false;
            // Inventory replies have no request ID: finish an older read before issuing the fresh one.
            if (requestPending) refreshAfterCurrentRequest = true;
            else RequestInventory();
        }

        public void RequireFreshInventoryOnOpen()
        {
            // A lost mutation response can arrive after an earlier bag read; keep manual opens fresh for this model.
            refreshOnOpenAfterUncertainDelivery = true;
        }

        private void RequestInventory()
        {
            CancelFailureRetry();
            failureRetryAttempted = false;
            SendInventoryRequest();
        }

        private void SendInventoryRequest()
        {
            if (!WorldAvailable || model == null || loginScreen.Model != model || requestPending || requestTimedOut)
                return;

            requestPending = true;
            hasInventoryResponse = false;
            requestDeadline = Time.unscaledTime + RequestTimeoutSeconds;
            ClearInventoryCards(preserveSelection: true);
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
                ClearInventoryCards();
                inventoryStatus.text = "Request failed. Reconnect to try again.";
                requestTimedOut = true;
                UpdateControls();
            }
        }

        private void InventoryReceived(bool success)
        {
            if (!requestPending || model == null || loginScreen.Model != model) return;
            if (!WorldAvailable && worldScreen.Stage != WorldStage.TransitionPending) return;
            requestPending = false;
            if (refreshAfterCurrentRequest)
            {
                refreshAfterCurrentRequest = false;
                RequestInventory();
                return;
            }
            if (!success)
            {
                hasInventoryResponse = false;
                ClearInventoryCards();
                if (IsOpen && WorldAvailable && !failureRetryAttempted)
                {
                    failureRetryAttempted = true;
                    failureRetryScheduled = true;
                    failureRetryDeadline = Time.unscaledTime + FailureRetryDelaySeconds;
                    inventoryStatus.text = "Inventory request failed. Checking once more…";
                }
                else
                {
                    inventoryStatus.text = worldScreen.Stage == WorldStage.TransitionPending
                        ? "Inventory request failed during travel. Open the bag and refresh after arrival."
                        : "Inventory request failed. Select Refresh to try again.";
                }
                UpdateControls();
                return;
            }

            List<InventoryItemView> snapshot = loginScreen.Data != null
                ? new List<InventoryItemView>(loginScreen.Data.GetInventory())
                : new List<InventoryItemView>();
            snapshot.Sort((left, right) => left.Slot.CompareTo(right.Slot));
            bool restoredSelection = false;
            foreach (InventoryItemView item in snapshot)
            {
                AddInventoryCard(item);
                if (hasSelectedItem && item.Slot == selectedSlot && item.ItemID == selectedItemID)
                {
                    Button card = inventoryGridContent.GetChild(inventoryGridContent.childCount - 1)
                        .GetComponent<Button>();
                    ApplySelection(item, card);
                    restoredSelection = true;
                }
            }
            if (hasSelectedItem && !restoredSelection) ClearSelectedItem();
            inventoryScroll.StopMovement();
            inventoryScroll.verticalNormalizedPosition = 1f;
            hasInventoryResponse = true;
            inventoryStatus.text = worldScreen.Stage == WorldStage.TransitionPending
                ? "Inventory response saved. Open the bag after arrival."
                : snapshot.Count == 0 ? "No items." : snapshot.Count.ToString(CultureInfo.InvariantCulture) + " items";
            UpdateControls();
        }

        private void AddInventoryCard(InventoryItemView item)
        {
            var cardObject = new GameObject("InventoryItemCard", typeof(RectTransform), typeof(CanvasRenderer),
                typeof(UnityEngine.UI.Image), typeof(UnityEngine.UI.Button));
            cardObject.transform.SetParent(inventoryGridContent, false);
            var background = cardObject.GetComponent<UnityEngine.UI.Image>();
            background.color = CardColor;
            var edge = cardObject.AddComponent<UnityEngine.UI.Outline>();
            edge.effectColor = new Color32(107, 146, 144, 90);
            edge.effectDistance = new Vector2(.7f, -.7f);
            var button = cardObject.GetComponent<UnityEngine.UI.Button>();
            button.targetGraphic = background;
            button.transition = Selectable.Transition.None;

            var accent = new GameObject("ItemAccent", typeof(RectTransform), typeof(CanvasRenderer), typeof(UnityEngine.UI.Image));
            accent.transform.SetParent(cardObject.transform, false);
            var accentRect = accent.GetComponent<RectTransform>();
            SetTopLeft(accentRect, 32f, 6f, 56f, 56f);
            var accentImage = accent.GetComponent<UnityEngine.UI.Image>();
            Sprite icon = ItemPresentation.Icon(item.ItemID);
            accentImage.sprite = icon;
            accentImage.preserveAspect = icon != null;
            accentImage.color = icon != null ? Color.white : ItemPresentation.Accent(item.ItemID);
            accentImage.raycastTarget = false;

            var badge = CreateText("ItemBadge", accent.transform, ItemPresentation.Badge(item.ItemID), 14,
                Color.white, TextAlignmentOptions.Center);
            Stretch(badge.rectTransform, 0);
            badge.gameObject.SetActive(icon == null);

            var quantityBadge = new GameObject("QuantityBadge", typeof(RectTransform), typeof(CanvasRenderer),
                typeof(UnityEngine.UI.Image));
            quantityBadge.transform.SetParent(cardObject.transform, false);
            var quantityBadgeImage = quantityBadge.GetComponent<UnityEngine.UI.Image>();
            quantityBadgeImage.color = new Color32(16, 37, 42, 255);
            quantityBadgeImage.raycastTarget = false;
            var quantityBadgeRect = quantityBadge.GetComponent<RectTransform>();
            quantityBadgeRect.anchorMin = quantityBadgeRect.anchorMax = quantityBadgeRect.pivot = new Vector2(1, 1);
            quantityBadgeRect.anchoredPosition = new Vector2(-8f, -44f);
            quantityBadgeRect.sizeDelta = new Vector2(44f, 22f);

            var name = CreateText("ItemName", cardObject.transform, ItemPresentation.Name(item.ItemID), 14,
                new Color32(236, 240, 232, 255), TextAlignmentOptions.MidlineLeft);
            name.rectTransform.anchorMin = new Vector2(0, 0);
            name.rectTransform.anchorMax = new Vector2(1, 0);
            name.rectTransform.pivot = new Vector2(.5f, 0);
            name.rectTransform.anchoredPosition = new Vector2(0, 5f);
            name.rectTransform.sizeDelta = new Vector2(-10f, 22f);
            name.enableWordWrapping = false;
            name.overflowMode = TextOverflowModes.Ellipsis;

            var quantity = CreateText("ItemQuantity", cardObject.transform,
                item.Quantity.ToString(CultureInfo.InvariantCulture), 14,
                new Color32(255, 242, 209, 255), TextAlignmentOptions.Center);
            quantity.rectTransform.anchorMin = quantity.rectTransform.anchorMax = quantity.rectTransform.pivot = new Vector2(1, 1);
            quantity.rectTransform.anchoredPosition = quantityBadgeRect.anchoredPosition;
            quantity.rectTransform.sizeDelta = quantityBadgeRect.sizeDelta;

            button.onClick.AddListener(() => SelectItem(item, button));
        }

        private void SelectItem(InventoryItemView item, Button card)
        {
            if (!IsOpen || !card.isActiveAndEnabled) return;
            hasSelectedItem = true;
            selectedSlot = item.Slot;
            selectedItemID = item.ItemID;
            ApplySelection(item, card);
            UpdateControls();
        }

        private void ApplySelection(InventoryItemView item, Button card)
        {
            foreach (Transform child in inventoryGridContent)
            {
                var button = child.GetComponent<UnityEngine.UI.Button>();
                if (button != null)
                {
                    button.GetComponent<UnityEngine.UI.Image>().color = button == card ? SelectedCardColor : CardColor;
                    var edge = button.GetComponent<UnityEngine.UI.Outline>();
                    if (edge != null) edge.effectColor = button == card
                        ? new Color32(229, 211, 163, 255) : new Color32(107, 146, 144, 90);
                }
            }
            Sprite icon = ItemPresentation.Icon(item.ItemID);
            if (icon != null)
            {
                if (detailIcon == null)
                {
                    var iconObject = new GameObject("DetailItemIcon", typeof(RectTransform), typeof(CanvasRenderer), typeof(UnityEngine.UI.Image));
                    iconObject.transform.SetParent(detailBadge.transform, false);
                    Stretch(iconObject.GetComponent<RectTransform>(), 0);
                    detailIcon = iconObject.GetComponent<UnityEngine.UI.Image>();
                    detailIcon.preserveAspect = true;
                    detailIcon.raycastTarget = false;
                }
                detailIcon.sprite = icon;
                detailIcon.enabled = true;
            }
            else if (detailIcon != null) detailIcon.enabled = false;
            detailBadge.text = icon != null ? string.Empty : ItemPresentation.Badge(item.ItemID);
            detailBadge.color = ItemPresentation.Accent(item.ItemID);
            detailName.text = ItemPresentation.Name(item.ItemID);
            detailSlot.text = "Slot " + item.Slot.ToString(CultureInfo.InvariantCulture);
            detailQuantity.text = "Quantity " + item.Quantity.ToString(CultureInfo.InvariantCulture);
        }

        private void ClearInventoryCards(bool preserveSelection = false)
        {
            for (int i = inventoryGridContent != null ? inventoryGridContent.childCount - 1 : -1; i >= 0; i--)
            {
                Transform child = inventoryGridContent.GetChild(i);
                if (Application.isPlaying)
                {
                    child.gameObject.SetActive(false);
                    Destroy(child.gameObject);
                }
                else DestroyImmediate(child.gameObject);
            }
            if (detailBadge != null)
            {
                detailBadge.text = string.Empty;
                if (detailIcon != null) detailIcon.enabled = false;
            }
            if (detailName != null) detailName.text = "Select an item";
            if (detailSlot != null) detailSlot.text = string.Empty;
            if (detailQuantity != null) detailQuantity.text = string.Empty;
            if (!preserveSelection) ClearSelectedItem();
        }

        private void ClearSelectedItem()
        {
            hasSelectedItem = false;
            selectedSlot = 0;
            selectedItemID = 0;
        }

        private void UpdateControls()
        {
            bool available = WorldAvailable && model != null && loginScreen != null && loginScreen.Model == model;
            if (bagButton != null) bagButton.interactable = available;
            if (refreshButton != null) refreshButton.interactable = available && !requestPending && !requestTimedOut;
            if (closeButton != null) closeButton.interactable = true;
            if (useButton != null) useButton.interactable = CanUseSelectedItem();
        }

        private static TMP_Text CreateText(string name, Transform parent, string value, float size, Color color,
            TextAlignmentOptions alignment)
        {
            var gameObject = new GameObject(name, typeof(RectTransform), typeof(CanvasRenderer), typeof(TextMeshProUGUI));
            gameObject.transform.SetParent(parent, false);
            var text = gameObject.GetComponent<TextMeshProUGUI>();
            text.font = TMP_Settings.defaultFontAsset;
            text.text = value;
            text.fontSize = size;
            text.color = color;
            text.alignment = alignment;
            text.richText = false;
            text.raycastTarget = false;
            return text;
        }

        private static void Stretch(RectTransform rect, int margin)
        {
            rect.anchorMin = Vector2.zero;
            rect.anchorMax = Vector2.one;
            rect.offsetMin = new Vector2(margin, margin);
            rect.offsetMax = new Vector2(-margin, -margin);
        }

        private static void SetTopLeft(RectTransform rect, float left, float top, float width, float height)
        {
            rect.anchorMin = rect.anchorMax = rect.pivot = new Vector2(0, 1);
            rect.anchoredPosition = new Vector2(left, -top);
            rect.sizeDelta = new Vector2(width, height);
        }

        private void OnDisable()
        {
            CancelFailureRetry();
            if (loginScreen != null) loginScreen.ClientChanged -= ChangeClient;
            if (worldScreen != null) worldScreen.WorldStateChanged -= WorldChanged;
            if (bagButton != null) bagButton.onClick.RemoveListener(TogglePanel);
            if (refreshButton != null) refreshButton.onClick.RemoveListener(RequestInventory);
            if (closeButton != null) closeButton.onClick.RemoveListener(ClosePanel);
            if (useButton != null) useButton.onClick.RemoveListener(UseSelectedItem);
            SetModelSubscriptions(false);
            if (usePending) RequireFreshInventoryOnOpen();
            ResetItemUse();
            if (requestPending)
            {
                requestPending = false;
                requestTimedOut = true;
            }
            hasInventoryResponse = false;
            ClearInventoryCards();
            if (inventoryPanel != null) inventoryPanel.SetActive(false);
        }

        private void CancelFailureRetry()
        {
            failureRetryScheduled = false;
        }
    }
}
