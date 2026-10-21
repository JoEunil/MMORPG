using System;
using System.Globalization;
using C01;
using ClientCore;
using ClientCore.PacketHelper;
using TMPro;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.InputSystem;
using UnityEngine.UI;

namespace C02
{
    public sealed class C05CombatScreen : MonoBehaviour
    {
        [SerializeField] private C01LoginScreen loginScreen;
        [SerializeField] private C02WorldScreen worldScreen;
        [SerializeField] private C09BazaarScreen bazaarScreen;
        [SerializeField] private C07InventoryScreen inventoryScreen;
        [SerializeField] private C14ServerContent serverContent;
        [SerializeField] private UnityEngine.UI.Button[] skillButtons;
        [SerializeField] private GameObject combatPanel;
        [SerializeField] private TMP_Text vitalsText;
        [SerializeField] private TMP_Text hpValueText;
        [SerializeField] private TMP_Text mpValueText;
        [SerializeField] private TMP_Text combatStatusText;
        [SerializeField] private TMP_Text aimText;
        [SerializeField] private UnityEngine.UI.Image hpFill;
        [SerializeField] private UnityEngine.UI.Image mpFill;
        private IViewModelUI model;
        private Action<ushort, ActionResultField[]> actionResults;
        private LineRenderer linePreview;
        private Material linePreviewMaterial;
        private float linePreviewDeadline;
        private GameObject skillTooltip;
        private TMP_Text skillTooltipText;
        private int tooltipSlot = -1;
        private int generation;
        private ushort observedZone;
        private uint ownId;
        private const float LineRangeLength = 12f;
        private const float LineRangeWidth = 3f;
        private const float LinePreviewLifetime = 2f;

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
            HideLinePreview();
            HideSkillTooltip();
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
            if (combatPanel != null) combatPanel.SetActive(WorldAvailable);
            if (WorldAvailable)
            {
                if (observedZone == 0) { observedZone = worldScreen.ZoneID; ownId = worldScreen.InternalID; }
                else if (observedZone != worldScreen.ZoneID || ownId != worldScreen.InternalID) ResetObservation();
                var state = worldScreen.OwnFull;
                if (state.hp == 0) { HideLinePreview(); HideSkillTooltip(); }
                vitalsText.text = string.Format(CultureInfo.InvariantCulture, "HP {0} / {1}    MP {2} / {3}",
                    state.hp, state.maxHp, state.mp, state.maxMp);
                if (hpValueText != null) hpValueText.text = string.Format(CultureInfo.InvariantCulture, "HP {0} / {1}", state.hp, state.maxHp);
                if (mpValueText != null) mpValueText.text = string.Format(CultureInfo.InvariantCulture, "MP {0} / {1}", state.mp, state.maxMp);
                if (hpFill != null) hpFill.fillAmount = GaugeAmount(state.hp, state.maxHp);
                if (mpFill != null) mpFill.fillAmount = GaugeAmount(state.mp, state.maxMp);
                aimText.text = string.Format(CultureInfo.InvariantCulture, "Lv {0}  EXP {1}  |  Line aim: {2}",
                    state.level, state.exp, LineDirection(state.dir));
                if (state.hp == 0) combatStatusText.text = "HP 0 — defeated. Disconnect to leave.";
                else if (combatStatusText.text.StartsWith("HP 0", StringComparison.Ordinal)) combatStatusText.text = string.Empty;
            }
            else
            {
                HideLinePreview();
                HideSkillTooltip();
                if (observedZone != 0 || ownId != 0) ResetObservation();
                vitalsText.text = string.Empty;
                if (hpValueText != null) hpValueText.text = string.Empty;
                if (mpValueText != null) mpValueText.text = string.Empty;
                if (hpFill != null) hpFill.fillAmount = 0;
                if (mpFill != null) mpFill.fillAmount = 0;
                aimText.text = string.Empty;
                combatStatusText.text = string.Empty;
            }
            foreach (var button in skillButtons)
                if (button != null) button.interactable = model != null && WorldAvailable && worldScreen.CanAct;
        }

        private void ResetObservation()
        {
            HideLinePreview();
            HideSkillTooltip();
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
                !WorldAvailable || !worldScreen.CanAct || !Application.isFocused ||
                bazaarScreen != null && bazaarScreen.IsOpen || inventoryScreen != null && inventoryScreen.IsOpen) return false;
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
            if (linePreview != null && linePreview.enabled && Time.unscaledTime >= linePreviewDeadline)
                HideLinePreview();
            UpdateSkillTooltip();
            if (!InputAvailable() || Keyboard.current == null) return;
            if (Keyboard.current.zKey.wasPressedThisFrame) RequestSkill(0);
            else if (Keyboard.current.xKey.wasPressedThisFrame) RequestSkill(1);
            else if (Keyboard.current.cKey.wasPressedThisFrame) RequestSkill(2);
        }

        private void ApplyResults(ushort count, ActionResultField[] results)
        {
            if (!WorldAvailable || worldScreen.OwnFull.hp == 0)
            {
                HideLinePreview();
                return;
            }
            if (results == null || count > results.Length) return;
            for (int i = 0; i < count; i++)
            {
                var result = results[i];
                // Only the current player's supported server phases are displayed. No hit/kill ACK exists.
                if (result.casterType != 0 || result.casterId != ownId || result.casterId == 0 || result.dir > 3 ||
                    result.skillSlot > 2 || result.skillId != result.skillSlot || result.skillPhase > 1 ||
                    !Finite(result.x) || !Finite(result.y)) continue;
                combatStatusText.text = SkillName(result.skillSlot) + (result.skillPhase == 0 ? " cast" : " released");
                if (result.skillSlot == 2)
                {
                    if (result.skillPhase == 0) ShowLinePreview(result);
                    else HideLinePreview();
                }
            }
        }

        private void ShowLinePreview(ActionResultField result)
        {
            if (linePreview == null)
            {
                var shader = Shader.Find("Universal Render Pipeline/Unlit");
                if (shader == null) return;
                var previewObject = new GameObject("C05 Line Skill Range");
                previewObject.transform.SetParent(worldScreen.transform, false);
                linePreview = previewObject.AddComponent<LineRenderer>();
                linePreviewMaterial = new Material(shader) { color = new Color(1f, 0.72f, 0.16f, 1f) };
                linePreview.sharedMaterial = linePreviewMaterial;
                linePreview.useWorldSpace = true;
                linePreview.loop = true;
                linePreview.positionCount = 4;
                linePreview.widthMultiplier = 0.12f;
                linePreview.startColor = linePreview.endColor = linePreviewMaterial.color;
            }

            // ActionResult has no dimensions; this outline mirrors the fixed server skill 2 contract (12 x 3).
            // Points use server x/y as global X/Z, so the C02Client parent transform does not offset the range.
            float halfWidth = LineRangeWidth * 0.5f;
            float x = result.x;
            float z = result.y;
            const float groundY = 0.05f;
            switch (result.dir)
            {
                case 0:
                    linePreview.SetPosition(0, new Vector3(x - halfWidth, groundY, z));
                    linePreview.SetPosition(1, new Vector3(x + halfWidth, groundY, z));
                    linePreview.SetPosition(2, new Vector3(x + halfWidth, groundY, z + LineRangeLength));
                    linePreview.SetPosition(3, new Vector3(x - halfWidth, groundY, z + LineRangeLength));
                    break;
                case 1:
                    linePreview.SetPosition(0, new Vector3(x - halfWidth, groundY, z));
                    linePreview.SetPosition(1, new Vector3(x + halfWidth, groundY, z));
                    linePreview.SetPosition(2, new Vector3(x + halfWidth, groundY, z - LineRangeLength));
                    linePreview.SetPosition(3, new Vector3(x - halfWidth, groundY, z - LineRangeLength));
                    break;
                case 2:
                    linePreview.SetPosition(0, new Vector3(x, groundY, z - halfWidth));
                    linePreview.SetPosition(1, new Vector3(x, groundY, z + halfWidth));
                    linePreview.SetPosition(2, new Vector3(x - LineRangeLength, groundY, z + halfWidth));
                    linePreview.SetPosition(3, new Vector3(x - LineRangeLength, groundY, z - halfWidth));
                    break;
                default:
                    linePreview.SetPosition(0, new Vector3(x, groundY, z - halfWidth));
                    linePreview.SetPosition(1, new Vector3(x, groundY, z + halfWidth));
                    linePreview.SetPosition(2, new Vector3(x + LineRangeLength, groundY, z + halfWidth));
                    linePreview.SetPosition(3, new Vector3(x + LineRangeLength, groundY, z - halfWidth));
                    break;
            }
            linePreview.enabled = true;
            linePreviewDeadline = Time.unscaledTime + LinePreviewLifetime;
        }

        private void HideLinePreview()
        {
            if (linePreview != null) linePreview.enabled = false;
            linePreviewDeadline = 0;
        }

        private void UpdateSkillTooltip()
        {
            if (!InputAvailable() || Mouse.current == null)
            {
                HideSkillTooltip();
                return;
            }

            Vector2 pointer = Mouse.current.position.ReadValue();
            for (int i = 0; i < skillButtons.Length; i++)
            {
                var button = skillButtons[i];
                var rect = button.GetComponent<RectTransform>();
                var canvas = button.GetComponentInParent<Canvas>();
                if (rect == null || canvas == null) continue;
                Camera eventCamera = canvas.renderMode == RenderMode.ScreenSpaceOverlay ? null : canvas.worldCamera;
                if (canvas.renderMode != RenderMode.ScreenSpaceOverlay && eventCamera == null) continue;
                if (RectTransformUtility.RectangleContainsScreenPoint(rect, pointer, eventCamera))
                {
                    ShowSkillTooltip(i);
                    return;
                }
            }
            HideSkillTooltip();
        }

        private void ShowSkillTooltip(int slot)
        {
            if (slot < 0 || slot > 2 || serverContent == null)
            {
                HideSkillTooltip();
                return;
            }
            // Authored skill content is fixed during play; rebuild text only when hover changes.
            if (tooltipSlot == slot && skillTooltip != null && skillTooltip.activeSelf) return;
            C14ServerContent.RingSkill skill = slot == 1 ? serverContent.ring :
                serverContent.otherSkills == null ? null : Array.Find(serverContent.otherSkills, candidate => candidate != null && candidate.skillID == slot);
            if (skill == null || skill.skillID != slot || skill.phases == null)
            {
                HideSkillTooltip();
                return;
            }

            C14ServerContent.SkillPhase hit = null;
            foreach (var phase in skill.phases)
            {
                if (phase == null || phase.action != C14SkillAction.Charging && phase.action != C14SkillAction.Hit)
                {
                    HideSkillTooltip();
                    return;
                }
                if (phase.action == C14SkillAction.Hit) { hit = phase; break; }
            }
            if (hit == null) { HideSkillTooltip(); return; }

            string shape;
            if (hit.range == C14SkillRange.Circle && Finite(hit.range1) && hit.range1 > 0 && hit.range2 == 0)
                shape = string.Format(CultureInfo.InvariantCulture, "Circle · Radius {0:0.##}", hit.range1);
            else if (hit.range == C14SkillRange.Rectangle && Finite(hit.range1) && hit.range1 > 0 &&
                     Finite(hit.range2) && hit.range2 > 0)
                shape = string.Format(CultureInfo.InvariantCulture, "Line · Length {0:0.##} · Width {1:0.##}", hit.range1, hit.range2);
            else
            {
                HideSkillTooltip();
                return;
            }

            if (combatPanel == null || aimText == null) { HideSkillTooltip(); return; }
            if (skillTooltip == null)
            {
                var tooltipRect = new GameObject("C05 Skill Tooltip", typeof(RectTransform), typeof(CanvasRenderer), typeof(Image));
                skillTooltip = tooltipRect;
                tooltipRect.SetActive(false);
                tooltipRect.transform.SetParent(combatPanel.transform, false);
                var rect = tooltipRect.GetComponent<RectTransform>();
                rect.anchorMin = rect.anchorMax = new Vector2(.5f, 1);
                rect.pivot = new Vector2(.5f, 0);
                rect.anchoredPosition = new Vector2(0, 12);
                rect.sizeDelta = new Vector2(320, 96);
                var background = tooltipRect.GetComponent<Image>();
                background.color = new Color(.12f, .105f, .08f, .96f);
                background.raycastTarget = false;

                var labelObject = new GameObject("Description", typeof(RectTransform), typeof(CanvasRenderer), typeof(TextMeshProUGUI));
                labelObject.transform.SetParent(tooltipRect.transform, false);
                skillTooltipText = labelObject.GetComponent<TextMeshProUGUI>();
                skillTooltipText.font = aimText.font;
                skillTooltipText.fontSize = 16;
                skillTooltipText.color = new Color(.96f, .87f, .65f);
                skillTooltipText.alignment = TextAlignmentOptions.MidlineLeft;
                skillTooltipText.enableWordWrapping = false;
                skillTooltipText.raycastTarget = false;
                var labelRect = skillTooltipText.rectTransform;
                labelRect.anchorMin = Vector2.zero;
                labelRect.anchorMax = Vector2.one;
                labelRect.pivot = new Vector2(.5f, .5f);
                labelRect.offsetMin = new Vector2(14, 10);
                labelRect.offsetMax = new Vector2(-14, -10);
            }

            string key = slot == 0 ? "Z" : slot == 1 ? "X" : "C";
            string name = SkillName((byte)slot);
            skillTooltipText.text = string.Format(CultureInfo.InvariantCulture,
                "<size=20><b>{0} · {1}</b></size>\n{2} · MP {3}", key, name, shape, skill.mana);
            skillTooltip.SetActive(true);
            skillTooltip.transform.SetAsLastSibling();
            tooltipSlot = slot;
        }

        private void HideSkillTooltip()
        {
            tooltipSlot = -1;
            if (skillTooltip != null) skillTooltip.SetActive(false);
            if (skillTooltipText != null) skillTooltipText.text = string.Empty;
        }

        // Matches movement: 0=North, 1=South, 2=West, 3=East.
        private static string LineDirection(byte dir) => dir == 0 ? "North" : dir == 1 ? "South" : dir == 2 ? "West" : "East";
        private static string SkillName(byte slot) => slot == 0 ? "Strike" : slot == 1 ? "Ring" : "Line";
        private static float GaugeAmount(int current, int maximum) => maximum <= 0 ? 0 : Mathf.Clamp01((float)current / maximum);
        private static bool Finite(float value) => !float.IsNaN(value) && !float.IsInfinity(value);

        private void OnDisable()
        {
            HideLinePreview();
            HideSkillTooltip();
            if (combatPanel != null) combatPanel.SetActive(false);
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
            if (hpValueText != null) hpValueText.text = string.Empty;
            if (mpValueText != null) mpValueText.text = string.Empty;
            if (hpFill != null) hpFill.fillAmount = 0;
            if (mpFill != null) mpFill.fillAmount = 0;
            if (combatStatusText != null) combatStatusText.text = string.Empty;
            if (aimText != null) aimText.text = string.Empty;
        }

        private void OnDestroy()
        {
            HideLinePreview();
            if (skillTooltip != null)
            {
                if (Application.isPlaying) Destroy(skillTooltip);
                else DestroyImmediate(skillTooltip);
                skillTooltip = null;
                skillTooltipText = null;
            }
            if (linePreview != null)
            {
                if (Application.isPlaying) Destroy(linePreview.gameObject);
                else DestroyImmediate(linePreview.gameObject);
                linePreview = null;
            }
            if (linePreviewMaterial != null)
            {
                if (Application.isPlaying) Destroy(linePreviewMaterial);
                else DestroyImmediate(linePreviewMaterial);
                linePreviewMaterial = null;
            }
        }
    }
}
