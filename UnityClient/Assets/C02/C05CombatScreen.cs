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
        [SerializeField] private Material skillEffectMaterial;
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
        private Material previewMaterial;
        private float linePreviewDeadline;
        private GameObject releaseEffectRoot;
        private MeshRenderer releaseEffectRenderer;
        private Mesh releaseEffectMesh;
        private ParticleSystem releaseSparks;
        private Material releaseEffectMaterial;
        private Material sparkMaterial;
        private readonly Vector3[] releaseBaseVertices = new Vector3[512];
        private readonly Vector3[] releaseVertices = new Vector3[512];
        private readonly float[] releaseVertexRise = new float[512];
        private readonly Color[] releaseColors = new Color[512];
        private readonly int[] releaseTriangles = new int[2048];
        private int releaseVertexCount;
        private int releaseIndexCount;
        private Vector3 releaseEffectOrigin;
        private float releaseEffectDeadline;
        private byte releaseEffectSlot;
        private readonly float[] cooldownDeadlines = new float[3];
        private readonly GameObject[] cooldownOverlays = new GameObject[3];
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
        private ulong observedCharacterId;
        private const float LineRangeLength = 12f;
        private const float LineRangeWidth = 3f;
        private const float LinePreviewLifetime = 2f;
        private const float ServerTickSeconds = 0.05f;
        private const float ReleaseEffectLifetime = 0.35f;
        private const int EffectArcSegments = 36;
        private const float EffectGroundHeight = 0.12f;

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
            EnsureCooldownOverlays();
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
            HideReleaseEffect();
            HideSkillTooltip();
            HideDefeatPanel();
            Unsubscribe();
            generation++;
            model = next;
            observedZone = 0;
            ownId = 0;
            observedCharacterId = 0;
            Array.Clear(cooldownDeadlines, 0, cooldownDeadlines.Length);
            UpdateCooldownOverlays();
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
                if (observedZone == 0)
                {
                    observedZone = worldScreen.ZoneID;
                    ownId = worldScreen.InternalID;
                    observedCharacterId = worldScreen.SelectedCharacterID;
                }
                else if (observedZone != worldScreen.ZoneID || ownId != worldScreen.InternalID ||
                    observedCharacterId != worldScreen.SelectedCharacterID)
                {
                    if (observedCharacterId != worldScreen.SelectedCharacterID)
                        Array.Clear(cooldownDeadlines, 0, cooldownDeadlines.Length);
                    ResetObservation();
                }
                var state = worldScreen.OwnFull;
                if (state.hp == 0)
                {
                    HideLinePreview();
                    HideReleaseEffect();
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
                HideReleaseEffect();
                HideSkillTooltip();
                HideDefeatPanel();
                if (worldScreen.Stage != C02Stage.TransitionPending && worldScreen.Stage != C02Stage.WaitingForZone &&
                    worldScreen.Stage != C02Stage.WaitingForFull)
                {
                    Array.Clear(cooldownDeadlines, 0, cooldownDeadlines.Length);
                    observedCharacterId = 0;
                }
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
            UpdateCooldownOverlays();
        }

        private void ResetObservation()
        {
            HideLinePreview();
            HideReleaseEffect();
            HideSkillTooltip();
            HideDefeatPanel();
            Unsubscribe();
            generation++;
            observedZone = WorldAvailable ? worldScreen.ZoneID : (ushort)0;
            ownId = WorldAvailable ? worldScreen.InternalID : 0;
            observedCharacterId = WorldAvailable ? worldScreen.SelectedCharacterID : 0;
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
            UpdateReleaseEffect();
            UpdateSkillTooltip();
            UpdateCooldownOverlays();
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
                HideReleaseEffect();
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
                float chargeDuration = 0;
                if (result.skillPhase == 0 && TryGetSkill(result.skillSlot, out var castingSkill) &&
                    castingSkill.phases != null && castingSkill.phases.Length > 1 && castingSkill.phases[1] != null)
                    chargeDuration = Mathf.Min(castingSkill.phases[1].waitTicks * ServerTickSeconds, 2f);
                if (worldScreen.OwnFull.hp > 0)
                    worldScreen.ActorMotion?.PlayAttack(result.dir, result.skillPhase != 0, result.skillSlot, chargeDuration);
                combatStatusText.text = SkillName(result.skillSlot) + (result.skillPhase == 0 ? " cast" : " released");
                if (result.skillPhase == 0)
                {
                    if (TryGetSkill(result.skillSlot, out var skill) && skill.cooldownTicks > 0)
                        cooldownDeadlines[result.skillSlot] = Time.unscaledTime + skill.cooldownTicks * ServerTickSeconds;
                    if (result.skillSlot == 2) ShowLinePreview(result);
                }
                else
                {
                    if (result.skillSlot == 2) HideLinePreview();
                    ShowReleaseEffect(result);
                }
            }
            UpdateCooldownOverlays();
        }

        private void ShowLinePreview(ActionResultField result)
        {
            if (linePreview == null)
            {
                previewMaterial = CreateSkillMaterial();
                if (previewMaterial == null) return;
                var previewObject = new GameObject("C05 Line Skill Range");
                previewObject.transform.SetParent(worldScreen.transform, false);
                linePreview = previewObject.AddComponent<LineRenderer>();
                linePreview.sharedMaterial = previewMaterial;
                linePreview.useWorldSpace = true;
                linePreview.loop = true;
                linePreview.positionCount = 4;
            }

            linePreview.widthMultiplier = 0.12f;
            linePreview.positionCount = 4;
            linePreview.startColor = linePreview.endColor = new Color(1f, 0.72f, 0.16f, 0.92f);

            // ActionResult has no dimensions; this outline mirrors the fixed server skill 2 contract (12 x 3).
            // Points use server x/y as global X/Z, so the C02Client parent transform does not offset the range.
            for (int point = 0; point < 4; point++)
                linePreview.SetPosition(point, RectangleCorner(result, point, 0.05f, LineRangeLength, LineRangeWidth));
            linePreview.enabled = true;
            linePreviewDeadline = Time.unscaledTime + LinePreviewLifetime;
        }

        private void ShowReleaseEffect(ActionResultField result)
        {
            if (!TryGetSkill(result.skillSlot, out var skill) || skill.phases == null || result.skillPhase >= skill.phases.Length) return;
            var phase = skill.phases[result.skillPhase];
            if (phase == null || phase.action != C14SkillAction.Hit ||
                phase.range != C14SkillRange.Circle && phase.range != C14SkillRange.Rectangle ||
                !Finite(phase.range1) || phase.range1 <= 0 ||
                phase.range == C14SkillRange.Rectangle && (!Finite(phase.range2) || phase.range2 <= 0)) return;
            if (!EnsureReleaseEffect()) return;
            releaseEffectOrigin = new Vector3(result.x, 0.07f, result.y);
            releaseEffectSlot = result.skillSlot;
            Vector3 forward = C02WorldScreen.ServerFacing(result.dir) * Vector3.back;
            Vector3 right = Vector3.Cross(Vector3.up, forward);
            releaseVertexCount = releaseIndexCount = 0;
            if (result.skillSlot == 0)
            {
                // The crescent hangs around the knight's waist and stays narrow like the blade path.
                AddArcRibbon(forward, right, -62f, 62f, phase.range1 * .84f, phase.range1,
                    22, .84f, .18f, .98f);
                AddArcRibbon(forward, right, -48f, 48f, phase.range1 * .91f, phase.range1 * .97f,
                    18, .92f, .08f, 1.02f);
            }
            else if (result.skillSlot == 1)
            {
                // A raised, returning sweep leaves a broad airy trace around the spinning blade.
                AddArcRibbon(forward, right, -138f, 138f, phase.range1 * .68f, phase.range1 * .96f,
                    EffectArcSegments, .76f, .48f, .7f);
                AddArcRibbon(forward, right, -112f, 112f, phase.range1 * .84f, phase.range1 * .91f,
                    30, .9f, .2f, .76f);
            }
            else
            {
                // A slender descending blade sweep leads the force along narrow, broken ground seams.
                float length = phase.range1;
                float width = Mathf.Min(phase.range2 * .12f, .36f);
                AddDirectionalRibbon(forward, right, .2f, length, width, 0f, .12f, 24, .48f);
                AddGroundFracture(forward, right, length * .3f, length, -phase.range2 * .25f, .11f, 10, .88f, 0);
                AddGroundFracture(forward, right, length * .38f, length * .96f, phase.range2 * .25f, .09f, 10, .82f, 1);
                AddGroundFracture(forward, right, length * .52f, length * .88f, 0f, .055f, 8, .64f, 2);
                float slashSpan = Mathf.Min(phase.range2 * .46f, 1.05f);
                AddDescendingBladeSlash(forward, right, length * .27f, slashSpan, .17f, .68f, 0);
                AddDescendingBladeSlash(forward, right, length * .27f, slashSpan, .052f, .98f, 1);
            }
            releaseEffectMesh.Clear();
            releaseEffectMesh.SetVertices(releaseBaseVertices, 0, releaseVertexCount);
            releaseEffectMesh.SetColors(releaseColors, 0, releaseVertexCount);
            releaseEffectMesh.SetTriangles(releaseTriangles, 0, releaseIndexCount, 0, false);
            float extent = Mathf.Max(phase.range1 * 1.2f, 2f);
            releaseEffectMesh.bounds = new Bounds(releaseEffectOrigin + Vector3.up * 1.3f,
                new Vector3(extent * 2f, 5.6f, extent * 2f));
            releaseEffectMaterial.color = Color.white;
            releaseEffectRenderer.enabled = true;
            releaseEffectDeadline = Time.unscaledTime + ReleaseEffectLifetime;
            EmitReleaseSparks(result, forward, right, phase.range1);
            UpdateReleaseEffect();
        }

        private bool EnsureReleaseEffect()
        {
            if (releaseEffectRoot != null) return true;
            releaseEffectMaterial = CreateSkillMaterial();
            if (releaseEffectMaterial == null) return false;
            releaseEffectRoot = new GameObject("C05 Knight Skill Effect");
            releaseEffectRoot.transform.SetPositionAndRotation(Vector3.zero, Quaternion.identity);
            releaseEffectMesh = new Mesh { name = "C05 Knight Skill Mesh" };
            releaseEffectMesh.MarkDynamic();
            releaseEffectRoot.AddComponent<MeshFilter>().sharedMesh = releaseEffectMesh;
            releaseEffectRenderer = releaseEffectRoot.AddComponent<MeshRenderer>();
            releaseEffectRenderer.sharedMaterial = releaseEffectMaterial;
            releaseEffectRenderer.shadowCastingMode = UnityEngine.Rendering.ShadowCastingMode.Off;
            releaseEffectRenderer.receiveShadows = false;

            var sparkObject = new GameObject("C05 Sword Sparks");
            sparkObject.transform.SetParent(releaseEffectRoot.transform, false);
            releaseSparks = sparkObject.AddComponent<ParticleSystem>();
            releaseSparks.Stop(true, ParticleSystemStopBehavior.StopEmittingAndClear);
            var main = releaseSparks.main;
            main.duration = 1f;
            main.loop = false;
            main.playOnAwake = false;
            main.simulationSpace = ParticleSystemSimulationSpace.World;
            main.startLifetime = new ParticleSystem.MinMaxCurve(.24f, .42f);
            main.startSpeed = 0;
            main.startSize = new ParticleSystem.MinMaxCurve(.045f, .105f);
            main.startColor = new Color(1f, .67f, .22f, .98f);
            main.maxParticles = 64;
            main.gravityModifier = .7f;
            var emission = releaseSparks.emission;
            emission.enabled = false;
            var shape = releaseSparks.shape;
            shape.enabled = false;
            var particleRenderer = releaseSparks.GetComponent<ParticleSystemRenderer>();
            sparkMaterial = CreateSkillMaterial();
            if (sparkMaterial == null)
            {
                DestroyEffectObjects();
                return false;
            }
            particleRenderer.sharedMaterial = sparkMaterial;
            particleRenderer.renderMode = ParticleSystemRenderMode.Billboard;
            particleRenderer.shadowCastingMode = UnityEngine.Rendering.ShadowCastingMode.Off;
            particleRenderer.receiveShadows = false;
            return true;
        }

        private Material CreateSkillMaterial()
        {
            if (skillEffectMaterial != null) return new Material(skillEffectMaterial);
            // Test fixtures lack the serialized build-safe asset; the live scene is wired to it.
            var shader = Shader.Find("Universal Render Pipeline/Unlit");
            return shader == null ? null : new Material(shader);
        }

        private void AddArcRibbon(Vector3 forward, Vector3 right, float startDegrees, float endDegrees,
            float innerRadius, float outerRadius, int segments, float alpha, float rise, float lift)
        {
            for (int i = 0; i <= segments; i++)
            {
                float t = (float)i / segments;
                float angle = Mathf.Lerp(startDegrees, endDegrees, t) * Mathf.Deg2Rad;
                Vector3 radial = forward * Mathf.Cos(angle) + right * Mathf.Sin(angle);
                float taper = Mathf.Sin(t * Mathf.PI);
                int first = releaseVertexCount;
                float verticalOffset = EffectGroundHeight + lift;
                WriteVertex(releaseEffectOrigin + radial * innerRadius + Vector3.up * verticalOffset,
                    rise, new Color(1f, .96f, .72f, alpha * (.35f + taper * .65f)));
                WriteVertex(releaseEffectOrigin + radial * outerRadius + Vector3.up * verticalOffset,
                    rise * .35f, new Color(1f, .48f, .12f, alpha * (.15f + taper * .85f)));
                if (i == 0) continue;
                int previous = first - 2;
                AddTriangle(previous, previous + 1, first);
                AddTriangle(first, previous + 1, first + 1);
            }
        }

        private void AddGroundFracture(Vector3 forward, Vector3 right, float start, float end,
            float sideOffset, float width, int segments, float alpha, int seed)
        {
            for (int i = 0; i <= segments; i++)
            {
                float t = (float)i / segments;
                float taper = Mathf.Sin(t * Mathf.PI);
                float zigzag = ((i + seed) % 2 == 0 ? -1f : 1f) * width * .72f;
                float distance = Mathf.Lerp(start, end, t);
                Vector3 center = releaseEffectOrigin + forward * distance +
                    right * (sideOffset + zigzag) + Vector3.up * (EffectGroundHeight + .012f);
                float halfWidth = width * (.12f + taper * .6f);
                int first = releaseVertexCount;
                WriteVertex(center - right * halfWidth, .035f, new Color(1f, .36f, .08f, alpha * (.2f + taper * .72f)));
                WriteVertex(center + right * halfWidth, .025f, new Color(1f, .86f, .4f, alpha * (.14f + taper * .68f)));
                if (i == 0) continue;
                int previous = first - 2;
                AddTriangle(previous, previous + 1, first);
                AddTriangle(first, previous + 1, first + 1);
            }
        }

        private void AddDescendingBladeSlash(Vector3 forward, Vector3 right, float distance,
            float halfSpan, float halfThickness, float alpha, int seed)
        {
            const int segments = 20;
            for (int i = 0; i <= segments; i++)
            {
                float t = (float)i / segments;
                float side = Mathf.Lerp(-halfSpan, halfSpan, t);
                float y = .34f + 2.15f * (1f - t) + .2f * Mathf.Sin(t * Mathf.PI);
                float forwardCurve = .12f + .58f * Mathf.Sin(t * Mathf.PI);
                Vector3 center = releaseEffectOrigin + forward * (distance + forwardCurve) +
                    right * side + Vector3.up * y;
                float slope = -2.15f / (halfSpan * 2f) +
                    .2f * Mathf.PI * Mathf.Cos(t * Mathf.PI) / (halfSpan * 2f);
                Vector3 normal = (Vector3.up - right * slope).normalized;
                int first = releaseVertexCount;
                float edgeFade = Mathf.Sin(t * Mathf.PI);
                float stagger = seed == 0 ? 0f : .012f;
                WriteVertex(center - normal * (halfThickness + stagger), .55f * (1f - t),
                    new Color(1f, .45f, .12f, alpha * (.25f + edgeFade * .55f)));
                WriteVertex(center + normal * (halfThickness + stagger), .72f * (1f - t),
                    new Color(1f, .98f, .78f, alpha * (.35f + edgeFade * .62f)));
                if (i == 0) continue;
                int previous = first - 2;
                AddTriangle(previous, previous + 1, first);
                AddTriangle(first, previous + 1, first + 1);
            }
        }

        private void AddDirectionalRibbon(Vector3 forward, Vector3 right, float start, float end, float width,
            float sideOffset, float rise, int segments, float alpha)
        {
            for (int i = 0; i <= segments; i++)
            {
                float t = (float)i / segments;
                float taper = Mathf.Sin(t * Mathf.PI);
                float distance = Mathf.Lerp(start, end, t);
                float curve = Mathf.Sin(t * Mathf.PI) * sideOffset * .12f;
                Vector3 center = releaseEffectOrigin + forward * distance + right * (sideOffset + curve) +
                    Vector3.up * EffectGroundHeight;
                float halfWidth = width * (.12f + taper * .88f);
                int first = releaseVertexCount;
                WriteVertex(center - right * halfWidth, rise * (1f - t),
                    new Color(1f, .53f, .16f, alpha * (.12f + taper * .72f)));
                WriteVertex(center + right * halfWidth, rise * (1f - t),
                    new Color(1f, .96f, .76f, alpha * (.2f + taper * .8f)));
                if (i == 0) continue;
                int previous = first - 2;
                AddTriangle(previous, previous + 1, first);
                AddTriangle(first, previous + 1, first + 1);
            }
        }

        private void WriteVertex(Vector3 position, float rise, Color color)
        {
            int index = releaseVertexCount++;
            releaseBaseVertices[index] = position;
            releaseVertices[index] = position;
            releaseVertexRise[index] = rise;
            releaseColors[index] = color;
        }

        private void AddTriangle(int a, int b, int c)
        {
            releaseTriangles[releaseIndexCount++] = a;
            releaseTriangles[releaseIndexCount++] = b;
            releaseTriangles[releaseIndexCount++] = c;
        }

        private void EmitReleaseSparks(ActionResultField result, Vector3 forward, Vector3 right, float range)
        {
            releaseSparks.Stop(true, ParticleSystemStopBehavior.StopEmittingAndClear);
            for (int i = 0; i < 22; i++)
            {
                float t = (float)i / 22f;
                Vector3 position;
                Vector3 velocity;
                if (result.skillSlot == 1)
                {
                    float angle = t * Mathf.PI * 2f;
                    Vector3 radial = forward * Mathf.Cos(angle) + right * Mathf.Sin(angle);
                    position = releaseEffectOrigin + radial * (range * .7f) + Vector3.up * .2f;
                    velocity = radial * (2.3f + (i % 4) * .35f) + Vector3.up * (.8f + (i % 3) * .25f);
                }
                else if (result.skillSlot == 2)
                {
                    position = releaseEffectOrigin + forward * (range * (.2f + .72f * t)) +
                        right * (Mathf.Sin(i * 2.1f) * .65f) + Vector3.up * (.24f + (i % 3) * .08f);
                    velocity = forward * (3.2f + (i % 5) * .35f) + right * (Mathf.Sin(i * 1.7f) * 1.3f) +
                        Vector3.up * (1.8f + (i % 4) * .4f);
                }
                else
                {
                    float angle = Mathf.Lerp(-1.05f, 1.05f, t);
                    Vector3 radial = forward * Mathf.Cos(angle) + right * Mathf.Sin(angle);
                    position = releaseEffectOrigin + radial * range * .78f + Vector3.up * .24f;
                    velocity = radial * (2f + (i % 4) * .3f) + Vector3.up * (.9f + (i % 4) * .28f);
                }
                var particle = new ParticleSystem.EmitParams
                {
                    position = position,
                    velocity = velocity,
                    startColor = i % 5 == 0 ? new Color(1f, .96f, .78f, 1f) : new Color(1f, .48f, .12f, .96f),
                    startSize = .055f + (i % 3) * .018f,
                    startLifetime = .22f + (i % 4) * .045f
                };
                releaseSparks.Emit(particle, 1);
            }
        }

        // Cast preview and hit pulse share the same server direction and winding.
        private static Vector3 RectangleCorner(ActionResultField result, int point, float height, float length, float width)
        {
            float halfWidth = width * 0.5f;
            float side = point == 0 || point == 3 ? -halfWidth : halfWidth;
            float reach = point < 2 ? 0 : length;
            if (result.dir == 1 || result.dir == 2) reach = -reach;
            return result.dir < 2
                ? new Vector3(result.x + side, height, result.y + reach)
                : new Vector3(result.x + reach, height, result.y + side);
        }

        private void UpdateReleaseEffect()
        {
            if (releaseEffectRenderer == null || !releaseEffectRenderer.enabled) return;
            float remaining = releaseEffectDeadline - Time.unscaledTime;
            if (remaining <= 0)
            {
                HideReleaseEffect();
                return;
            }
            float progress = Mathf.Clamp01(1f - remaining / ReleaseEffectLifetime);
            float scale = Mathf.Lerp(.18f, 1.08f, progress);
            float fall = releaseEffectSlot == 2 ? 1f - progress : 0;
            for (int i = 0; i < releaseVertexCount; i++)
            {
                Vector3 offset = releaseBaseVertices[i] - releaseEffectOrigin;
                releaseVertices[i] = releaseEffectOrigin + new Vector3(offset.x * scale,
                    offset.y + releaseVertexRise[i] * fall, offset.z * scale);
            }
            releaseEffectMesh.SetVertices(releaseVertices, 0, releaseVertexCount,
                UnityEngine.Rendering.MeshUpdateFlags.DontRecalculateBounds);
            releaseEffectMaterial.color = new Color(1f, 1f, 1f, Mathf.Lerp(1f, .28f, progress));
        }

        private void HideReleaseEffect()
        {
            if (releaseEffectRenderer != null) releaseEffectRenderer.enabled = false;
            if (releaseSparks != null) releaseSparks.Stop(true, ParticleSystemStopBehavior.StopEmittingAndClear);
            releaseEffectDeadline = 0;
        }

        private void DestroyEffectObjects()
        {
            Mesh mesh = releaseEffectMesh;
            if (releaseEffectRoot != null)
            {
                if (Application.isPlaying) Destroy(releaseEffectRoot);
                else DestroyImmediate(releaseEffectRoot);
            }
            if (mesh != null)
            {
                if (Application.isPlaying) Destroy(mesh);
                else DestroyImmediate(mesh);
            }
            releaseEffectRoot = null;
            releaseEffectRenderer = null;
            releaseEffectMesh = null;
            releaseSparks = null;
            DestroyMaterial(releaseEffectMaterial);
            DestroyMaterial(sparkMaterial);
            releaseEffectMaterial = null;
            sparkMaterial = null;
        }

        private void DestroyMaterial(Material material)
        {
            if (material == null) return;
            if (Application.isPlaying) Destroy(material);
            else DestroyImmediate(material);
        }

        private bool TryGetSkill(int slot, out C14ServerContent.RingSkill skill)
        {
            skill = slot == 1 ? serverContent?.ring : serverContent?.otherSkills == null ? null :
                Array.Find(serverContent.otherSkills, candidate => candidate != null && candidate.skillID == slot);
            return skill != null && skill.skillID == slot;
        }

        private void EnsureCooldownOverlays()
        {
            for (int i = 0; i < skillButtons.Length && i < cooldownOverlays.Length; i++)
            {
                var icon = skillButtons[i].transform.Find("SkillIcon");
                if (icon == null) continue;
                var overlay = icon.Find("C05CooldownOverlay");
                if (overlay == null)
                {
                    var overlayObject = new GameObject("C05CooldownOverlay", typeof(RectTransform), typeof(CanvasRenderer), typeof(Image));
                    overlayObject.transform.SetParent(icon, false);
                    var rect = overlayObject.GetComponent<RectTransform>();
                    rect.anchorMin = Vector2.zero;
                    rect.anchorMax = Vector2.one;
                    rect.offsetMin = Vector2.zero;
                    rect.offsetMax = Vector2.zero;
                    overlay = overlayObject.transform;
                }
                cooldownOverlays[i] = overlay.gameObject;
                var mask = overlay.GetComponent<Image>();
                var iconImage = icon.GetComponent<Image>();
                mask.sprite = iconImage != null ? iconImage.sprite : null;
                mask.type = Image.Type.Filled;
                mask.fillMethod = Image.FillMethod.Radial360;
                mask.fillOrigin = (int)Image.Origin360.Top;
                mask.fillClockwise = false;
                mask.color = new Color(0f, 0f, 0f, 0.62f);
                mask.raycastTarget = false;
                mask.enabled = mask.sprite != null;

                var textTransform = overlay.Find("Time");
                TMP_Text text;
                if (textTransform == null)
                {
                    var textObject = new GameObject("Time", typeof(RectTransform), typeof(CanvasRenderer), typeof(TextMeshProUGUI));
                    textObject.transform.SetParent(overlay, false);
                    textTransform = textObject.transform;
                }
                text = textTransform.GetComponent<TMP_Text>();
                var name = skillButtons[i].transform.Find("SkillName")?.GetComponent<TMP_Text>();
                if (name == null) name = skillButtons[i].GetComponentInChildren<TMP_Text>();
                if (name != null) text.font = name.font;
                text.fontSize = 14;
                text.enableAutoSizing = false;
                text.outlineColor = new Color32(0, 0, 0, 230);
                text.outlineWidth = .15f;
                text.color = new Color(1f, 0.96f, 0.72f, 1f);
                text.alignment = TextAlignmentOptions.Center;
                text.enableWordWrapping = false;
                text.raycastTarget = false;
                var textRect = text.rectTransform;
                textRect.anchorMin = Vector2.zero;
                textRect.anchorMax = new Vector2(1, .45f);
                textRect.offsetMin = Vector2.zero;
                textRect.offsetMax = Vector2.zero;
                text.text = string.Empty;
                cooldownOverlays[i].SetActive(false);
            }
        }

        private void UpdateCooldownOverlays()
        {
            for (int i = 0; i < cooldownOverlays.Length; i++)
            {
                var overlay = cooldownOverlays[i];
                if (overlay == null) continue;
                float remaining = cooldownDeadlines[i] - Time.unscaledTime;
                if (!WorldAvailable || remaining <= 0 || !TryGetSkill(i, out var skill) || skill.cooldownTicks == 0)
                {
                    overlay.SetActive(false);
                    continue;
                }
                var mask = overlay.GetComponent<Image>();
                mask.enabled = mask.sprite != null;
                mask.fillAmount = Mathf.Clamp01(remaining / (skill.cooldownTicks * ServerTickSeconds));
                var text = overlay.transform.Find("Time").GetComponent<TMP_Text>();
                text.text = string.Format(CultureInfo.InvariantCulture, remaining >= 10 ? "~{0:0}s" : "~{0:0.0}s", remaining);
                overlay.SetActive(true);
            }
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
            HideReleaseEffect();
            HideSkillTooltip();
            HideDefeatPanel();
            Array.Clear(cooldownDeadlines, 0, cooldownDeadlines.Length);
            UpdateCooldownOverlays();
            if (combatPanel != null) combatPanel.SetActive(false);
            loginScreen?.Data?.CancelAction();
            worldScreen?.CancelMovement();
            Unsubscribe();
            generation++;
            model = null;
            observedZone = 0;
            ownId = 0;
            observedCharacterId = 0;
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
            HideReleaseEffect();
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
            DestroyMaterial(previewMaterial);
            previewMaterial = null;
            DestroyEffectObjects();
        }
    }
}
