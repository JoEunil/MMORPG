using System;
using System.Collections.Generic;
using System.Globalization;
using C01;
using ClientCore;
using TMPro;
using UnityEngine;
using UnityEngine.UI;
using UnityEngine.EventSystems;
using UnityEngine.InputSystem;

namespace C02
{
    public sealed class C07InventoryScreen : MonoBehaviour
    {
        private const float RequestTimeoutSeconds = 8f;
        private const float FailureRetryDelaySeconds = .35f;
        private static readonly Color CardColor = new Color(.16f, .18f, .20f, 1f);
        private static readonly Color SelectedCardColor = new Color(.38f, .30f, .20f, 1f);
        [SerializeField] private C01LoginScreen loginScreen;
        [SerializeField] private C02WorldScreen worldScreen;
        [SerializeField] private C09BazaarScreen bazaarScreen;
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
        private IViewModelUI model;
        private bool requestPending;
        private bool requestTimedOut;
        private float requestDeadline;
        private bool failureRetryScheduled;
        private bool failureRetryAttempted;
        private float failureRetryDeadline;
        private bool hasInventoryResponse;
        private bool refreshAfterCurrentRequest;
        private bool refreshOnOpenAfterUncertainDelivery;

        public bool IsOpen => inventoryPanel != null && inventoryPanel.activeSelf;

        private bool WorldAvailable => worldScreen != null && worldScreen.isActiveAndEnabled &&
            worldScreen.Stage == C02Stage.InWorld && worldScreen.HasOwnFull && worldScreen.IsWorldVisible;

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
            loginScreen.ClientChanged += ChangeClient;
            worldScreen.WorldStateChanged += WorldChanged;
            ChangeClient(loginScreen.Model);
        }

        private void Update()
        {
            if (Application.isFocused && Keyboard.current != null)
            {
                if (IsOpen && Keyboard.current.escapeKey.wasPressedThisFrame) ClosePanel();
                else if (Keyboard.current.iKey.wasPressedThisFrame && EventSystem.current != null)
                {
                    var selected = EventSystem.current.currentSelectedGameObject;
                    var input = selected != null ? selected.GetComponentInParent<TMP_InputField>() : null;
                    if (input == null || !input.isFocused) TogglePanel();
                }
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
            if (model == next)
            {
                if (model != null) model.OnInventoryReceived += InventoryReceived;
                if (requestTimedOut) inventoryStatus.text = "Request timed out. Reconnect to try again.";
                WorldChanged();
                return;
            }
            CancelFailureRetry();
            if (model != null) model.OnInventoryReceived -= InventoryReceived;
            model = next;
            requestPending = false;
            requestTimedOut = false;
            hasInventoryResponse = false;
            refreshAfterCurrentRequest = false;
            refreshOnOpenAfterUncertainDelivery = false;
            ClearInventoryCards();
            inventoryStatus.text = string.Empty;
            inventoryPanel.SetActive(false);
            if (model != null) model.OnInventoryReceived += InventoryReceived;
            WorldChanged();
        }

        private void WorldChanged()
        {
            bool available = WorldAvailable && model != null && loginScreen.Model == model;
            if (!available || !IsOpen) CancelFailureRetry();
            bagButton.gameObject.SetActive(available);
            if (!available) inventoryPanel.SetActive(false);
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
            requestDeadline = Time.unscaledTime + RequestTimeoutSeconds;
            ClearInventoryCards();
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
            if (!WorldAvailable && worldScreen.Stage != C02Stage.TransitionPending) return;
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
                    inventoryStatus.text = worldScreen.Stage == C02Stage.TransitionPending
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
            foreach (InventoryItemView item in snapshot) AddInventoryCard(item);
            inventoryScroll.StopMovement();
            inventoryScroll.verticalNormalizedPosition = 1f;
            hasInventoryResponse = true;
            inventoryStatus.text = worldScreen.Stage == C02Stage.TransitionPending
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
            var button = cardObject.GetComponent<UnityEngine.UI.Button>();
            button.targetGraphic = background;
            button.transition = Selectable.Transition.None;

            var accent = new GameObject("ItemAccent", typeof(RectTransform), typeof(CanvasRenderer), typeof(UnityEngine.UI.Image));
            accent.transform.SetParent(cardObject.transform, false);
            var accentRect = accent.GetComponent<RectTransform>();
            SetTopLeft(accentRect, 32f, 6f, 56f, 56f);
            var accentImage = accent.GetComponent<UnityEngine.UI.Image>();
            Sprite icon = C11ItemPresentation.Icon(item.ItemID);
            accentImage.sprite = icon;
            accentImage.preserveAspect = icon != null;
            accentImage.color = icon != null ? Color.white : C11ItemPresentation.Accent(item.ItemID);
            accentImage.raycastTarget = false;

            var badge = CreateText("ItemBadge", accent.transform, C11ItemPresentation.Badge(item.ItemID), 14,
                Color.white, TextAlignmentOptions.Center);
            Stretch(badge.rectTransform, 0);
            badge.gameObject.SetActive(icon == null);

            var quantityBadge = new GameObject("QuantityBadge", typeof(RectTransform), typeof(CanvasRenderer),
                typeof(UnityEngine.UI.Image));
            quantityBadge.transform.SetParent(cardObject.transform, false);
            var quantityBadgeImage = quantityBadge.GetComponent<UnityEngine.UI.Image>();
            quantityBadgeImage.color = new Color(.08f, .10f, .12f, .98f);
            quantityBadgeImage.raycastTarget = false;
            var quantityBadgeRect = quantityBadge.GetComponent<RectTransform>();
            quantityBadgeRect.anchorMin = quantityBadgeRect.anchorMax = quantityBadgeRect.pivot = new Vector2(1, 1);
            quantityBadgeRect.anchoredPosition = new Vector2(-8f, -44f);
            quantityBadgeRect.sizeDelta = new Vector2(44f, 22f);

            var name = CreateText("ItemName", cardObject.transform, C11ItemPresentation.Name(item.ItemID), 14,
                new Color(.88f, .91f, .95f), TextAlignmentOptions.MidlineLeft);
            name.rectTransform.anchorMin = new Vector2(0, 0);
            name.rectTransform.anchorMax = new Vector2(1, 0);
            name.rectTransform.pivot = new Vector2(.5f, 0);
            name.rectTransform.anchoredPosition = new Vector2(0, 5f);
            name.rectTransform.sizeDelta = new Vector2(-10f, 22f);
            name.enableWordWrapping = false;
            name.overflowMode = TextOverflowModes.Ellipsis;

            var quantity = CreateText("ItemQuantity", cardObject.transform,
                item.Quantity.ToString(CultureInfo.InvariantCulture), 14,
                new Color(.98f, .91f, .76f), TextAlignmentOptions.Center);
            quantity.rectTransform.anchorMin = quantity.rectTransform.anchorMax = quantity.rectTransform.pivot = new Vector2(1, 1);
            quantity.rectTransform.anchoredPosition = quantityBadgeRect.anchoredPosition;
            quantity.rectTransform.sizeDelta = quantityBadgeRect.sizeDelta;

            button.onClick.AddListener(() => SelectItem(item, button));
        }

        private void SelectItem(InventoryItemView item, Button card)
        {
            if (!IsOpen || !card.isActiveAndEnabled) return;
            foreach (Transform child in inventoryGridContent)
            {
                var button = child.GetComponent<UnityEngine.UI.Button>();
                if (button != null) button.GetComponent<UnityEngine.UI.Image>().color = button == card ? SelectedCardColor : CardColor;
            }
            Sprite icon = C11ItemPresentation.Icon(item.ItemID);
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
            detailBadge.text = icon != null ? string.Empty : C11ItemPresentation.Badge(item.ItemID);
            detailBadge.color = C11ItemPresentation.Accent(item.ItemID);
            detailName.text = C11ItemPresentation.Name(item.ItemID);
            detailSlot.text = "Slot " + item.Slot.ToString(CultureInfo.InvariantCulture);
            detailQuantity.text = "Quantity " + item.Quantity.ToString(CultureInfo.InvariantCulture);
        }

        private void ClearInventoryCards()
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
        }

        private void UpdateControls()
        {
            bool available = WorldAvailable && model != null && loginScreen != null && loginScreen.Model == model;
            if (bagButton != null) bagButton.interactable = available;
            if (refreshButton != null) refreshButton.interactable = available && !requestPending && !requestTimedOut;
            if (closeButton != null) closeButton.interactable = true;
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
            if (model != null) model.OnInventoryReceived -= InventoryReceived;
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
