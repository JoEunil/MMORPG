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
        [SerializeField] private UnityEngine.UI.Button potionQuickSlot;
        [SerializeField] private TMP_Text potionQuantity;
        [SerializeField] private UnityEngine.UI.Button mpPotionQuickSlot;
        [SerializeField] private TMP_Text mpPotionQuantity;
        [SerializeField] private TMP_Text potionQuickStatus;
        private TMP_Text potionCooldown, mpPotionCooldown;
        private float potionNoticeDeadline;
        private IViewModelUI model;
        private Action<byte> goldRewardReceived;
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
        private bool inventoryWarmupAttempted;
        private bool inventoryWarmupPending;
        private bool refreshAfterCurrentRequest;
        private bool refreshOnOpenAfterUncertainDelivery;
        private UnityEngine.UI.Button equipButton;
        private bool equipmentReadPending, equipmentSetPending, equipmentUncertain;
        private int equipmentReadAttempts;
        private float equipmentDeadline, equipmentNextReadAt;
        private EquipmentStateView equipmentState;
        private string equipmentResult;
        public bool EquipmentKnown { get; private set; }
        public bool EquipmentMutationPending => equipmentSetPending || equipmentUncertain;
        public bool IsItemEquipped(uint itemID) => EquipmentKnown && itemID != 0 &&
            (equipmentState.WeaponID == itemID || equipmentState.ShieldID == itemID || equipmentState.BodyID == itemID);
        public bool CanSellItem(uint itemID) => itemID < 3 || itemID > 5 ||
            (EquipmentKnown && !EquipmentMutationPending && !IsItemEquipped(itemID));
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
            refreshButton.onClick.AddListener(RefreshEquipment);
            closeButton.onClick.AddListener(ClosePanel);
            if (useButton != null) useButton.onClick.AddListener(UseSelectedItem);
            EnsureEquipmentButton();
            EnsurePotionQuickSlots();
            if (potionQuickSlot != null) potionQuickSlot.onClick.AddListener(UseQuickPotion);
            if (mpPotionQuickSlot != null) mpPotionQuickSlot.onClick.AddListener(UseQuickMpPotion);
            loginScreen.ClientChanged += ChangeClient;
            worldScreen.WorldStateChanged += WorldChanged;
            ChangeClient(loginScreen.Model);
        }

        private void Update()
        {
            if (equipmentReadPending && Time.unscaledTime >= equipmentDeadline)
            {
                equipmentReadPending = false;
                EquipmentKnown = false;
                UpdateEquipmentMarkers();
                equipmentResult = "Equipment lookup timed out. Refresh to try again.";
                equipmentNextReadAt = Time.unscaledTime + FailureRetryDelaySeconds;
                UpdateControls();
            }
            if (equipmentSetPending && Time.unscaledTime >= equipmentDeadline)
            {
                equipmentSetPending = false;
                equipmentUncertain = true;
                EquipmentKnown = false;
                UpdateEquipmentMarkers();
                equipmentResult = "Equipment result unknown. Reconnect before changing it again.";
                UpdateControls();
            }
            if (!EquipmentKnown && !equipmentReadPending && !EquipmentMutationPending && equipmentReadAttempts < 2 &&
                Time.unscaledTime >= equipmentNextReadAt && WorldAvailable && model != null && loginScreen.Model == model)
                RequestEquipmentRead();
            // Bootstrap callbacks expose the world before ClientCore marks it ready; read on the next frame.
            if (!inventoryWarmupAttempted && WorldAvailable && model != null && loginScreen.Model == model &&
                !hasInventoryResponse && !requestPending && !requestTimedOut && !usePending)
            {
                inventoryWarmupPending = true;
                RequestInventory();
            }
            if (potionNoticeDeadline > 0 && Time.unscaledTime >= potionNoticeDeadline)
            {
                potionNoticeDeadline = 0;
                if (potionQuickStatus != null) potionQuickStatus.text = string.Empty;
                UpdateControls();
            }
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
            if (useCooldownDeadline > 0) UpdatePotionCooldown();
            if (Application.isFocused && Keyboard.current != null)
            {
                if (GameUiInput.EscapeAvailableForPanel() && IsOpen) ClosePanel();
                else if (Keyboard.current.iKey.wasPressedThisFrame && GameUiInput.CanUseWorldShortcuts()) TogglePanel();
                else if (Keyboard.current.digit1Key.wasPressedThisFrame) UseQuickPotion();
                else if (Keyboard.current.digit2Key.wasPressedThisFrame) UseQuickMpPotion();
            }

            if (failureRetryScheduled)
            {
                if ((!IsOpen && !inventoryWarmupPending) || !WorldAvailable || model == null || loginScreen.Model != model)
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
            inventoryWarmupPending = false;
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
            ResetEquipment();
            ResetItemUse();
            requestPending = false;
            requestTimedOut = false;
            hasInventoryResponse = false;
            inventoryWarmupAttempted = false;
            inventoryWarmupPending = false;
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
            model.OnEquipmentReceived -= EquipmentReceived;
            model.OnInventoryReceived -= InventoryReceived;
            model.OnItemUseReceived -= ItemUseReceived;
            model.OnItemRewardReceived -= ItemRewardReceived;
            model.OnGoldRewardReceived -= goldRewardReceived;
            goldRewardReceived = null;
            model.OnZoneTransitionStarted -= ItemUseZoneTransitionStarted;
            model.OnConnectionFailed -= ItemUseConnectionFailed;
            if (!subscribe) return;
            model.OnEquipmentReceived += EquipmentReceived;
            model.OnInventoryReceived += InventoryReceived;
            model.OnItemUseReceived += ItemUseReceived;
            model.OnItemRewardReceived += ItemRewardReceived;
            var source = model;
            Action<byte> receive = null;
            receive = status =>
            {
                if (status == 0 && isActiveAndEnabled && goldRewardReceived == receive && model == source &&
                    loginScreen.Model == source && WorldAvailable)
                    ShowLootFailure("Gold reward failed.");
            };
            goldRewardReceived = receive;
            model.OnGoldRewardReceived += goldRewardReceived;
            model.OnZoneTransitionStarted += ItemUseZoneTransitionStarted;
            model.OnConnectionFailed += ItemUseConnectionFailed;
        }

        private void EnsureEquipmentButton()
        {
            if (equipButton != null) return;
            var go = new GameObject("InventoryEquipButton", typeof(RectTransform), typeof(UnityEngine.UI.Image), typeof(UnityEngine.UI.Button));
            go.transform.SetParent(detailName.transform.parent, false);
            var rect = go.GetComponent<RectTransform>();
            rect.anchorMin = new Vector2(0, 1); rect.anchorMax = Vector2.one; rect.pivot = new Vector2(.5f, 1);
            rect.offsetMin = new Vector2(18, -350); rect.offsetMax = new Vector2(-18, -316);
            var image = go.GetComponent<UnityEngine.UI.Image>(); image.color = new Color32(23, 49, 57, 255);
            equipButton = go.GetComponent<UnityEngine.UI.Button>(); equipButton.targetGraphic = image;
            var label = CreateText("Label", go.transform, "EQUIP", 14, new Color32(236, 240, 232, 255), TextAlignmentOptions.Center);
            Stretch(label.rectTransform, 0);
            equipButton.onClick.AddListener(ToggleEquipment);
        }

        private void ResetEquipment()
        {
            EquipmentKnown = false;
            equipmentReadPending = equipmentSetPending = equipmentUncertain = false;
            equipmentReadAttempts = 0;
            equipmentNextReadAt = 0;
            equipmentState = default;
            equipmentResult = null;
            UpdateEquipmentMarkers();
            if (worldScreen != null) worldScreen.SetOwnWeaponVisible(false);
            if (bazaarScreen != null) bazaarScreen.RefreshEquipmentControls();
        }

        private void RefreshEquipment()
        {
            if (!WorldAvailable || equipmentReadPending || equipmentSetPending) return;
            equipmentReadAttempts = 0;
            RequestEquipmentRead();
        }

        private void RequestEquipmentRead()
        {
            if (!WorldAvailable || model == null || loginScreen.Model != model || equipmentReadPending || equipmentSetPending) return;
            equipmentReadAttempts++;
            equipmentReadPending = true;
            equipmentDeadline = Time.unscaledTime + RequestTimeoutSeconds;
            try { model.RequestEquipment(); }
            catch (Exception)
            {
                equipmentReadPending = false;
                equipmentResult = "Equipment could not be read. Refresh to try again.";
                equipmentNextReadAt = Time.unscaledTime + FailureRetryDelaySeconds;
                UpdateEquipmentMarkers();
            }
            UpdateControls();
        }

        private void ToggleEquipment()
        {
            if (!IsOpen || !WorldAvailable || model == null || loginScreen.Model != model || !hasSelectedItem || selectedItemID < 3 || selectedItemID > 5 ||
                !hasInventoryResponse || requestPending || !EquipmentKnown || equipmentReadPending || EquipmentMutationPending || !worldScreen.CanAct) return;
            byte slot = (byte)(selectedItemID - 3);
            uint target = IsItemEquipped(selectedItemID) ? 0U : selectedItemID;
            equipmentSetPending = true;
            equipmentDeadline = Time.unscaledTime + RequestTimeoutSeconds;
            equipmentResult = "Changing equipment…";
            worldScreen.StopActions();
            try
            {
                if (!model.RequestEquipItem(slot, target))
                {
                    equipmentSetPending = false;
                    equipmentResult = "Equipment change is unavailable.";
                }
            }
            catch (Exception)
            {
                equipmentSetPending = false;
                equipmentUncertain = true;
                EquipmentKnown = false;
                UpdateEquipmentMarkers();
                equipmentResult = "Equipment result unknown. Reconnect before changing it again.";
            }
            if (bazaarScreen != null) bazaarScreen.RefreshEquipmentControls();
            UpdateControls();
        }

        private void EquipmentReceived(byte operation, byte status, byte slot, EquipmentStateView equipment)
        {
            if (model == null || loginScreen.Model != model || !WorldAvailable ||
                (operation == 0 ? !equipmentReadPending : !equipmentSetPending && !equipmentUncertain)) return;
            if (operation == 0) equipmentReadPending = false;
            else { equipmentSetPending = false; equipmentReadPending = false; equipmentUncertain = false; }
            if (status == 1)
            {
                EquipmentKnown = true;
                equipmentState = equipment;
                equipmentResult = equipmentUncertain ? "Equipment change result unknown. Reconnect before changing it." : null;
                worldScreen.SetOwnWeaponVisible(equipment.WeaponID == 3);
                if (operation == 1) RefreshAfterDelivery();
            }
            else
            {
                EquipmentKnown = false;
                equipmentResult = status == 2 ? "Equipment is busy. Refresh in a moment."
                    : status == 3 ? "The item is no longer in your bag."
                    : status == 4 ? "This item cannot be equipped."
                    : status == 5 ? "Cannot change equipment while dead."
                    : status == 6 ? "World ownership changed. Refresh after arrival."
                    : status == 7 ? "Equipment request expired. Refresh before changing it."
                    : status == 8 ? "Equipment storage failed. Refresh before changing it."
                    : "Equipment request was rejected.";
                equipmentNextReadAt = Time.unscaledTime + FailureRetryDelaySeconds;
            }
            UpdateEquipmentMarkers();
            if (bazaarScreen != null) bazaarScreen.RefreshEquipmentControls();
            UpdateControls();
        }

        private void ResetItemUse()
        {
            usePending = false;
            useTimedOut = false;
            useCooldownDeadline = 0;
            UpdatePotionCooldown();
            SetUseStatus(string.Empty);
            ClearLootNotice();
        }

        private void ItemUseZoneTransitionStarted(ulong ownershipEpoch, ushort zoneID)
        {
            ResetEquipment();
            inventoryWarmupAttempted = false;
            inventoryWarmupPending = false;
            CancelFailureRetry();
            ResetItemUse();
            hasInventoryResponse = false;
            RequireFreshInventoryOnOpen();
            if (requestPending) refreshAfterCurrentRequest = true;
            UpdateControls();
        }

        private void ItemUseConnectionFailed(string message)
        {
            ResetEquipment();
            inventoryWarmupPending = false;
            CancelFailureRetry();
            ResetItemUse();
            hasInventoryResponse = false;
            RequireFreshInventoryOnOpen();
            UpdateControls();
        }

        private bool CanUseSelectedItem()
        {
            if (!IsOpen || !ItemUseAvailable || !hasSelectedItem || !ItemPresentation.IsPotion(selectedItemID, worldScreen.Content))
                return false;
            foreach (InventoryItemView item in loginScreen.Data.GetInventory())
                if (item.Slot == selectedSlot && item.ItemID == selectedItemID && item.Quantity > 0) return true;
            return false;
        }

        private void UseSelectedItem()
        {
            if (!CanUseSelectedItem()) return;
            UsePotion(selectedItemID);
        }

        private bool ItemUseAvailable => WorldAvailable && model != null && loginScreen != null &&
            loginScreen.Model == model && loginScreen.Data != null && hasInventoryResponse &&
            !requestPending && !requestTimedOut && !usePending && Time.unscaledTime >= useCooldownDeadline;

        private bool QuickPotionAvailable => WorldAvailable && model != null && loginScreen != null &&
            loginScreen.Model == model && !IsOpen && (bazaarScreen == null || !bazaarScreen.IsOpen) &&
            worldScreen.CanAct;

        private void UseQuickPotion() => UseQuickPotionItem(1);
        private void UseQuickMpPotion() => UseQuickPotionItem(2);

        private void UseQuickPotionItem(uint itemID)
        {
            if (!QuickPotionAvailable || !Application.isFocused || !GameUiInput.CanUseWorldShortcuts() ||
                requestPending || requestTimedOut || usePending || Time.unscaledTime < useCooldownDeadline) return;
            if (!hasInventoryResponse)
            {
                RequestInventory();
                return;
            }
            if (!ItemUseAvailable) return;
            foreach (InventoryItemView item in loginScreen.Data.GetInventory())
                if (item.ItemID == itemID && item.Quantity > 0)
                {
                    UsePotion(itemID);
                    return;
                }
        }

        private void UsePotion(uint itemID)
        {
            if (!ItemPresentation.IsPotion(itemID, worldScreen.Content)) return;
            usePending = true;
            useTimedOut = false;
            useDeadline = Time.unscaledTime + RequestTimeoutSeconds;
            SetUseStatus(worldScreen.Content.GetItem(itemID).hpRecovery > 0 ? "Using HP potion…" : "Using MP potion…");
            UpdateControls();
            try
            {
                if (model.RequestItemUse(itemID)) return;
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
            if (!usePending || model == null || loginScreen.Model != model || !ItemPresentation.IsPotion(itemID, worldScreen.Content)) return;
            string resource = worldScreen.Content.GetItem(itemID).hpRecovery > 0 ? "HP" : "MP";
            usePending = false;
            useTimedOut = false;
            useCooldownDeadline = Time.unscaledTime + cooldownRemainingTicks * .05f;
            string result;
            switch (status)
            {
                case 0: result = "Invalid item use request."; break;
                case 1: result = resource + " potion used."; break;
                case 2: result = "Cannot use a potion while dead."; break;
                case 3: result = resource + " is already full."; break;
                case 4: result = "Potion is on cooldown."; break;
                case 5: result = "No " + resource + " potions remain."; break;
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
                RefreshAfterDelivery();
            }
            UpdateControls();
        }

        private void ItemRewardReceived(uint itemID, ushort grantedQuantity, ushort remainingQuantity, byte status)
        {
            if (model == null || loginScreen.Model != model) return;
            switch (status)
            {
                case 1: SetUseStatus("Received " + ItemPresentation.Name(itemID, worldScreen.Content) + " ×" + grantedQuantity.ToString(CultureInfo.InvariantCulture) + "."); break;
                case 2: ShowLootFailure("Bag full. Item reward could not be added."); break;
                case 3: ShowLootFailure("Item reward failed."); break;
                default: SetUseStatus("Unknown item reward result."); break;
            }
            if (status == 1)
            {
                if (!IsOpen) RequireFreshInventoryOnOpen();
                RefreshAfterDelivery();
            }
            UpdateControls();
        }

        private void SetUseStatus(string message)
        {
            if (useStatus != null) useStatus.text = message;
            if (potionQuickStatus != null) potionQuickStatus.text = message;
            potionNoticeDeadline = string.IsNullOrEmpty(message) ? 0 : Time.unscaledTime + 6f;
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
            if (worldScreen != null && worldScreen.ItemTextFont != null)
            {
                detailName.font = worldScreen.ItemTextFont;
                if (useStatus != null) useStatus.font = worldScreen.ItemTextFont;
                if (lootNotice != null) lootNotice.font = worldScreen.ItemTextFont;
            }
            bool available = WorldAvailable && model != null && loginScreen.Model == model;
            if (!available) inventoryWarmupPending = false;
            if (!available || (!IsOpen && !inventoryWarmupPending)) CancelFailureRetry();
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
            else
            {
                inventoryWarmupPending = false;
                CancelFailureRetry();
            }
            inventoryPanel.SetActive(open);
            if (open) inventoryPanel.transform.SetAsLastSibling();
            if (open && (!hasInventoryResponse || refreshOnOpenAfterUncertainDelivery) && !requestTimedOut && !requestPending)
                RequestInventory();
            UpdateControls();
        }

        public void ClosePanel()
        {
            inventoryWarmupPending = false;
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
            hasInventoryResponse = false;
            if (requestPending) refreshAfterCurrentRequest = true;
            UpdateControls();
        }

        public void RefreshShortcutControls() => UpdateControls();

        private void RequestInventory()
        {
            if (inventoryWarmupAttempted) inventoryWarmupPending = false;
            CancelFailureRetry();
            failureRetryAttempted = false;
            SendInventoryRequest();
        }

        private void SendInventoryRequest()
        {
            if (!WorldAvailable || model == null || loginScreen.Model != model || requestPending || requestTimedOut)
                return;

            inventoryWarmupAttempted = true;
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
                inventoryWarmupPending = false;
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
            requestPending = false;
            if (!WorldAvailable)
            {
                // Drain the old uncorrelated read, then warm up once after the destination bootstrap.
                inventoryWarmupAttempted = false;
                inventoryWarmupPending = false;
                refreshAfterCurrentRequest = false;
                hasInventoryResponse = false;
                ClearInventoryCards();
                UpdateControls();
                return;
            }
            if (refreshAfterCurrentRequest)
            {
                inventoryWarmupPending = false;
                refreshAfterCurrentRequest = false;
                RequestInventory();
                return;
            }
            if (!success)
            {
                hasInventoryResponse = false;
                ClearInventoryCards();
                if ((IsOpen || inventoryWarmupPending) && WorldAvailable && !failureRetryAttempted)
                {
                    failureRetryAttempted = true;
                    failureRetryScheduled = true;
                    failureRetryDeadline = Time.unscaledTime + FailureRetryDelaySeconds;
                    inventoryStatus.text = "Inventory request failed. Checking once more…";
                }
                else
                {
                    inventoryWarmupPending = false;
                    inventoryStatus.text = "Inventory request failed. Select Refresh to try again.";
                }
                UpdateControls();
                return;
            }

            inventoryWarmupPending = false;
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
            inventoryStatus.text = snapshot.Count == 0 ? "No items." : snapshot.Count.ToString(CultureInfo.InvariantCulture) + " items";
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

            var name = CreateText("ItemName", cardObject.transform, ItemPresentation.Name(item.ItemID, worldScreen.Content), 14,
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

            if (item.ItemID >= 3 && item.ItemID <= 5)
            {
                var equipped = CreateText("EquippedItem" + item.ItemID.ToString(CultureInfo.InvariantCulture),
                    cardObject.transform, "EQUIPPED", 14, new Color32(229, 211, 163, 255), TextAlignmentOptions.MidlineLeft);
                SetTopLeft(equipped.rectTransform, 6, 66, 108, 17);
                equipped.gameObject.SetActive(IsItemEquipped(item.ItemID));
            }

            button.onClick.AddListener(() => SelectItem(item, button));
        }

        private void SelectItem(InventoryItemView item, Button card)
        {
            if (!IsOpen || !card.isActiveAndEnabled) return;
            hasSelectedItem = true;
            selectedSlot = item.Slot;
            selectedItemID = item.ItemID;
            ApplySelection(item, card);
            if (useStatus != null && !usePending && !useTimedOut)
                useStatus.text = ItemPresentation.IsPotion(item.ItemID, worldScreen.Content) && Time.unscaledTime < useCooldownDeadline
                    ? "Potion is on cooldown."
                    : ItemPresentation.UseDescription(item.ItemID, worldScreen.Content);
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
            detailName.text = ItemPresentation.Name(item.ItemID, worldScreen.Content);
            detailSlot.text = item.ItemID >= 3 && item.ItemID <= 5
                ? ItemPresentation.EquipmentDescription(item.ItemID, worldScreen.Content)
                : "Slot " + item.Slot.ToString(CultureInfo.InvariantCulture);
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
            UpdatePotionCooldown();
            bool available = WorldAvailable && model != null && loginScreen != null && loginScreen.Model == model;
            bool selectedPotion = hasSelectedItem && ItemPresentation.IsPotion(selectedItemID, worldScreen.Content);
            if (bagButton != null) bagButton.interactable = available;
            if (refreshButton != null) refreshButton.interactable = available && !requestPending && !requestTimedOut;
            if (closeButton != null) closeButton.interactable = true;
            if (useStatus != null && selectedPotion && useTimedOut)
                useStatus.text = "Use result unknown. Checking the bag; reconnect before trying again.";
            else if (useStatus != null && !usePending &&
                (potionNoticeDeadline == 0 || (hasSelectedItem && !selectedPotion)))
                useStatus.text = hasSelectedItem
                    ? (selectedPotion && Time.unscaledTime < useCooldownDeadline
                        ? "Potion is on cooldown." : ItemPresentation.UseDescription(selectedItemID, worldScreen.Content))
                    : string.Empty;
            if (equipButton != null)
            {
                equipButton.gameObject.SetActive(hasSelectedItem && selectedItemID >= 3 && selectedItemID <= 5);
                equipButton.interactable = available && IsOpen && hasInventoryResponse && !requestPending && EquipmentKnown &&
                    !equipmentReadPending && !EquipmentMutationPending && worldScreen.CanAct;
                var label = equipButton.GetComponentInChildren<TMP_Text>(true);
                if (label != null) label.text = equipmentSetPending ? "CHANGING…" : IsItemEquipped(selectedItemID) ? "UNEQUIP" : "EQUIP";
            }
            if (hasSelectedItem && selectedItemID >= 3 && selectedItemID <= 5 && useStatus != null)
                useStatus.text = equipmentResult ?? (!EquipmentKnown ? "Equipment: --" :
                    "Weapon: " + (equipmentState.WeaponID != 0 ? "On" : "Off") + " · Shield: " +
                    (equipmentState.ShieldID != 0 ? "On" : "Off") + "\nArmor: " + (equipmentState.BodyID != 0 ? "On" : "Off") +
                    "\nATK " + equipmentState.BaseAttack + "+" + equipmentState.BonusAttack + "=" + equipmentState.EffectiveAttack +
                    " · DEF " + equipmentState.DefensePercent + "%");
            if (useButton != null)
            {
                var label = useButton.GetComponentInChildren<TMP_Text>(true);
                if (label != null) label.text = selectedPotion && worldScreen.Content.GetItem(selectedItemID).mpRecovery > 0
                    ? "USE MP POTION" : "USE HP POTION";
                useButton.gameObject.SetActive(selectedPotion);
                useButton.interactable = CanUseSelectedItem();
            }
            if (potionQuickSlot != null || mpPotionQuickSlot != null)
            {
                bool known = hasInventoryResponse && !requestPending && !requestTimedOut;
                uint hpQuantity = 0, mpQuantity = 0;
                if (known && loginScreen.Data != null)
                    foreach (InventoryItemView item in loginScreen.Data.GetInventory())
                        if (item.ItemID == 1) hpQuantity += item.Quantity;
                        else if (item.ItemID == 2) mpQuantity += item.Quantity;
                UpdatePotionQuickSlot(potionQuickSlot, potionQuantity, hpQuantity, known);
                UpdatePotionQuickSlot(mpPotionQuickSlot, mpPotionQuantity, mpQuantity, known);
                if (potionQuickStatus != null)
                {
                    if (requestTimedOut) potionQuickStatus.text = "Bag result unknown. Reconnect before checking potions.";
                }
            }
        }

        private void EnsurePotionQuickSlots()
        {
            if (potionQuickSlot == null) return;
            if (mpPotionQuickSlot == null)
            {
                var slot = Instantiate(potionQuickSlot.gameObject, potionQuickSlot.transform.parent, false);
                slot.name = "MpPotionQuickSlot";
                mpPotionQuickSlot = slot.GetComponent<UnityEngine.UI.Button>();
                mpPotionQuickSlot.onClick = new UnityEngine.UI.Button.ButtonClickedEvent();
                mpPotionQuantity = slot.transform.Find("Quantity")?.GetComponent<TMP_Text>();
                var icon = slot.transform.Find("Icon")?.GetComponent<UnityEngine.UI.Image>();
                if (icon != null) icon.sprite = ItemPresentation.Icon(2);
                var key = slot.transform.Find("Key")?.GetComponent<TMP_Text>();
                if (key != null) key.text = "2";
            }
            potionCooldown = ConfigurePotionQuickSlot(potionQuickSlot, 126);
            mpPotionCooldown = ConfigurePotionQuickSlot(mpPotionQuickSlot, 170);
        }

        private TMP_Text ConfigurePotionQuickSlot(UnityEngine.UI.Button button, float x)
        {
            var rect = (RectTransform)button.transform;
            rect.anchorMin = rect.anchorMax = rect.pivot = new Vector2(.5f, 0);
            rect.anchoredPosition = new Vector2(x, 8);
            rect.sizeDelta = new Vector2(40, 50);
            var icon = button.transform.Find("Icon") as RectTransform;
            if (icon != null) icon.sizeDelta = new Vector2(38, 38);
            var key = button.transform.Find("Key")?.GetComponent<TMP_Text>();
            if (key != null) key.fontSize = 16;
            var quantity = button.transform.Find("Quantity")?.GetComponent<TMP_Text>();
            if (quantity != null)
            {
                quantity.fontSize = 15;
                quantity.enableAutoSizing = true;
                quantity.fontSizeMin = 11;
                quantity.fontSizeMax = 15;
            }
            var cooldown = button.transform.Find("Cooldown")?.GetComponent<TMP_Text>();
            if (cooldown == null)
            {
                cooldown = CreateText("Cooldown", button.transform, string.Empty, 14,
                    new Color32(255, 242, 209, 255), TextAlignmentOptions.Center);
                cooldown.fontStyle = FontStyles.Bold;
                cooldown.rectTransform.anchorMin = cooldown.rectTransform.anchorMax = cooldown.rectTransform.pivot = new Vector2(.5f, .5f);
                cooldown.rectTransform.sizeDelta = new Vector2(36, 20);
                var outline = cooldown.gameObject.AddComponent<UnityEngine.UI.Outline>();
                outline.effectColor = new Color32(16, 37, 42, 255);
                outline.effectDistance = new Vector2(1, -1);
            }
            cooldown.gameObject.SetActive(false);
            return cooldown;
        }

        private void UpdatePotionCooldown()
        {
            int seconds = Mathf.Max(0, Mathf.CeilToInt(useCooldownDeadline - Time.unscaledTime));
            if (potionCooldown != null)
            {
                potionCooldown.gameObject.SetActive(seconds > 0);
                if (seconds > 0) potionCooldown.SetText("{0}s", seconds);
            }
            if (mpPotionCooldown != null)
            {
                mpPotionCooldown.gameObject.SetActive(seconds > 0);
                if (seconds > 0) mpPotionCooldown.SetText("{0}s", seconds);
            }
        }

        private void UpdateEquipmentMarkers()
        {
            if (inventoryGridContent == null) return;
            foreach (Transform card in inventoryGridContent)
                for (uint itemID = 3; itemID <= 5; itemID++)
                {
                    var marker = card.Find("EquippedItem" + itemID.ToString(CultureInfo.InvariantCulture));
                    if (marker != null) marker.gameObject.SetActive(IsItemEquipped(itemID));
                }
        }

        private void UpdatePotionQuickSlot(UnityEngine.UI.Button button, TMP_Text quantityText, uint quantity, bool known)
        {
            if (button == null) return;
            button.gameObject.SetActive(WorldAvailable && model != null && loginScreen != null && loginScreen.Model == model);
            if (quantityText != null) quantityText.text = known ? quantity.ToString(CultureInfo.InvariantCulture) : "?";
            button.interactable = QuickPotionAvailable && !requestPending && !requestTimedOut &&
                !usePending && Time.unscaledTime >= useCooldownDeadline && (!known || quantity > 0);
        }

        private TMP_Text CreateText(string name, Transform parent, string value, float size, Color color,
            TextAlignmentOptions alignment)
        {
            var gameObject = new GameObject(name, typeof(RectTransform), typeof(CanvasRenderer), typeof(TextMeshProUGUI));
            gameObject.transform.SetParent(parent, false);
            var text = gameObject.GetComponent<TextMeshProUGUI>();
            text.font = worldScreen != null && worldScreen.ItemTextFont != null ? worldScreen.ItemTextFont : TMP_Settings.defaultFontAsset;
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
            inventoryWarmupPending = false;
            CancelFailureRetry();
            if (loginScreen != null) loginScreen.ClientChanged -= ChangeClient;
            if (worldScreen != null) worldScreen.WorldStateChanged -= WorldChanged;
            if (bagButton != null) bagButton.onClick.RemoveListener(TogglePanel);
            if (refreshButton != null) refreshButton.onClick.RemoveListener(RequestInventory);
            if (refreshButton != null) refreshButton.onClick.RemoveListener(RefreshEquipment);
            if (closeButton != null) closeButton.onClick.RemoveListener(ClosePanel);
            if (useButton != null) useButton.onClick.RemoveListener(UseSelectedItem);
            if (potionQuickSlot != null) potionQuickSlot.onClick.RemoveListener(UseQuickPotion);
            if (mpPotionQuickSlot != null) mpPotionQuickSlot.onClick.RemoveListener(UseQuickMpPotion);
            SetModelSubscriptions(false);
            ResetEquipment();
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
