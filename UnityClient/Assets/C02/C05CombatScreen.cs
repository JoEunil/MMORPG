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
        private GameObject defeatPanel;
        private UnityEngine.UI.Button respawnButton;
        private TMP_Text defeatMessage;
        private double respawnDeadline;
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
            HideDefeatPanel();
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
                if (state.hp == 0)
                {
                    HideLinePreview();
                    HideSkillTooltip();
                    combatStatusText.text = string.Empty;
                    ShowDefeatPanel();
                }
                else
                {
                    HideDefeatPanel();
                }
                vitalsText.text = string.Format(CultureInfo.InvariantCulture, "HP {0} / {1}    MP {2} / {3}",
                    state.hp, state.maxHp, state.mp, state.maxMp);
                if (hpValueText != null) hpValueText.text = string.Format(CultureInfo.InvariantCulture, "HP {0} / {1}", state.hp, state.maxHp);
                if (mpValueText != null) mpValueText.text = string.Format(CultureInfo.InvariantCulture, "MP {0} / {1}", state.mp, state.maxMp);
                if (hpFill != null) hpFill.fillAmount = GaugeAmount(state.hp, state.maxHp);
                if (mpFill != null) mpFill.fillAmount = GaugeAmount(state.mp, state.maxMp);
                aimText.text = string.Format(CultureInfo.InvariantCulture, "Lv {0}  EXP {1}  |  Line aim: {2}",
                    state.level, state.exp, LineDirection(state.dir));
            }
            else
            {
                HideLinePreview();
                HideSkillTooltip();
                HideDefeatPanel();
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
            HideDefeatPanel();
            Unsubscribe();
            generation++;
            observedZone = WorldAvailable ? worldScreen.ZoneID : (ushort)0;
            ownId = WorldAvailable ? worldScreen.InternalID : 0;
            combatStatusText.text = string.Empty;
            Subscribe();
        }

        private void ShowDefeatPanel()
        {
            if (defeatPanel == null)
            {
                var canvas = combatPanel != null ? combatPanel.GetComponentInParent<Canvas>() : null;
                if (canvas == null) return;

                defeatPanel = new GameObject("C05 Defeated", typeof(RectTransform), typeof(CanvasRenderer), typeof(UnityEngine.UI.Image));
                defeatPanel.transform.SetParent(canvas.transform, false);
                var overlayRect = defeatPanel.GetComponent<RectTransform>();
                overlayRect.anchorMin = Vector2.zero;
                overlayRect.anchorMax = Vector2.one;
                overlayRect.offsetMin = Vector2.zero;
                overlayRect.offsetMax = Vector2.zero;
                var overlay = defeatPanel.GetComponent<UnityEngine.UI.Image>();
                overlay.color = new Color(0.025f, 0.02f, 0.015f, 0.68f);
                overlay.raycastTarget = true;

                var cardObject = new GameObject("Defeat Message", typeof(RectTransform), typeof(CanvasRenderer), typeof(UnityEngine.UI.Image));
                cardObject.transform.SetParent(defeatPanel.transform, false);
                var cardRect = cardObject.GetComponent<RectTransform>();
                cardRect.anchorMin = cardRect.anchorMax = new Vector2(0.5f, 0.5f);
                cardRect.sizeDelta = new Vector2(380, 220);
                var card = cardObject.GetComponent<UnityEngine.UI.Image>();
                card.color = new Color(0.13f, 0.095f, 0.055f, 0.99f);
                card.raycastTarget = false;

                var titleObject = new GameObject("Title", typeof(RectTransform), typeof(CanvasRenderer), typeof(TextMeshProUGUI));
                titleObject.transform.SetParent(cardObject.transform, false);
                var titleRect = titleObject.GetComponent<RectTransform>();
                titleRect.anchorMin = titleRect.anchorMax = new Vector2(0.5f, 0.5f);
                titleRect.anchoredPosition = new Vector2(0, 58);
                titleRect.sizeDelta = new Vector2(340, 48);
                var title = titleObject.GetComponent<TextMeshProUGUI>();
                title.font = aimText.font;
                title.fontSize = 30;
                title.fontStyle = FontStyles.Bold;
                title.color = new Color(0.96f, 0.76f, 0.36f);
                title.alignment = TextAlignmentOptions.Center;
                title.raycastTarget = false;
                title.text = "DEFEATED";

                var messageObject = new GameObject("Message", typeof(RectTransform), typeof(CanvasRenderer), typeof(TextMeshProUGUI));
                messageObject.transform.SetParent(cardObject.transform, false);
                var messageRect = messageObject.GetComponent<RectTransform>();
                messageRect.anchorMin = messageRect.anchorMax = new Vector2(0.5f, 0.5f);
                messageRect.anchoredPosition = new Vector2(0, 4);
                messageRect.sizeDelta = new Vector2(330, 60);
                var message = messageObject.GetComponent<TextMeshProUGUI>();
                message.font = aimText.font;
                message.fontSize = 18;
                message.color = new Color(0.91f, 0.88f, 0.81f);
                message.alignment = TextAlignmentOptions.Center;
                message.raycastTarget = false;
                message.text = "Your character has fallen.\nReturn to town to recover, or log out.";
                defeatMessage = message;

                var buttonObject = new GameObject("Log Out", typeof(RectTransform), typeof(CanvasRenderer), typeof(UnityEngine.UI.Image), typeof(UnityEngine.UI.Button));
                buttonObject.transform.SetParent(cardObject.transform, false);
                var buttonRect = buttonObject.GetComponent<RectTransform>();
                buttonRect.anchorMin = buttonRect.anchorMax = new Vector2(0.5f, 0.5f);
                buttonRect.anchoredPosition = new Vector2(88, -68);
                buttonRect.sizeDelta = new Vector2(158, 48);
                var buttonImage = buttonObject.GetComponent<UnityEngine.UI.Image>();
                buttonImage.color = new Color(0.35f, 0.12f, 0.075f, 1f);
                var button = buttonObject.GetComponent<UnityEngine.UI.Button>();
                button.targetGraphic = buttonImage;
                button.onClick.AddListener(LeaveAfterDefeat);

                var buttonLabelObject = new GameObject("Label", typeof(RectTransform), typeof(CanvasRenderer), typeof(TextMeshProUGUI));
                buttonLabelObject.transform.SetParent(buttonObject.transform, false);
                var buttonLabelRect = buttonLabelObject.GetComponent<RectTransform>();
                buttonLabelRect.anchorMin = Vector2.zero;
                buttonLabelRect.anchorMax = Vector2.one;
                buttonLabelRect.offsetMin = buttonLabelRect.offsetMax = Vector2.zero;
                var buttonLabel = buttonLabelObject.GetComponent<TextMeshProUGUI>();
                buttonLabel.font = aimText.font;
                buttonLabel.fontSize = 20;
                buttonLabel.fontStyle = FontStyles.Bold;
                buttonLabel.color = new Color(1f, 0.9f, 0.69f);
                buttonLabel.alignment = TextAlignmentOptions.Center;
                buttonLabel.raycastTarget = false;
                buttonLabel.text = "LOG OUT";
                var respawnObject = Instantiate(buttonObject, cardObject.transform);
                respawnObject.name = "Return to Town";
                respawnObject.GetComponent<RectTransform>().anchoredPosition = new Vector2(-88, -68);
                respawnObject.GetComponent<UnityEngine.UI.Image>().color = new Color(.13f, .32f, .27f, 1f);
                respawnButton = respawnObject.GetComponent<UnityEngine.UI.Button>();
                respawnButton.onClick.RemoveAllListeners();
                respawnButton.onClick.AddListener(ReturnToTown);
                var respawnLabel = respawnObject.GetComponentInChildren<TMP_Text>();
                respawnLabel.fontSize = 16;
                respawnLabel.text = "RETURN TO TOWN";
            }

            defeatPanel.SetActive(true);
            defeatPanel.transform.SetAsLastSibling();
            if (respawnButton != null) respawnButton.interactable = Time.unscaledTimeAsDouble >= respawnDeadline;
        }

        private void HideDefeatPanel()
        {
            if (defeatPanel != null) defeatPanel.SetActive(false);
            respawnDeadline = 0;
            if (defeatMessage != null) defeatMessage.text = "Your character has fallen.\nReturn to town to recover, or log out.";
        }

        private void ReturnToTown()
        {
            if (!WorldAvailable || model == null || worldScreen.OwnFull.hp != 0 ||
                Time.unscaledTimeAsDouble < respawnDeadline) return;
            if (!model.RequestRespawn())
            {
                if (defeatMessage != null) defeatMessage.text = "Cannot request town recovery yet.\nTry again or log out.";
                return;
            }
            respawnDeadline = Time.unscaledTimeAsDouble + 8;
            respawnButton.interactable = false;
            defeatMessage.text = "Returning to town…\nWaiting for the server.";
        }
        private void LeaveAfterDefeat() => loginScreen.ConnectionFailed("Session closed.");

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
            if (respawnDeadline > 0 && Time.unscaledTimeAsDouble >= respawnDeadline &&
                defeatPanel != null && defeatPanel.activeSelf)
            {
                respawnDeadline = 0;
                if (respawnButton != null) respawnButton.interactable = WorldAvailable && worldScreen.OwnFull.hp == 0;
                if (defeatMessage != null) defeatMessage.text = "No recovery response yet.\nTry again or log out.";
            }
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
                if (worldScreen.OwnFull.hp > 0)
                    worldScreen.ActorMotion?.PlayAttack(result.dir, result.skillPhase != 0);
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
            HideDefeatPanel();
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
            if (defeatPanel != null)
            {
                if (Application.isPlaying) Destroy(defeatPanel);
                else DestroyImmediate(defeatPanel);
                defeatPanel = null;
            }
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
