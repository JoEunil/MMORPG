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
    public sealed class C07InventoryScreen : MonoBehaviour
    {
        private const float RequestTimeoutSeconds = 8f;
        private static readonly Color CardColor = new Color(.94f, .89f, .76f, 1f);
        private static readonly Color SelectedCardColor = new Color(.99f, .96f, .86f, 1f);
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
        [SerializeField] private TMP_Text detailName;
        [SerializeField] private TMP_Text detailSlot;
        [SerializeField] private TMP_Text detailQuantity;
        [SerializeField] private TMP_Text inventoryStatus;
        private IViewModelUI model;
        private Button selectedCard;
        private bool requestPending;
        private bool requestTimedOut;
        private float requestDeadline;
        private bool hasInventoryResponse;

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
            if (model != null) model.OnInventoryReceived -= InventoryReceived;
            model = next;
            requestPending = false;
            requestTimedOut = false;
            hasInventoryResponse = false;
            ClearInventoryCards();
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
            if (open)
            {
                if (bazaarScreen != null) bazaarScreen.ClosePanel();
                worldScreen.StopActions();
            }
            inventoryPanel.SetActive(open);
            if (open && !hasInventoryResponse && !requestTimedOut && !requestPending)
                RequestInventory();
            UpdateControls();
        }

        public void ClosePanel()
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
            if (!success)
            {
                hasInventoryResponse = false;
                ClearInventoryCards();
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
                typeof(Image), typeof(Button));
            cardObject.transform.SetParent(inventoryGridContent, false);
            var background = cardObject.GetComponent<Image>();
            background.color = CardColor;
            var button = cardObject.GetComponent<Button>();
            button.targetGraphic = background;

            var accent = new GameObject("ItemAccent", typeof(RectTransform), typeof(CanvasRenderer), typeof(Image));
            accent.transform.SetParent(cardObject.transform, false);
            var accentRect = accent.GetComponent<RectTransform>();
            SetTopLeft(accentRect, 6f, 6f, 28f, 28f);
            var accentImage = accent.GetComponent<Image>();
            accentImage.color = C11ItemPresentation.Accent(item.ItemID);
            accentImage.raycastTarget = false;

            var badge = CreateText("ItemBadge", accent.transform, C11ItemPresentation.Badge(item.ItemID), 10,
                Color.white, TextAlignmentOptions.Center);
            Stretch(badge.rectTransform, 0);

            var name = CreateText("ItemName", cardObject.transform, C11ItemPresentation.Name(item.ItemID), 13,
                new Color(.23f, .19f, .13f), TextAlignmentOptions.MidlineLeft);
            SetTopRight(name.rectTransform, 40f, 5f, 5f, 32f);
            name.enableWordWrapping = true;
            name.overflowMode = TextOverflowModes.Ellipsis;

            var quantity = CreateText("ItemQuantity", cardObject.transform,
                "Qty " + item.Quantity.ToString(CultureInfo.InvariantCulture), 11,
                new Color(.39f, .31f, .20f), TextAlignmentOptions.MidlineLeft);
            SetTopRight(quantity.rectTransform, 40f, 5f, 39f, 20f);

            button.onClick.AddListener(() => SelectItem(item, button));
        }

        private void SelectItem(InventoryItemView item, Button card)
        {
            if (!IsOpen || !card.isActiveAndEnabled) return;
            selectedCard = card;
            foreach (Transform child in inventoryGridContent)
            {
                var button = child.GetComponent<Button>();
                if (button != null) button.GetComponent<Image>().color = button == selectedCard ? SelectedCardColor : CardColor;
            }
            detailBadge.text = C11ItemPresentation.Badge(item.ItemID);
            detailBadge.color = C11ItemPresentation.Accent(item.ItemID);
            detailName.text = C11ItemPresentation.Name(item.ItemID);
            detailSlot.text = "Slot " + item.Slot.ToString(CultureInfo.InvariantCulture);
            detailQuantity.text = "Quantity " + item.Quantity.ToString(CultureInfo.InvariantCulture);
        }

        private void ClearInventoryCards()
        {
            selectedCard = null;
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
            if (detailBadge != null) detailBadge.text = string.Empty;
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

        private static void SetTopRight(RectTransform rect, float left, float right, float top, float height)
        {
            rect.anchorMin = new Vector2(0, 1);
            rect.anchorMax = new Vector2(1, 1);
            rect.pivot = new Vector2(.5f, 1);
            rect.anchoredPosition = new Vector2((left - right) * .5f, -top);
            rect.sizeDelta = new Vector2(-left - right, height);
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
            hasInventoryResponse = false;
            ClearInventoryCards();
            if (inventoryPanel != null) inventoryPanel.SetActive(false);
        }
    }
}
