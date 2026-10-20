using System;
using System.Collections.Generic;
using System.Globalization;
using C01;
using ClientCore;
using ClientCore.PacketHelper;
using TMPro;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.InputSystem;

namespace C02
{
    public enum C02Stage { Ready, Choosing, Entering, WaitingForZone, WaitingForFull, TransitionPending, ReconnectRequired, InWorld, Failed }

    public sealed class C02WorldScreen : MonoBehaviour
    {
        [SerializeField] private C01LoginScreen loginScreen;
        [SerializeField] private UnityEngine.UI.Button[] characterChoices;
        [SerializeField] private UnityEngine.UI.Button enterButton;
        [SerializeField] private UnityEngine.UI.Button disconnectButton;
        [SerializeField] private TMP_Text entryStatusText;
        [SerializeField] private TMP_Text worldStatusText;
        [SerializeField] private UnityEngine.UI.Button zoneTransitionButton;
        [SerializeField] private GameObject selectionPanel;
        [SerializeField] private GameObject background;
        [SerializeField] private GameObject worldRoot;
        [SerializeField] private GameObject worldHud;
        [SerializeField] private Transform ownCharacter;
        [SerializeField] private Transform fieldPlane;
        [SerializeField] private Transform quarterViewCamera;
        private IViewModelUI model;
        private readonly Dictionary<uint, FullStateField> pendingFull = new Dictionary<uint, FullStateField>();
        private FullStateField ownFull;
        private Vector2? moveTarget;
        private double nextMoveTime;
        private double progressDeadline;
        private double targetDeadline;
        private double transitionDeadline;
        private ushort transitionTargetZone;
        private readonly List<RaycastResult> uiHits = new List<RaycastResult>();
        public C02Stage Stage { get; private set; }
        public ulong SelectedCharacterID { get; private set; }
        public ushort ZoneID { get; private set; }
        public uint InternalID { get; private set; }
        public bool HasOwnFull { get; private set; }
        public FullStateField OwnFull => ownFull;
        public Transform ViewCamera => quarterViewCamera;
        public bool CanAct => isActiveAndEnabled && Stage == C02Stage.InWorld && HasOwnFull && ownFull.hp > 0;
        public bool IsWorldVisible => worldRoot.activeSelf;
        public Vector2 ServerPosition => new Vector2(ownFull.x, ownFull.y);
        public Vector2 ZoneStartPosition { get; private set; }
        public int EnterRequestCount { get; private set; }
        public event Action WorldStateChanged;
        private bool EntryPending => Stage == C02Stage.Entering || Stage == C02Stage.WaitingForZone || Stage == C02Stage.WaitingForFull;
        private const double ZoneTransitionTimeoutSeconds = 12;

        private void OnEnable()
        {
            for (int i = 0; i < characterChoices.Length; i++)
            {
                int row = i;
                characterChoices[i].onClick.AddListener(() => SelectCharacter(row));
            }
            enterButton.onClick.AddListener(EnterSelected);
            disconnectButton.onClick.AddListener(Disconnect);
            if (zoneTransitionButton != null)
            {
                zoneTransitionButton.onClick.AddListener(RequestNearbyZoneChange);
                zoneTransitionButton.gameObject.SetActive(false);
            }
            loginScreen.ClientChanged += ChangeClient;
            loginScreen.CharacterListReceived += ListReceived;
            ChangeClient(loginScreen.Model);
        }

        private void ChangeClient(IViewModelUI next)
        {
            DetachModel();
            model = next;
            ResetWorld();
            SelectedCharacterID = 0;
            Stage = C02Stage.Ready;
            if (entryStatusText != null) entryStatusText.text = "Sign in, then select a character returned by the server.";
            foreach (var choice in characterChoices)
                if (choice != null) choice.gameObject.SetActive(false);
            if (enterButton != null) enterButton.interactable = false;
            if (model == null) return;
            model.OnEnterSuccess += EnterReceived;
            model.OnZoneChageReceived += ZoneReceived;
            model.OnZoneChageFailed += ZoneFailed;
            model.OnFullReceived += FullReceived;
            model.OnDeltaReceived += DeltaReceived;
        }

        private void DetachModel()
        {
            if (model != null)
            {
                model.OnEnterSuccess -= EnterReceived;
                model.OnZoneChageReceived -= ZoneReceived;
                model.OnZoneChageFailed -= ZoneFailed;
                model.OnFullReceived -= FullReceived;
                model.OnDeltaReceived -= DeltaReceived;
            }
            model = null;
        }

        private void ListReceived(byte status, ushort count)
        {
            var rows = loginScreen.Characters.Item2;
            Stage = C02Stage.Choosing;
            SelectedCharacterID = 0;
            enterButton.interactable = false;
            for (int i = 0; i < characterChoices.Length; i++)
            {
                bool available = status != 0 && i < count && i < rows.Count;
                characterChoices[i].gameObject.SetActive(available);
                characterChoices[i].interactable = available;
                if (available) characterChoices[i].GetComponentInChildren<TMP_Text>().text =
                    $"{rows[i].Name.TrimEnd('\0')} / Lv {rows[i].Level} / ID {rows[i].CharacterID}";
            }
            entryStatusText.text = status == 0 || count == 0
                ? "No selectable characters. World entry is unavailable."
                : "Select a character, then enter the field.";
        }

        private void SelectCharacter(int row)
        {
            if (Stage != C02Stage.Choosing || model == null || loginScreen.IsBusy) return;
            var rows = loginScreen.Characters;
            if (row < 0 || row >= rows.Item1 || row >= rows.Item2.Count) return;
            SelectedCharacterID = rows.Item2[row].CharacterID;
            enterButton.interactable = SelectedCharacterID != 0;
            entryStatusText.text = $"Selected character ID {SelectedCharacterID}. Ready to enter.";
        }

        private void EnterSelected()
        {
            if (Stage != C02Stage.Choosing || model == null || loginScreen.IsBusy || SelectedCharacterID == 0) return;
            // Recheck the live server-list snapshot; no numeric/free-form ID entry exists.
            if (!loginScreen.Characters.Item2.Exists(c => c.CharacterID == SelectedCharacterID)) return;
            ResetWorld();
            Stage = C02Stage.Entering;
            enterButton.interactable = false;
            foreach (var choice in characterChoices) choice.interactable = false;
            entryStatusText.text = "Requesting world entry...";
            loginScreen.SetBusy(true);
            EnterRequestCount++;
            model.Enter(SelectedCharacterID);
        }

        private void EnterReceived(bool success)
        {
            if (!EntryPending) return;
            if (!success) { Fail("World entry was rejected."); return; }
            if (Stage == C02Stage.Entering)
            {
                Stage = C02Stage.WaitingForZone;
                entryStatusText.text = "World entry accepted. Waiting for the server Zone result...";
            }
        }

        private void ZoneReceived(ushort zone, ulong chat, uint internalId, float x, float y)
        {
            if (Stage == C02Stage.TransitionPending)
            {
                if (internalId == 0 || zone != transitionTargetZone || !InsideZone(zone, new Vector2(x, y)))
                { RequireReconnect("The server returned an invalid Zone result. Disconnect and reconnect."); return; }
                ZoneID = zone;
                InternalID = internalId;
                ZoneStartPosition = new Vector2(x, y);
                ownFull = default;
                HasOwnFull = false;
                pendingFull.Clear();
                ownCharacter.gameObject.SetActive(false);
                worldStatusText.text = string.Format(CultureInfo.InvariantCulture,
                    "Zone {0} entered at ({1:F3}, {2:F3}). Waiting for your matching server state…\nPlease wait before taking another action.",
                    zone, x, y);
                UpdateZoneTransitionButton();
                WorldStateChanged?.Invoke();
                return;
            }
            if (!EntryPending) return;
            CancelMovement();
            if (internalId == 0 || !InsideZone(zone, new Vector2(x, y))) { Fail("Invalid server Zone result."); return; }
            ZoneID = zone;
            InternalID = internalId;
            ZoneStartPosition = new Vector2(x, y);
            Stage = C02Stage.WaitingForFull;
            entryStatusText.text = "Zone confirmed. Waiting for your matching server Full state...";
            WorldStateChanged?.Invoke();
            if (pendingFull.TryGetValue(InternalID, out var state)) ApplyOwnFull(state);
            pendingFull.Clear();
        }

        private void ZoneFailed()
        {
            if (Stage == C02Stage.TransitionPending)
            {
                if (ZoneID == transitionTargetZone)
                { RequireReconnect("Zone state became uncertain. Disconnect and reconnect."); return; }
                transitionDeadline = 0;
                transitionTargetZone = 0;
                Stage = C02Stage.InWorld;
                StopActions();
                ShowPosition();
                WorldStateChanged?.Invoke();
                return;
            }
            CancelMovement();
            if (EntryPending) Fail("Zone entry was rejected.");
        }

        private void FullReceived(ushort count, FullStateField[] states)
        {
            if (states == null || count > states.Length) return;
            if (!EntryPending && Stage != C02Stage.InWorld && Stage != C02Stage.TransitionPending) return;
            if (Stage == C02Stage.TransitionPending && ZoneID != transitionTargetZone) return;
            for (int i = 0; i < count; i++)
            {
                var state = states[i];
                if (InternalID == 0) pendingFull[state.zoneInternalID] = state;
                else if (state.zoneInternalID == InternalID) ApplyOwnFull(state);
            }
            // Full frames cover AOI cells; absence never means deletion.
        }

        private void ApplyOwnFull(FullStateField state)
        {
            if (state.dir > 3 || state.hp < 0 || state.mp < 0 || state.maxHp < 0 || state.maxMp < 0 ||
                state.hp > state.maxHp || state.mp > state.maxMp || !InsideZone(ZoneID, new Vector2(state.x, state.y)))
            { Fail("Invalid server character state."); return; }
            if (HasOwnFull && (state.x != ownFull.x || state.y != ownFull.y))
                progressDeadline = Time.unscaledTimeAsDouble + 3;
            ownFull = state;
            if (ownFull.hp == 0) StopActions();
            HasOwnFull = true;
            Stage = C02Stage.InWorld;
            transitionDeadline = 0;
            transitionTargetZone = 0;
            ownCharacter.gameObject.SetActive(true);
            worldRoot.SetActive(true);
            worldHud.SetActive(true);
            selectionPanel.SetActive(false);
            background.SetActive(false);
            loginScreen.SetBusy(false);
            ShowPosition();
            UpdateZoneTransitionButton();
            WorldStateChanged?.Invoke();
        }

        private void DeltaReceived(ushort count, DeltaUpdateField[] updates)
        {
            if (updates == null || count > updates.Length) return;
            if (!EntryPending && Stage != C02Stage.InWorld) return;
            bool changed = false;
            var next = ownFull;
            for (int i = 0; i < count; i++)
            {
                var update = updates[i];
                if (update.fieldID != 0 && update.fieldID != 1 && update.fieldID != 4 && update.fieldID != 5 &&
                    update.fieldID != 6 && update.fieldID != 7 && update.fieldID != 8) continue;
                if (HasOwnFull && update.zoneInternalID == InternalID)
                {
                    if (update.fieldID == 0 || update.fieldID == 1)
                    {
                        if (update.fieldVal > (uint)(update.fieldID == 0 ? next.maxHp : next.maxMp))
                        { Fail("Invalid server character vitals."); return; }
                        if (update.fieldID == 0) next.hp = (int)update.fieldVal; else next.mp = (int)update.fieldVal;
                    }
                    else if (update.fieldID == 4) next.exp = update.fieldVal;
                    else if (update.fieldID == 5)
                    {
                        if (update.fieldVal > ushort.MaxValue) { Fail("Invalid server character level."); return; }
                        next.level = (ushort)update.fieldVal;
                    }
                    else if (update.fieldID == 6)
                    {
                        if (update.fieldVal > 3) { Fail("Invalid server character direction."); return; }
                        next.dir = (byte)update.fieldVal;
                    }
                    else
                    {
                        float value = BitConverter.ToSingle(BitConverter.GetBytes(update.fieldVal), 0);
                        if (!Finite(value)) { Fail("Invalid server character position."); return; }
                        if (update.fieldID == 7) next.x = value; else next.y = value;
                    }
                    changed = true;
                }
                else if (InternalID == 0 && pendingFull.TryGetValue(update.zoneInternalID, out var state))
                {
                    if (update.fieldID == 0) state.hp = unchecked((int)update.fieldVal);
                    else if (update.fieldID == 1) state.mp = unchecked((int)update.fieldVal);
                    else if (update.fieldID == 4) state.exp = update.fieldVal;
                    else if (update.fieldID == 5)
                    {
                        if (update.fieldVal > ushort.MaxValue)
                        {
                            pendingFull.Remove(update.zoneInternalID);
                            continue;
                        }
                        state.level = (ushort)update.fieldVal;
                    }
                    else if (update.fieldID == 6) state.dir = update.fieldVal <= 3 ? (byte)update.fieldVal : byte.MaxValue;
                    else
                    {
                        float value = BitConverter.ToSingle(BitConverter.GetBytes(update.fieldVal), 0);
                        if (update.fieldID == 7) state.x = value; else state.y = value;
                    }
                    pendingFull[update.zoneInternalID] = state;
                }
            }
            if (changed)
            {
                if (!InsideZone(ZoneID, new Vector2(next.x, next.y)))
                { Fail("Invalid server character position."); return; }
                if (next.x != ownFull.x || next.y != ownFull.y)
                    progressDeadline = Time.unscaledTimeAsDouble + 3;
                ownFull = next;
                if (ownFull.hp == 0) StopActions();
                ShowPosition();
                WorldStateChanged?.Invoke();
            }
        }

        private void Update()
        {
            if (Stage == C02Stage.TransitionPending)
            {
                if (Time.unscaledTimeAsDouble >= transitionDeadline)
                    RequireReconnect("Zone transition timed out. Disconnect and reconnect before continuing.");
                return;
            }
            if (Stage == C02Stage.ReconnectRequired) return;
            if (!CanAct || model == null || loginScreen.Data == null || !Application.isFocused)
            { CancelMovement(); return; }
            var events = EventSystem.current;
            var selectedInput = events != null && events.currentSelectedGameObject != null
                ? events.currentSelectedGameObject.GetComponentInParent<TMP_InputField>() : null;
            if (events == null || selectedInput != null && selectedInput.isFocused)
            { CancelMovement(); return; }

            double now = Time.unscaledTimeAsDouble;
            if (Mouse.current != null && Mouse.current.leftButton.wasPressedThisFrame)
            {
                CancelMovement();
                var pointer = new PointerEventData(events) { position = Mouse.current.position.ReadValue() };
                uiHits.Clear();
                events.RaycastAll(pointer, uiHits);
                foreach (var hit in uiHits)
                    if (hit.module is UnityEngine.UI.GraphicRaycaster) return;
                // The visible HUD uses non-raycast decorative graphics; its panel still blocks movement clicks.
                var hudRect = worldHud.transform as RectTransform;
                var canvas = worldHud.GetComponentInParent<Canvas>();
                if (worldHud.activeInHierarchy && hudRect != null && RectTransformUtility.RectangleContainsScreenPoint(
                    hudRect, pointer.position, canvas != null && canvas.renderMode != RenderMode.ScreenSpaceOverlay ? canvas.worldCamera : null)) return;
                var camera = quarterViewCamera.GetComponent<Camera>();
                var surface = fieldPlane.GetComponent<Collider>();
                // Only the existing base plane is a movement surface; decoration has no navigation rules.
                if (camera == null || surface == null || !surface.Raycast(camera.ScreenPointToRay(pointer.position), out var ground, 1000)) return;
                var target = new Vector2(ground.point.x, ground.point.z);
                if (!InsideZone(ZoneID, target)) return;
                moveTarget = target;
                progressDeadline = now + 3;
                targetDeadline = now + 15;
            }
            if (!moveTarget.HasValue) return;
            if (now >= progressDeadline || now >= targetDeadline ||
                (moveTarget.Value - ServerPosition).sqrMagnitude <= 0.05f * 0.05f)
            { CancelMovement(); return; }
            if (now < nextMoveTime) return;
            if (!TryMoveCandidate(ZoneID, ServerPosition, moveTarget.Value, out var candidate, out var direction))
            { CancelMovement(); return; }
            // One candidate from the latest confirmed state. A long frame never catches up requests.
            nextMoveTime = now + 0.05;
            loginScreen.Data.UpdateMove(direction, candidate.x, candidate.y);
        }

        private static bool InsideZone(ushort zone, Vector2 position)
        {
            if (zone < 1 || zone > 4 || !Finite(position.x) || !Finite(position.y)) return false;
            int column = (zone - 1) % 3, row = (zone - 1) / 3;
            return position.x >= column * 100 - 5 && position.x <= (column + 1) * 100 + 5 &&
                position.y >= row * 100 - 5 && position.y <= (row + 1) * 100 + 5;
        }

        private static bool TryMoveCandidate(ushort zone, Vector2 origin, Vector2 target, out Vector2 candidate, out byte direction)
        {
            candidate = origin;
            direction = 0;
            if (!InsideZone(zone, origin) || !InsideZone(zone, target)) return false;
            Vector2 delta = target - origin;
            float distance = delta.magnitude;
            if (distance <= 0.05f) return false;
            candidate = origin + delta * (Mathf.Min(1, distance) / distance);
            // Movement convention: 0=+Y, 1=-Y, 2=-X, 3=+X. Ties choose X.
            direction = Mathf.Abs(delta.y) > Mathf.Abs(delta.x)
                ? (byte)(delta.y >= 0 ? 0 : 1) : (byte)(delta.x < 0 ? 2 : 3);
            return true;
        }

        public void CancelMovement()
        {
            moveTarget = null;
            loginScreen?.Data?.CancelMove();
        }

        public void StopActions()
        {
            loginScreen?.Data?.CancelAction();
            CancelMovement();
        }

        private void OnApplicationFocus(bool focused) { if (!focused) CancelMovement(); }

        private void RequestNearbyZoneChange()
        {
            if (!CanAct || model == null || loginScreen.Data == null ||
                !TryNearbyZoneChange(ZoneID, ServerPosition, out var operation, out var targetZone)) return;

            StopActions();
            transitionTargetZone = targetZone;
            transitionDeadline = Time.unscaledTimeAsDouble + ZoneTransitionTimeoutSeconds;
            Stage = C02Stage.TransitionPending;
            worldStatusText.text = string.Format(CultureInfo.InvariantCulture,
                "Requesting entry to Zone {0}…\nMovement and skills are paused until the server confirms the transition.", targetZone);
            UpdateZoneTransitionButton();
            WorldStateChanged?.Invoke();
            try { model.ZoneChange(operation); }
            catch (Exception) { ZoneFailed(); }
        }

        private void RequireReconnect(string message)
        {
            StopActions();
            transitionDeadline = 0;
            Stage = C02Stage.ReconnectRequired;
            if (worldStatusText != null) worldStatusText.text = message;
            UpdateZoneTransitionButton();
            WorldStateChanged?.Invoke();
        }

        private void UpdateZoneTransitionButton()
        {
            if (zoneTransitionButton == null) return;
            ushort targetZone = 0;
            bool available = CanAct && TryNearbyZoneChange(ZoneID, ServerPosition, out _, out targetZone);
            zoneTransitionButton.gameObject.SetActive(available);
            zoneTransitionButton.interactable = available;
            if (available && zoneTransitionButton.GetComponentInChildren<TMP_Text>(true) is TMP_Text label)
                label.text = targetZone == 2 ? "ENTER ZONE 2  →" : "←  RETURN TO ZONE 1";
        }

        private static bool TryNearbyZoneChange(ushort zone, Vector2 position, out byte operation, out ushort targetZone)
        {
            operation = 0;
            targetZone = 0;
            if (!Finite(position.x) || !Finite(position.y) || position.x < 95 || position.x > 105 ||
                position.y < -5 || position.y > 105) return false;
            if (zone == 1) { operation = 4; targetZone = 2; return true; }
            if (zone == 2) { operation = 3; targetZone = 1; return true; }
            return false;
        }

        // The authored hero faces local -Z (Vector3.back).
        internal static Quaternion ServerFacing(byte direction) => Quaternion.Euler(0,
            direction == 0 ? 180 : direction == 1 ? 0 : direction == 2 ? 90 : 270, 0);

        private void ShowPosition()
        {
            if (moveTarget.HasValue && (moveTarget.Value - ServerPosition).sqrMagnitude <= 0.05f * 0.05f)
                CancelMovement();
            var position = new Vector3(ownFull.x, 0, ownFull.y);
            ownCharacter.position = position;
            ownCharacter.rotation = ServerFacing(ownFull.dir);
            fieldPlane.position = position;
            quarterViewCamera.position = position + new Vector3(12, 16, -12);
            quarterViewCamera.LookAt(position + Vector3.up);
            worldStatusText.text = string.Format(CultureInfo.InvariantCulture,
                "C02 / SERVER-CONFIRMED FIELD\nCharacter ID {0}    Internal ID {1}\nZone {2}    Server (x, y): ({3:F3}, {4:F3})\nClick the field to walk. Position is confirmed by the server.",
                SelectedCharacterID, InternalID, ZoneID, ownFull.x, ownFull.y);
            UpdateZoneTransitionButton();
        }

        private void ResetWorld()
        {
            CancelMovement();
            nextMoveTime = 0;
            pendingFull.Clear();
            ownFull = default;
            HasOwnFull = false;
            InternalID = 0;
            ZoneID = 0;
            ZoneStartPosition = Vector2.zero;
            transitionDeadline = 0;
            transitionTargetZone = 0;
            if (zoneTransitionButton != null) zoneTransitionButton.gameObject.SetActive(false);
            if (worldRoot != null) worldRoot.SetActive(false);
            if (worldHud != null) worldHud.SetActive(false);
            if (selectionPanel != null) selectionPanel.SetActive(true);
            if (background != null) background.SetActive(true);
            if (worldStatusText != null) worldStatusText.text = string.Empty;
            WorldStateChanged?.Invoke();
        }

        private void Fail(string message)
        {
            CancelMovement();
            loginScreen.ConnectionFailed(message);
            Stage = C02Stage.Failed;
            entryStatusText.text = message + " Sign in to retry.";
        }

        private void Disconnect() => loginScreen.ConnectionFailed("Session closed.");
        private static bool Finite(float value) => !float.IsNaN(value) && !float.IsInfinity(value);

        private void OnDisable()
        {
            CancelMovement();
            if (loginScreen != null)
            {
                loginScreen.ClientChanged -= ChangeClient;
                loginScreen.CharacterListReceived -= ListReceived;
            }
            if (enterButton != null) enterButton.onClick.RemoveListener(EnterSelected);
            if (disconnectButton != null) disconnectButton.onClick.RemoveListener(Disconnect);
            if (zoneTransitionButton != null) zoneTransitionButton.onClick.RemoveListener(RequestNearbyZoneChange);
            if (zoneTransitionButton != null) zoneTransitionButton.gameObject.SetActive(false);
            foreach (var choice in characterChoices)
                if (choice != null) choice.onClick.RemoveAllListeners();
            if (worldRoot != null) worldRoot.SetActive(false);
            if (worldHud != null) worldHud.SetActive(false);
            DetachModel();
            pendingFull.Clear();
            ownFull = default;
            HasOwnFull = false;
            InternalID = 0;
            ZoneID = 0;
            ZoneStartPosition = Vector2.zero;
            transitionDeadline = 0;
            transitionTargetZone = 0;
            SelectedCharacterID = 0;
            Stage = C02Stage.Ready;
            WorldStateChanged?.Invoke();
            if (loginScreen != null) loginScreen.ReleaseClient();
        }
    }
}
