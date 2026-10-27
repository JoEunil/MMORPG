using Mmorpg.Client;
using TMPro;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

namespace Mmorpg.Client.Editor
{
    public static class HudSetup
    {
        [MenuItem("Tools/Client/HUD/Apply Compact Combat HUD")]
        public static void Apply()
        {
            var scene = EditorSceneManager.GetActiveScene();
            if (EditorApplication.isPlayingOrWillChangePlaymode || EditorApplication.isCompiling || scene.isDirty)
                throw new System.InvalidOperationException("Stop Play, wait for compilation, and save the current scene before applying C12 HUD.");
            if (!scene.IsValid() || scene.path != "Assets/Client/Scenes/World.unity")
                throw new System.InvalidOperationException("C12 requires the saved C02 scene.");
            var fillSprite = AssetDatabase.LoadAssetAtPath<Sprite>("Assets/Client/Art/CitadelEnvironment/vitals-track-face-v1.png");
            if (fillSprite == null) throw new System.InvalidOperationException("Import the authored HP/MP track face as a Sprite before applying the HUD.");
            var icons = new[] {
                AssetDatabase.LoadAssetAtPath<Sprite>("Assets/Client/Art/Skills/skill-strike-medallion-v3.png"),
                AssetDatabase.LoadAssetAtPath<Sprite>("Assets/Client/Art/Skills/skill-ring-medallion-v3.png"),
                AssetDatabase.LoadAssetAtPath<Sprite>("Assets/Client/Art/Skills/skill-line-medallion-v3.png")
            };
            if (System.Array.Exists(icons, icon => icon == null))
                throw new System.InvalidOperationException("Import all three skill icons as Sprites before applying the HUD.");

            var screen = Object.FindFirstObjectByType<WorldScreen>(FindObjectsInactive.Include);
            var login = Object.FindFirstObjectByType<LoginScreen>(FindObjectsInactive.Include);
            var serverContent = AssetDatabase.LoadAssetAtPath<ServerContent>("Assets/Client/Content/ServerContent.asset");
            if (screen == null || login == null) throw new System.InvalidOperationException("The existing C02 world and login screens are required.");
            if (serverContent == null) throw new System.InvalidOperationException("The existing C14 server skill content is required.");
            var screenRefs = new SerializedObject(screen);
            var hudReference = screenRefs.FindProperty("worldHud");
            if (hudReference == null || hudReference.objectReferenceValue == null)
                throw new System.InvalidOperationException("The existing WorldHud reference is missing.");
            var hud = ((GameObject)hudReference.objectReferenceValue).transform;
            var panel = (hud.Find("C05CombatPanel") ?? hud.parent.Find("C05CombatPanel")) as RectTransform;
            if (panel == null) throw new System.InvalidOperationException("The existing C05CombatPanel is missing.");
            var vitals = ExistingText(panel, "C05Vitals");
            var aim = ExistingText(panel, "C05Aim");
            var status = ExistingText(panel, "C05Status");
            var combat = screen.GetComponent<CombatScreen>();
            if (combat == null) throw new System.InvalidOperationException("The existing C05CombatScreen is missing.");
            var buttons = new UnityEngine.UI.Button[3];
            for (int i = 0; i < buttons.Length; i++)
            {
                var child = panel.Find("C05Skill" + i);
                buttons[i] = child != null ? child.GetComponent<UnityEngine.UI.Button>() : null;
                if (buttons[i] == null || buttons[i].GetComponentInChildren<TMP_Text>() == null)
                    throw new System.InvalidOperationException("All three existing C05 skill buttons and labels are required.");
            }

            panel.SetParent(hud.parent, false);
            var scaler = panel.GetComponentInParent<Canvas>().GetComponent<UnityEngine.UI.CanvasScaler>();
            if (scaler != null) scaler.screenMatchMode = UnityEngine.UI.CanvasScaler.ScreenMatchMode.Expand;
            panel.anchorMin = panel.anchorMax = panel.pivot = new Vector2(.5f, 0);
            panel.anchoredPosition = new Vector2(0, 12);
            panel.sizeDelta = new Vector2(232, 200);
            var background = panel.GetComponent<UnityEngine.UI.Image>();
            background.color = new Color(.105f, .12f, .13f, .94f);
            background.raycastTarget = true;

            vitals.gameObject.SetActive(false);
            PlaceText(aim, 10, new Vector2(12, -137), new Vector2(208, 18));
            PlaceText(status, 10, new Vector2(12, -177), new Vector2(208, 18));
            vitals.enableAutoSizing = true;
            vitals.fontSizeMin = 9;
            vitals.fontSizeMax = 12;
            vitals.alignment = TextAlignmentOptions.Left;
            aim.alignment = TextAlignmentOptions.Left;
            status.alignment = TextAlignmentOptions.Left;
            status.enableWordWrapping = false;

            var hpFill = Gauge(panel, "C12HpTrack", new Vector2(12, -8), new Vector2(208, 22),
                new Color(.19f, .11f, .1f, .96f), new Color(.72f, .2f, .16f, 1), fillSprite);
            var mpFill = Gauge(panel, "C12MpTrack", new Vector2(12, -34), new Vector2(208, 22),
                new Color(.14f, .17f, .22f, .96f), new Color(.3f, .62f, .95f, 1), fillSprite);
            var hpValue = GaugeValue(panel, "C12HpTrack", "C12HpValue");
            var mpValue = GaugeValue(panel, "C12MpTrack", "C12MpValue");

            var labels = new[] { "STRIKE", "RING", "LINE" };
            var keys = new[] { "Z", "X", "C" };
            for (int i = 0; i < buttons.Length; i++)
            {
                var rect = buttons[i].GetComponent<RectTransform>();
                rect.anchorMin = rect.anchorMax = rect.pivot = new Vector2(0, 1);
                rect.anchoredPosition = new Vector2(20 + i * 68, -68);
                rect.sizeDelta = new Vector2(56, 62);
                var image = buttons[i].GetComponent<UnityEngine.UI.Image>();
                image.color = Color.clear;
                image.raycastTarget = true;
                var label = rect.Find("SkillName")?.GetComponent<TMP_Text>() ?? buttons[i].GetComponentInChildren<TMP_Text>();
                label.gameObject.name = "SkillName";
                label.text = labels[i];
                label.fontSize = 9;
                label.color = new Color(.96f, .87f, .65f);
                label.alignment = TextAlignmentOptions.Center;
                label.raycastTarget = false;
                label.rectTransform.anchorMin = label.rectTransform.anchorMax = label.rectTransform.pivot = new Vector2(.5f, 0);
                label.rectTransform.anchoredPosition = new Vector2(0, 1);
                label.rectTransform.sizeDelta = new Vector2(56, 13);
                var iconRect = rect.Find("SkillIcon") as RectTransform;
                if (iconRect == null)
                    iconRect = new GameObject("SkillIcon", typeof(RectTransform), typeof(UnityEngine.UI.Image)).GetComponent<RectTransform>();
                iconRect.SetParent(rect, false);
                iconRect.gameObject.SetActive(true);
                iconRect.anchorMin = iconRect.anchorMax = iconRect.pivot = new Vector2(.5f, 1);
                iconRect.anchoredPosition = Vector2.zero;
                iconRect.sizeDelta = new Vector2(56, 62);
                var icon = iconRect.GetComponent<UnityEngine.UI.Image>();
                icon.sprite = icons[i];
                icon.color = Color.white;
                icon.preserveAspect = true;
                icon.raycastTarget = false;
                buttons[i].targetGraphic = icon;

                var badge = rect.Find("SkillKeyBadge") as RectTransform;
                if (badge == null)
                    badge = new GameObject("SkillKeyBadge", typeof(RectTransform), typeof(UnityEngine.UI.Image)).GetComponent<RectTransform>();
                badge.SetParent(rect, false);
                badge.gameObject.SetActive(true);
                badge.anchorMin = badge.anchorMax = badge.pivot = new Vector2(0, 1);
                badge.anchoredPosition = new Vector2(1, -1);
                badge.sizeDelta = new Vector2(22, 22);
                var badgeImage = badge.GetComponent<UnityEngine.UI.Image>();
                badgeImage.sprite = fillSprite;
                badgeImage.color = new Color(.09f, .08f, .065f, .96f);
                badgeImage.raycastTarget = false;
                var keyLabel = GaugeValue(badge, null, "SkillKey");
                keyLabel.text = keys[i];
                keyLabel.fontSize = 22;
                keyLabel.fontStyle = FontStyles.Bold;
                keyLabel.color = new Color(1f, .89f, .59f);
                badge.SetAsLastSibling();
            }

            var combatRefs = new SerializedObject(combat);
            combatRefs.FindProperty("loginScreen").objectReferenceValue = login;
            combatRefs.FindProperty("worldScreen").objectReferenceValue = screen;
            combatRefs.FindProperty("serverContent").objectReferenceValue = serverContent;
            combatRefs.FindProperty("combatPanel").objectReferenceValue = panel.gameObject;
            var slots = combatRefs.FindProperty("skillButtons");
            slots.arraySize = buttons.Length;
            for (int i = 0; i < buttons.Length; i++) slots.GetArrayElementAtIndex(i).objectReferenceValue = buttons[i];
            combatRefs.FindProperty("vitalsText").objectReferenceValue = vitals;
            combatRefs.FindProperty("hpValueText").objectReferenceValue = hpValue;
            combatRefs.FindProperty("mpValueText").objectReferenceValue = mpValue;
            combatRefs.FindProperty("aimText").objectReferenceValue = aim;
            combatRefs.FindProperty("combatStatusText").objectReferenceValue = status;
            combatRefs.FindProperty("hpFill").objectReferenceValue = hpFill;
            combatRefs.FindProperty("mpFill").objectReferenceValue = mpFill;
            combatRefs.ApplyModifiedPropertiesWithoutUndo();
            combat.enabled = true;
            panel.gameObject.SetActive(screen.CanAct);

            EditorUtility.SetDirty(panel);
            EditorUtility.SetDirty(combat);
            EditorSceneManager.MarkSceneDirty(scene);
            if (!EditorSceneManager.SaveScene(scene)) throw new System.InvalidOperationException("C12 could not save the C02 scene.");
            AssetDatabase.SaveAssets();
        }

        private static TMP_Text ExistingText(RectTransform parent, string name)
        {
            var child = parent.Find(name);
            var text = child != null ? child.GetComponent<TMP_Text>() : null;
            if (text == null) throw new System.InvalidOperationException("The existing " + name + " label is missing.");
            return text;
        }

        private static void PlaceText(TMP_Text text, int size, Vector2 position, Vector2 dimensions)
        {
            text.fontSize = size;
            text.color = new Color(.92f, .91f, .84f);
            text.raycastTarget = false;
            var rect = text.rectTransform;
            rect.anchorMin = rect.anchorMax = rect.pivot = new Vector2(0, 1);
            rect.anchoredPosition = position;
            rect.sizeDelta = dimensions;
        }

        private static TMP_Text GaugeValue(RectTransform parent, string trackName, string name)
        {
            var valueParent = trackName == null ? parent : parent.Find(trackName) as RectTransform;
            if (valueParent == null) throw new System.InvalidOperationException("The C12 gauge track is missing.");
            var value = valueParent.Find(name)?.GetComponent<TMP_Text>();
            if (value == null)
            {
                var child = new GameObject(name, typeof(RectTransform), typeof(CanvasRenderer), typeof(TextMeshProUGUI));
                child.transform.SetParent(valueParent, false);
                value = child.GetComponent<TextMeshProUGUI>();
            }
            var rect = value.rectTransform;
            rect.anchorMin = Vector2.zero;
            rect.anchorMax = Vector2.one;
            rect.pivot = new Vector2(.5f, .5f);
            rect.offsetMin = Vector2.zero;
            rect.offsetMax = Vector2.zero;
            value.alignment = TextAlignmentOptions.Center;
            value.fontSize = 15;
            value.fontStyle = FontStyles.Bold;
            value.color = new Color(.98f, .96f, .88f);
            value.raycastTarget = false;
            value.enableWordWrapping = false;
            value.transform.SetAsLastSibling();
            return value;
        }

        private static UnityEngine.UI.Image Gauge(RectTransform parent, string name, Vector2 position,
            Vector2 size, Color trackColor, Color fillColor, Sprite sprite)
        {
            var trackTransform = parent.Find(name) as RectTransform;
            if (trackTransform == null)
                trackTransform = new GameObject(name, typeof(RectTransform), typeof(UnityEngine.UI.Image)).GetComponent<RectTransform>();
            trackTransform.SetParent(parent, false);
            trackTransform.anchorMin = trackTransform.anchorMax = trackTransform.pivot = new Vector2(0, 1);
            trackTransform.anchoredPosition = position;
            trackTransform.sizeDelta = size;
            var track = trackTransform.GetComponent<UnityEngine.UI.Image>();
            track.sprite = sprite;
            track.type = UnityEngine.UI.Image.Type.Simple;
            track.preserveAspect = false;
            track.color = trackColor;
            track.raycastTarget = false;

            const string fillName = "Fill";
            var fillTransform = trackTransform.Find(fillName) as RectTransform;
            if (fillTransform == null)
                fillTransform = new GameObject(fillName, typeof(RectTransform), typeof(UnityEngine.UI.Image)).GetComponent<RectTransform>();
            fillTransform.SetParent(trackTransform, false);
            fillTransform.anchorMin = Vector2.zero;
            fillTransform.anchorMax = Vector2.one;
            fillTransform.pivot = new Vector2(0, .5f);
            fillTransform.offsetMin = Vector2.zero;
            fillTransform.offsetMax = Vector2.zero;
            var fill = fillTransform.GetComponent<UnityEngine.UI.Image>();
            fill.sprite = sprite;
            fill.type = UnityEngine.UI.Image.Type.Filled;
            fill.fillMethod = UnityEngine.UI.Image.FillMethod.Horizontal;
            fill.fillOrigin = (int)UnityEngine.UI.Image.OriginHorizontal.Left;
            fill.preserveAspect = false;
            fill.color = fillColor;
            fill.raycastTarget = false;
            return fill;
        }
    }
}
