using System;
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
    public sealed class C05CombatScreen : MonoBehaviour
    {
        [SerializeField] private C01LoginScreen loginScreen;
        [SerializeField] private C02WorldScreen worldScreen;
        [SerializeField] private UnityEngine.UI.Button[] skillButtons;
        [SerializeField] private TMP_Text vitalsText;
        [SerializeField] private TMP_Text combatStatusText;
        [SerializeField] private TMP_Text aimText;
        private IViewModelUI model;
        private Action<ushort, ActionResultField[]> actionResults;
        private int generation;
        private ushort observedZone;
        private uint ownId;

        private bool WorldAvailable => worldScreen != null && worldScreen.isActiveAndEnabled &&
            worldScreen.Stage == C02Stage.InWorld && worldScreen.HasOwnFull;

        private void OnEnable()
        {
            if (loginScreen == null || worldScreen == null || skillButtons == null || skillButtons.Length != 3 ||
                Array.Exists(skillButtons, button => button == null) || vitalsText == null || combatStatusText == null || aimText == null)
            {
                Debug.LogError("C05 combat references are missing.", this);
                enabled = false;
                return;
            }
            skillButtons[0].onClick.AddListener(Strike);
            skillButtons[1].onClick.AddListener(Ring);
            skillButtons[2].onClick.AddListener(Line);
            loginScreen.ClientChanged += ChangeClient;
            worldScreen.WorldStateChanged += WorldChanged;
            ChangeClient(loginScreen.Model);
        }

        private void Strike() => RequestSkill(0);
        private void Ring() => RequestSkill(1);
        private void Line() => RequestSkill(2);

        private void ChangeClient(IViewModelUI next)
        {
            Unsubscribe();
            generation++;
            model = next;
            observedZone = 0;
            ownId = 0;
            combatStatusText.text = string.Empty;
            Subscribe();
            WorldChanged();
        }

        private void Subscribe()
        {
            if (model == null) return;
            var source = model;
            int version = generation;
            actionResults = (count, results) =>
            {
                if (!isActiveAndEnabled || source != model || version != generation || !WorldAvailable) return;
                WorldChanged();
                if (source == model && version == generation) ApplyResults(count, results);
            };
            model.OnActionResultReceived += actionResults;
        }

        private void Unsubscribe()
        {
            if (model != null && actionResults != null) model.OnActionResultReceived -= actionResults;
            actionResults = null;
        }

        private void WorldChanged()
        {
            if (WorldAvailable)
            {
                if (observedZone == 0) { observedZone = worldScreen.ZoneID; ownId = worldScreen.InternalID; }
                else if (observedZone != worldScreen.ZoneID || ownId != worldScreen.InternalID) ResetObservation();
                var state = worldScreen.OwnFull;
                vitalsText.text = string.Format(CultureInfo.InvariantCulture, "HP {0} / {1}    MP {2} / {3}",
                    state.hp, state.maxHp, state.mp, state.maxMp);
                aimText.text = "Line aim: " + LineDirection(state.dir);
                if (state.hp == 0) combatStatusText.text = "HP 0 — movement and combat stopped. Disconnect to leave.";
                else if (combatStatusText.text.StartsWith("HP 0", StringComparison.Ordinal)) combatStatusText.text = string.Empty;
            }
            else
            {
                if (observedZone != 0 || ownId != 0) ResetObservation();
                vitalsText.text = string.Empty;
                aimText.text = string.Empty;
                combatStatusText.text = string.Empty;
            }
            foreach (var button in skillButtons)
                if (button != null) button.interactable = model != null && WorldAvailable && worldScreen.CanAct;
        }

        private void ResetObservation()
        {
            Unsubscribe();
            generation++;
            observedZone = WorldAvailable ? worldScreen.ZoneID : (ushort)0;
            ownId = WorldAvailable ? worldScreen.InternalID : 0;
            combatStatusText.text = string.Empty;
            Subscribe();
        }

        private bool InputAvailable()
        {
            if (!isActiveAndEnabled || model == null || loginScreen.Model != model || loginScreen.Data == null ||
                !WorldAvailable || !worldScreen.CanAct || !Application.isFocused) return false;
            var events = EventSystem.current;
            if (events == null) return false;
            var input = events.currentSelectedGameObject != null
                ? events.currentSelectedGameObject.GetComponentInParent<TMP_InputField>() : null;
            return input == null || !input.isFocused;
        }

        private void RequestSkill(byte slot)
        {
            if (slot > 2 || !InputAvailable()) return;
            worldScreen.CancelMovement();
            var state = worldScreen.OwnFull;
            loginScreen.Data.UpdateAction(state.dir, state.x, state.y, slot);
            combatStatusText.text = SkillName(slot) + " requested";
        }

        private void Update()
        {
            if (!InputAvailable() || Keyboard.current == null) return;
            if (Keyboard.current.digit1Key.wasPressedThisFrame) RequestSkill(0);
            else if (Keyboard.current.digit2Key.wasPressedThisFrame) RequestSkill(1);
            else if (Keyboard.current.digit3Key.wasPressedThisFrame) RequestSkill(2);
        }

        private void ApplyResults(ushort count, ActionResultField[] results)
        {
            if (!WorldAvailable || worldScreen.OwnFull.hp == 0 || results == null || count > results.Length) return;
            for (int i = 0; i < count; i++)
            {
                var result = results[i];
                // Only the current player's supported server phases are displayed. No hit/kill ACK exists.
                if (result.casterType != 0 || result.casterId != ownId || result.casterId == 0 || result.dir > 3 ||
                    result.skillSlot > 2 || result.skillId != result.skillSlot || result.skillPhase > 1 ||
                    !Finite(result.x) || !Finite(result.y)) continue;
                combatStatusText.text = SkillName(result.skillSlot) + (result.skillPhase == 0 ? " cast" : " released");
            }
        }

        // Rectangular skill semantics differ from movement's vertical directions on this server.
        private static string LineDirection(byte dir) => dir == 0 ? "South" : dir == 1 ? "North" : dir == 2 ? "West" : "East";
        private static string SkillName(byte slot) => slot == 0 ? "Strike" : slot == 1 ? "Ring" : "Line";
        private static bool Finite(float value) => !float.IsNaN(value) && !float.IsInfinity(value);

        private void OnDisable()
        {
            loginScreen?.Data?.CancelAction();
            worldScreen?.CancelMovement();
            Unsubscribe();
            generation++;
            model = null;
            observedZone = 0;
            ownId = 0;
            if (loginScreen != null) loginScreen.ClientChanged -= ChangeClient;
            if (worldScreen != null) worldScreen.WorldStateChanged -= WorldChanged;
            if (skillButtons != null && skillButtons.Length == 3)
            {
                if (skillButtons[0] != null) skillButtons[0].onClick.RemoveListener(Strike);
                if (skillButtons[1] != null) skillButtons[1].onClick.RemoveListener(Ring);
                if (skillButtons[2] != null) skillButtons[2].onClick.RemoveListener(Line);
                foreach (var button in skillButtons) if (button != null) button.interactable = false;
            }
            if (vitalsText != null) vitalsText.text = string.Empty;
            if (combatStatusText != null) combatStatusText.text = string.Empty;
            if (aimText != null) aimText.text = string.Empty;
        }
    }
}
