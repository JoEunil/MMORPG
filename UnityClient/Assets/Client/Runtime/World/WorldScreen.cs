using System;
using System.Collections.Generic;
using System.Globalization;
using Mmorpg.Client;
using ClientCore;
using ClientCore.PacketHelper;
using TMPro;
using UnityEngine;
using UnityEngine.InputSystem;

namespace Mmorpg.Client
{
    public enum WorldStage { Ready, Choosing, Entering, WaitingForZone, WaitingForFull, TransitionPending, ReconnectRequired, InWorld, Failed }

    [UnityEngine.Scripting.APIUpdating.MovedFrom(true, sourceNamespace: "C02", sourceAssembly: "C02.Runtime", sourceClassName: "C02WorldScreen")]

    public sealed class WorldScreen : MonoBehaviour
    {
        [SerializeField] private LoginScreen loginScreen;
        [SerializeField] private InventoryScreen inventoryScreen;
        [SerializeField] private MarketScreen bazaarScreen;
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
        [SerializeField] private WorldMap worldMap;
        private IViewModelUI model;
        private readonly Dictionary<uint, FullStateField> pendingFull = new Dictionary<uint, FullStateField>();
        private FullStateField ownFull;
        private const float MovementUnitsPerSecond = 9f;
        // Request horizon covers the send/server/snapshot/receive cycle; presentation stays tighter.
        private const float MaximumRequestLead = 1.8f;
        private const float MaximumVisualLead = 0.9f;
        private const float StaleMovementSeconds = 0.35f;
        private Vector2 inputTarget;
        private bool hasMovementIntent;
        private Vector2 movementDirection;
        private float secondsWithoutProgress;
        private const double MovementIntervalSeconds = 0.05;
        private Vector2 visualPosition;
        private bool hasVisualPosition;
        private float secondsWithoutInput;
        private Vector2 visualCorrectionVelocity;
        private double transitionDeadline;
        private ushort transitionTargetZone;
        public WorldStage Stage { get; private set; }
        public ulong SelectedCharacterID { get; private set; }
        public ushort ZoneID { get; private set; }
        public uint InternalID { get; private set; }
        public bool HasOwnFull { get; private set; }
        public FullStateField OwnFull => ownFull;
        public Transform ViewCamera => quarterViewCamera;
        public ActorMotion ActorMotion { get; private set; }
        public bool CanAct => isActiveAndEnabled && Stage == WorldStage.InWorld && HasOwnFull && ownFull.hp > 0;
        public bool IsWorldVisible => worldRoot.activeSelf;
        public Vector2 ServerPosition => new Vector2(ownFull.x, ownFull.y);
        public Vector2 ZoneStartPosition { get; private set; }
        public int EnterRequestCount { get; private set; }
        public event Action WorldStateChanged;
        private bool EntryPending => Stage == WorldStage.Entering || Stage == WorldStage.WaitingForZone || Stage == WorldStage.WaitingForFull;
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
            Stage = WorldStage.Ready;
            if (entryStatusText != null) entryStatusText.text = "Sign in, then select a character returned by the server.";
            foreach (var choice in characterChoices)
                if (choice != null) choice.gameObject.SetActive(false);
            if (enterButton != null) enterButton.interactable = false;
            if (model == null) return;
            model.OnEnterSuccess += EnterReceived;
            model.OnZoneChageReceived += ZoneReceived;
            model.OnZoneTransitionStarted += ZoneTransitionStarted;
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
                model.OnZoneTransitionStarted -= ZoneTransitionStarted;
                model.OnZoneChageFailed -= ZoneFailed;
                model.OnFullReceived -= FullReceived;
                model.OnDeltaReceived -= DeltaReceived;
            }
            model = null;
        }

        private void ListReceived(byte status, ushort count)
        {
            var rows = loginScreen.Characters.Item2;
            Stage = WorldStage.Choosing;
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
            if (Stage != WorldStage.Choosing || model == null || loginScreen.IsBusy) return;
            var rows = loginScreen.Characters;
            if (row < 0 || row >= rows.Item1 || row >= rows.Item2.Count) return;
            SelectedCharacterID = rows.Item2[row].CharacterID;
            enterButton.interactable = SelectedCharacterID != 0;
            entryStatusText.text = $"Selected character ID {SelectedCharacterID}. Ready to enter.";
        }

        private void EnterSelected()
        {
            if (Stage != WorldStage.Choosing || model == null || loginScreen.IsBusy || SelectedCharacterID == 0) return;
            // Recheck the live server-list snapshot; no numeric/free-form ID entry exists.
            if (!loginScreen.Characters.Item2.Exists(c => c.CharacterID == SelectedCharacterID)) return;
            ResetWorld();
            Stage = WorldStage.Entering;
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
            if (Stage == WorldStage.Entering)
            {
                Stage = WorldStage.WaitingForZone;
                entryStatusText.text = "World entry accepted. Waiting for the server Zone result...";
            }
        }

        private void ZoneReceived(ushort zone, ulong chat, uint internalId, float x, float y)
        {
            if (Stage == WorldStage.TransitionPending)
            {
                if (internalId == 0 || zone != transitionTargetZone || !InsideZone(zone, new Vector2(x, y)))
                { RequireReconnect("The server returned an invalid Zone result. Disconnect and reconnect."); return; }
                CancelMovement();
                if (ActorMotion != null) ActorMotion.ResetPose();
                ZoneID = zone;
                InternalID = internalId;
                ZoneStartPosition = new Vector2(x, y);
                ownFull = default;
                HasOwnFull = false;
                hasVisualPosition = false;
                pendingFull.Clear();
                ownCharacter.gameObject.SetActive(false);
                worldStatusText.text = string.Format(CultureInfo.InvariantCulture,
                    "AREA {0}\nPreparing your character…", zone);
                UpdateZoneTransitionButton();
                WorldStateChanged?.Invoke();
                return;
            }
            if (!EntryPending) return;
            CancelMovement();
            if (ActorMotion != null) ActorMotion.ResetPose();
            hasVisualPosition = false;
            if (internalId == 0 || !InsideZone(zone, new Vector2(x, y))) { Fail("Invalid server Zone result."); return; }
            ZoneID = zone;
            InternalID = internalId;
            ZoneStartPosition = new Vector2(x, y);
            Stage = WorldStage.WaitingForFull;
            entryStatusText.text = "Zone confirmed. Waiting for your matching server Full state...";
            WorldStateChanged?.Invoke();
            if (pendingFull.TryGetValue(InternalID, out var state)) ApplyOwnFull(state);
            pendingFull.Clear();
        }

        private void ZoneFailed()
        {
            if (Stage == WorldStage.TransitionPending)
            {
                if (ZoneID == transitionTargetZone)
                { RequireReconnect("Zone state became uncertain. Disconnect and reconnect."); return; }
                transitionDeadline = 0;
                transitionTargetZone = 0;
                Stage = WorldStage.InWorld;
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
            if (!EntryPending && Stage != WorldStage.InWorld && Stage != WorldStage.TransitionPending) return;
            if (Stage == WorldStage.TransitionPending && ZoneID != transitionTargetZone) return;
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
            bool lostHp = HasOwnFull && state.hp < ownFull.hp;
            if (!HasOwnFull) ownCharacter.rotation = ServerFacing(state.dir);
            Vector2 previousServer = HasOwnFull ? ServerPosition : new Vector2(state.x, state.y);
            ownFull = state;
            ReconcileVisual(previousServer, ServerPosition);
            if (ownFull.hp == 0)
            {
                StopActions();
                visualPosition = ServerPosition;
                hasVisualPosition = true;
            }
            HasOwnFull = true;
            Stage = WorldStage.InWorld;
            transitionDeadline = 0;
            transitionTargetZone = 0;
            ownCharacter.gameObject.SetActive(true);
            if (ActorMotion == null)
                ActorMotion = ownCharacter.GetComponent<ActorMotion>() ?? ownCharacter.gameObject.AddComponent<ActorMotion>();
            ActorMotion.Configure();
            if (lostHp) ActorMotion.PlayHit();
            if (ownFull.hp == 0) ActorMotion.ResetPose();
            worldRoot.SetActive(true);
            worldHud.SetActive(true);
            selectionPanel.SetActive(false);
            background.SetActive(false);
            loginScreen.SetBusy(false);
            PlaceFieldForZone();
            ShowPosition();
            UpdateZoneTransitionButton();
            WorldStateChanged?.Invoke();
        }

        private void DeltaReceived(ushort count, DeltaUpdateField[] updates)
        {
            if (updates == null || count > updates.Length) return;
            if (!EntryPending && Stage != WorldStage.InWorld) return;
            bool changed = false;
            var next = ownFull;
            for (int i = 0; i < count; i++)
            {
                var update = updates[i];
                if (update.fieldID > 8) continue;
                if (HasOwnFull && update.zoneInternalID == InternalID)
                {
                    if (update.fieldID <= 3)
                    {
                        if (update.fieldVal > int.MaxValue)
                        { Fail("Invalid server character vitals."); return; }
                        if (update.fieldID == 0) next.hp = (int)update.fieldVal;
                        else if (update.fieldID == 1) next.mp = (int)update.fieldVal;
                        else if (update.fieldID == 2) next.maxHp = (int)update.fieldVal;
                        else next.maxMp = (int)update.fieldVal;
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
                    else if (update.fieldID == 2) state.maxHp = unchecked((int)update.fieldVal);
                    else if (update.fieldID == 3) state.maxMp = unchecked((int)update.fieldVal);
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
                // The server writes current values before maxima; validate the complete frame together.
                if (next.hp < 0 || next.mp < 0 || next.maxHp < 0 || next.maxMp < 0 ||
                    next.hp > next.maxHp || next.mp > next.maxMp)
                { Fail("Invalid server character vitals."); return; }
                if (!InsideZone(ZoneID, new Vector2(next.x, next.y)))
                { Fail("Invalid server character position."); return; }
                bool lostHp = next.hp < ownFull.hp;
                Vector2 previousServer = ServerPosition;
                ownFull = next;
                ReconcileVisual(previousServer, ServerPosition);
                if (lostHp && ActorMotion != null) ActorMotion.PlayHit();
                if (ownFull.hp == 0) { StopActions(); visualPosition = ServerPosition; if (ActorMotion != null) ActorMotion.ResetPose(); }
                ShowPosition();
                WorldStateChanged?.Invoke();
            }
        }

        private void Update()
        {
            if (Stage == WorldStage.TransitionPending)
            {
                if (Time.unscaledTimeAsDouble >= transitionDeadline)
                    RequireReconnect("Zone transition timed out. Disconnect and reconnect before continuing.");
                UpdateVisualPosition(Vector2.zero, Time.unscaledDeltaTime);
                return;
            }
            if (Stage == WorldStage.ReconnectRequired)
            { UpdateVisualPosition(Vector2.zero, Time.unscaledDeltaTime); return; }
            if (!CanAct || model == null || loginScreen.Data == null || !Application.isFocused || Keyboard.current == null)
            { CancelMovement(); UpdateVisualPosition(Vector2.zero, Time.unscaledDeltaTime); return; }
            if (!GameUiInput.CanUseWorldShortcuts() ||
                inventoryScreen != null && inventoryScreen.IsOpen || bazaarScreen != null && bazaarScreen.IsOpen)
            { CancelMovement(); UpdateVisualPosition(Vector2.zero, Time.unscaledDeltaTime); return; }

            var keyboard = Keyboard.current;
            Vector2 direction = CameraRelativeDirection(keyboard, quarterViewCamera);
            if (direction == Vector2.zero)
            {
                CancelMovement();
                UpdateVisualPosition(Vector2.zero, Time.unscaledDeltaTime);
                return;
            }
            UpdateVisualPosition(direction, Time.unscaledDeltaTime);
            if (hasMovementIntent)
            {
                if (TryMoveCandidate(ZoneID, ServerPosition, inputTarget, out var candidate, out var serverDirection) &&
                    (worldMap == null || worldMap.CanTraverse(ServerPosition, candidate)))
                    loginScreen.Data.UpdateMove(serverDirection, candidate.x, candidate.y);
                else loginScreen.Data.CancelMove();
                // ClientTick already coalesces the latest target into one send per 50ms.
                // A second frame-based timer adds drift and can miss a whole network tick.
            }
        }

        private static Vector2 CameraRelativeDirection(Keyboard keyboard, Transform cameraTransform)
        {
            Vector3 forward = Vector3.ProjectOnPlane(cameraTransform.forward, Vector3.up).normalized;
            Vector3 right = Vector3.ProjectOnPlane(cameraTransform.right, Vector3.up).normalized;
            Vector2 input = new Vector2((keyboard.rightArrowKey.isPressed ? 1 : 0) - (keyboard.leftArrowKey.isPressed ? 1 : 0),
                (keyboard.upArrowKey.isPressed ? 1 : 0) - (keyboard.downArrowKey.isPressed ? 1 : 0));
            if (input.sqrMagnitude > 1) input.Normalize();
            Vector3 world = right * input.x + forward * input.y;
            return world.sqrMagnitude > 0 ? new Vector2(world.x, world.z).normalized : Vector2.zero;
        }

        private void UpdateVisualPosition(Vector2 direction, float deltaSeconds)
        {
            if (!HasOwnFull) return;
            if (!hasVisualPosition) { visualPosition = ServerPosition; hasVisualPosition = true; }
            if (ownFull.hp == 0)
            {
                visualPosition = ServerPosition;
                visualCorrectionVelocity = Vector2.zero;
                RenderVisualPosition();
                return;
            }
            float elapsed = Finite(deltaSeconds) ? Mathf.Max(0, deltaSeconds) : 0;
            // Only one tick of held input is integrated after a frame hitch.
            float step = Mathf.Min(elapsed, (float)MovementIntervalSeconds) * MovementUnitsPerSecond;
            direction = direction.normalized;
            if (ActorMotion != null) ActorMotion.SetMovementIntent(direction);
            if (direction.sqrMagnitude > 0 && ownFull.hp > 0)
            {
                secondsWithoutInput = 0;
                // Every heading change discards unacknowledged lead from the released direction.
                if (!hasMovementIntent) secondsWithoutProgress = 0;
                if (!hasMovementIntent || (direction - movementDirection).sqrMagnitude > .000001f)
                    inputTarget = ServerPosition;
                movementDirection = direction;
                hasMovementIntent = true;
                secondsWithoutProgress = Mathf.Min(StaleMovementSeconds, secondsWithoutProgress + elapsed);
                bool stale = secondsWithoutProgress >= StaleMovementSeconds;
                Vector2 target = stale
                    ? ServerPosition + direction.normalized * (MovementUnitsPerSecond * (float)MovementIntervalSeconds)
                    : inputTarget + direction.normalized * step;
                target = ServerPosition + Vector2.ClampMagnitude(target - ServerPosition, MaximumRequestLead);
                if (InsideZoneBounds(ZoneID, target, MaximumRequestLead) && (worldMap == null ||
                    worldMap.CanTraverse(inputTarget, target) && worldMap.CanTraverse(ServerPosition, target)))
                    inputTarget = target;
                else
                {
                    // A blocked local path cannot keep an old invisible target queued.
                    CancelMovement();
                    stale = true;
                }
                // Integrate presentation from its current position, including on a turn.
                // Rebuilding it from delayed authority pulls the actor back on every reversal.
                Vector2 next;
                if (stale)
                    next = Vector2.SmoothDamp(visualPosition, ServerPosition, ref visualCorrectionVelocity,
                        .12f, MovementUnitsPerSecond, Mathf.Min(elapsed, (float)MovementIntervalSeconds));
                else
                {
                    visualCorrectionVelocity = Vector2.zero;
                    next = visualPosition + direction * step;
                }
                next = ServerPosition + Vector2.ClampMagnitude(next - ServerPosition, MaximumVisualLead);
                visualPosition = worldMap == null || worldMap.CanTraverse(visualPosition, next) &&
                    worldMap.CanTraverse(ServerPosition, next) ? next : ServerPosition;
            }
            else
            {
                if (hasMovementIntent) CancelMovement();
                secondsWithoutInput += elapsed;
                // With no movement ACK, use the same bounded wait as stalled held input.
                // A shorter key-up wait rewinds before delayed final snapshots can arrive.
                if (secondsWithoutInput >= StaleMovementSeconds)
                {
                    Vector2 next = Vector2.SmoothDamp(visualPosition, ServerPosition, ref visualCorrectionVelocity,
                        .12f, MovementUnitsPerSecond, Mathf.Min(elapsed, (float)MovementIntervalSeconds));
                    visualPosition = worldMap == null || worldMap.CanTraverse(visualPosition, next) ? next : ServerPosition;
                }
            }
            RenderVisualPosition();
        }

        private void ReconcileVisual(Vector2 previousServer, Vector2 server)
        {
            if (!hasVisualPosition || Vector2.Distance(previousServer, server) > 2f ||
                Vector2.Distance(visualPosition, server) > 3f)
            {
                CancelMovement();
                visualPosition = server;
                hasVisualPosition = true;
                visualCorrectionVelocity = Vector2.zero;
                if (ActorMotion != null) ActorMotion.ResetPose();
            }
            else if ((server - previousServer).sqrMagnitude > 0.000001f)
            {
                if (!hasMovementIntent || Vector2.Dot(server - previousServer, movementDirection) > .000001f)
                    secondsWithoutProgress = 0;
                // No sequence/ACK exists: keep only bounded current intent, never replay requests.
                if (hasMovementIntent)
                {
                    Vector2 pending = inputTarget - server;
                    // A server correction that passes the pending target must not request a walk back to it.
                    inputTarget = server + movementDirection *
                        Mathf.Clamp(Vector2.Dot(pending, movementDirection), 0, MaximumRequestLead);
                    if (worldMap != null && !worldMap.CanTraverse(server, inputTarget)) CancelMovement();
                }
            }
        }

        private void RenderVisualPosition()
        {
            if (!HasOwnFull || ownCharacter == null) return;
            Vector2 position2 = hasVisualPosition ? visualPosition : ServerPosition;
            var position = new Vector3(position2.x, 0, position2.y);
            ownCharacter.position = position;
            fieldPlane.position = new Vector3(ownFull.x, 0, ownFull.y);
            quarterViewCamera.position = position + new Vector3(12, 16, -12);
            quarterViewCamera.LookAt(position + Vector3.up);
        }

        private static bool InsideZone(ushort zone, Vector2 position)
            => InsideZoneBounds(zone, position, 0);

        private static bool InsideZoneBounds(ushort zone, Vector2 position, float extraMargin)
        {
            if (zone < 1 || zone > 6 || !Finite(position.x) || !Finite(position.y)) return false;
            int column = (zone - 1) % 3, row = (zone - 1) / 3;
            return position.x >= column * WorldMap.ZoneSize - 5 - extraMargin && position.x <= (column + 1) * WorldMap.ZoneSize + 5 + extraMargin &&
                position.y >= row * WorldMap.ZoneSize - 5 - extraMargin && position.y <= (row + 1) * WorldMap.ZoneSize + 5 + extraMargin;
        }

        private static bool TryMoveCandidate(ushort zone, Vector2 origin, Vector2 target, out Vector2 candidate, out byte direction)
        {
            candidate = origin;
            direction = 0;
            if (!InsideZone(zone, origin) || !InsideZoneBounds(zone, target, MaximumRequestLead)) return false;
            Vector2 delta = target - origin;
            float distance = delta.magnitude;
            if (distance <= 0.05f) return false;
            candidate = origin + delta * (Mathf.Min(MaximumRequestLead, distance) / distance);
            // Movement convention: 0=+Y, 1=-Y, 2=-X, 3=+X. Ties choose X.
            direction = Mathf.Abs(delta.y) > Mathf.Abs(delta.x)
                ? (byte)(delta.y >= 0 ? 0 : 1) : (byte)(delta.x < 0 ? 2 : 3);
            return true;
        }

        public void CancelMovement()
        {
            loginScreen?.Data?.CancelMove();
            if (hasMovementIntent)
            {
                secondsWithoutInput = 0;
                visualCorrectionVelocity = Vector2.zero;
            }
            hasMovementIntent = false;
            movementDirection = Vector2.zero;
            if (ActorMotion != null) ActorMotion.SetMovementIntent(Vector2.zero);
            inputTarget = ServerPosition;
            secondsWithoutProgress = 0;
        }

        public void StopActions()
        {
            loginScreen?.Data?.CancelAction();
            CancelMovement();
        }

        private void OnApplicationFocus(bool focused) { if (!focused) CancelMovement(); }

        private void ZoneTransitionStarted(ulong ownershipEpoch, ushort targetZone)
        {
            if ((Stage != WorldStage.InWorld && Stage != WorldStage.TransitionPending) || !HasOwnFull) return;
            if (targetZone == 0 || targetZone > 6)
            { RequireReconnect("The server returned an unsupported area. Disconnect and reconnect."); return; }
            StopActions();
            if (ActorMotion != null) ActorMotion.ResetPose();
            transitionTargetZone = targetZone;
            transitionDeadline = Time.unscaledTimeAsDouble + ZoneTransitionTimeoutSeconds;
            Stage = WorldStage.TransitionPending;
            worldStatusText.text = string.Format(CultureInfo.InvariantCulture,
                "Travelling to area {0}…\nWaiting for the complete server state.", targetZone);
            // Keep the previous field and actors visible until the paired baseline is ready.
            UpdateZoneTransitionButton();
            WorldStateChanged?.Invoke();
        }

        private void RequireReconnect(string message)
        {
            StopActions();
            transitionDeadline = 0;
            Stage = WorldStage.ReconnectRequired;
            if (worldStatusText != null) worldStatusText.text = message;
            UpdateZoneTransitionButton();
            WorldStateChanged?.Invoke();
        }

        private void UpdateZoneTransitionButton()
        {
            // The server decides ownership from validated movement; there is no manual travel request.
            if (zoneTransitionButton != null) zoneTransitionButton.gameObject.SetActive(false);
        }

        private void PlaceFieldForZone()
        {
            // Move only authored scenery. Actors and camera remain in server world coordinates.
            Vector3 origin = new Vector3(((ZoneID - 1) % 3) * WorldMap.ZoneSize, 0, ((ZoneID - 1) / 3) * WorldMap.ZoneSize);
            var scenery = worldRoot.transform.Find("V04MeanderingMeadow");
            if (scenery != null) scenery.localPosition = origin;
            scenery = worldRoot.transform.Find("V03FieldSurround");
            if (scenery != null) scenery.localPosition = origin;
            scenery = worldRoot.transform.Find("V01Outpost");
            if (scenery != null)
            {
                scenery.gameObject.SetActive(ZoneID == 1);
                scenery.localPosition = origin + new Vector3(10, 0, 10);
            }
        }

        // The authored hero faces local -Z (Vector3.back).
        internal static Quaternion ServerFacing(byte direction) => Quaternion.Euler(0,
            direction == 0 ? 180 : direction == 1 ? 0 : direction == 2 ? 90 : 270, 0);

        private void ShowPosition()
        {
            if (!hasVisualPosition) { visualPosition = ServerPosition; hasVisualPosition = true; }
            // Root facing is seeded on entry. C19 turns only the visual model during movement/actions.
            RenderVisualPosition();
            worldStatusText.text = string.Format(CultureInfo.InvariantCulture,
                "C02 / SERVER-CONFIRMED FIELD · {5}\nCharacter ID {0}    Internal ID {1}\nZone {2}    Server (x, y): ({3:F3}, {4:F3})\nArrow keys move. Z / X / C use skills. I opens bag. B opens market.",
                SelectedCharacterID, InternalID, ZoneID, ownFull.x, ownFull.y, ZoneID == 1 ? "TOWN / SAFE AREA" : "HUNTING FIELD");
            UpdateZoneTransitionButton();
        }

        private void ResetWorld()
        {
            CancelMovement();
            if (ActorMotion != null) ActorMotion.ResetPose();
            hasVisualPosition = false;
            visualPosition = Vector2.zero;
            secondsWithoutInput = 0;
            visualCorrectionVelocity = Vector2.zero;
            pendingFull.Clear();
            ownFull = default;
            HasOwnFull = false;
            inputTarget = Vector2.zero;
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
            hasVisualPosition = false;
            loginScreen.ConnectionFailed(message);
            Stage = WorldStage.Failed;
            entryStatusText.text = message + " Sign in to retry.";
        }

        private void Disconnect() => loginScreen.ConnectionFailed("Session closed.");
        private static bool Finite(float value) => !float.IsNaN(value) && !float.IsInfinity(value);

        private void OnDisable()
        {
            CancelMovement();
            if (ActorMotion != null) ActorMotion.ResetPose();
            hasVisualPosition = false;
            if (loginScreen != null)
            {
                loginScreen.ClientChanged -= ChangeClient;
                loginScreen.CharacterListReceived -= ListReceived;
            }
            if (enterButton != null) enterButton.onClick.RemoveListener(EnterSelected);
            if (disconnectButton != null) disconnectButton.onClick.RemoveListener(Disconnect);
            if (zoneTransitionButton != null) zoneTransitionButton.gameObject.SetActive(false);
            foreach (var choice in characterChoices)
                if (choice != null) choice.onClick.RemoveAllListeners();
            if (worldRoot != null) worldRoot.SetActive(false);
            if (worldHud != null) worldHud.SetActive(false);
            DetachModel();
            pendingFull.Clear();
            ownFull = default;
            HasOwnFull = false;
            inputTarget = Vector2.zero;
            InternalID = 0;
            ZoneID = 0;
            ZoneStartPosition = Vector2.zero;
            transitionDeadline = 0;
            transitionTargetZone = 0;
            SelectedCharacterID = 0;
            Stage = WorldStage.Ready;
            WorldStateChanged?.Invoke();
            if (loginScreen != null) loginScreen.ReleaseClient();
        }
    }
}
