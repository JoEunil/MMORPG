using C01;
using C02;
using TMPro;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

public static class C12HudSetup
{
    [MenuItem("C02/C12/Apply Compact Combat HUD")]
    public static void Apply()
    {
        var scene = EditorSceneManager.GetActiveScene();
        if (EditorApplication.isPlayingOrWillChangePlaymode || EditorApplication.isCompiling || scene.isDirty)
            throw new System.InvalidOperationException("Stop Play, wait for compilation, and save the current scene before applying C12 HUD.");
        if (!scene.IsValid() || scene.path != "Assets/C02/C02.unity")
            throw new System.InvalidOperationException("C12 requires the saved C02 scene.");
        var fillSprite = AssetDatabase.GetBuiltinExtraResource<Sprite>("UI/Skin/UISprite.psd");
        if (fillSprite == null) throw new System.InvalidOperationException("The built-in uGUI sprite required by the HP/MP gauges is unavailable.");

        var screen = Object.FindFirstObjectByType<C02WorldScreen>(FindObjectsInactive.Include);
        var login = Object.FindFirstObjectByType<C01LoginScreen>(FindObjectsInactive.Include);
        if (screen == null || login == null) throw new System.InvalidOperationException("The existing C02 world and login screens are required.");
        var screenRefs = new SerializedObject(screen);
        var hudReference = screenRefs.FindProperty("worldHud");
        if (hudReference == null || hudReference.objectReferenceValue == null)
            throw new System.InvalidOperationException("The existing WorldHud reference is missing.");
        var hud = ((GameObject)hudReference.objectReferenceValue).transform;
        var panel = hud.Find("C05CombatPanel") as RectTransform;
        if (panel == null) throw new System.InvalidOperationException("The existing C05CombatPanel is missing.");
        var vitals = ExistingText(panel, "C05Vitals");
        var aim = ExistingText(panel, "C05Aim");
        var status = ExistingText(panel, "C05Status");
        var combat = screen.GetComponent<C05CombatScreen>();
        if (combat == null) throw new System.InvalidOperationException("The existing C05CombatScreen is missing.");
        var buttons = new UnityEngine.UI.Button[3];
        for (int i = 0; i < buttons.Length; i++)
        {
            var child = panel.Find("C05Skill" + i);
            buttons[i] = child != null ? child.GetComponent<UnityEngine.UI.Button>() : null;
            if (buttons[i] == null || buttons[i].GetComponentInChildren<TMP_Text>() == null)
                throw new System.InvalidOperationException("All three existing C05 skill buttons and labels are required.");
        }

        panel.anchorMin = panel.anchorMax = panel.pivot = new Vector2(0, 1);
        panel.anchoredPosition = new Vector2(0, -116);
        panel.sizeDelta = new Vector2(420, 132);
        var background = panel.GetComponent<UnityEngine.UI.Image>();
        background.color = new Color(.105f, .12f, .13f, .94f);
        background.raycastTarget = true;

        PlaceText(vitals, 13, new Vector2(12, -7), new Vector2(396, 20));
        PlaceText(aim, 11, new Vector2(12, -48), new Vector2(396, 18));
        PlaceText(status, 10, new Vector2(12, -107), new Vector2(396, 24));
        vitals.alignment = TextAlignmentOptions.Left;
        aim.alignment = TextAlignmentOptions.Left;
        status.alignment = TextAlignmentOptions.Left;
        status.enableWordWrapping = false;

        var hpFill = Gauge(panel, "C12HpTrack", new Vector2(12, -32), new Vector2(190, 9),
            new Color(.16f, .19f, .17f, .96f), new Color(.28f, .76f, .37f, 1), fillSprite);
        var mpFill = Gauge(panel, "C12MpTrack", new Vector2(218, -32), new Vector2(190, 9),
            new Color(.14f, .17f, .22f, .96f), new Color(.3f, .62f, .95f, 1), fillSprite);

        var labels = new[] { "1  STRIKE", "2  RING", "3  LINE" };
        for (int i = 0; i < buttons.Length; i++)
        {
            var rect = buttons[i].GetComponent<RectTransform>();
            rect.anchorMin = rect.anchorMax = rect.pivot = new Vector2(0, 1);
            rect.anchoredPosition = new Vector2(12 + i * 135, -70);
            rect.sizeDelta = new Vector2(126, 32);
            var image = buttons[i].GetComponent<UnityEngine.UI.Image>();
            image.color = new Color(.27f, .25f, .19f, 1);
            image.raycastTarget = true;
            var label = buttons[i].GetComponentInChildren<TMP_Text>();
            label.text = labels[i];
            label.fontSize = 12;
            label.color = new Color(.96f, .87f, .65f);
            label.alignment = TextAlignmentOptions.Center;
        }

        var combatRefs = new SerializedObject(combat);
        combatRefs.FindProperty("loginScreen").objectReferenceValue = login;
        combatRefs.FindProperty("worldScreen").objectReferenceValue = screen;
        var slots = combatRefs.FindProperty("skillButtons");
        slots.arraySize = buttons.Length;
        for (int i = 0; i < buttons.Length; i++) slots.GetArrayElementAtIndex(i).objectReferenceValue = buttons[i];
        combatRefs.FindProperty("vitalsText").objectReferenceValue = vitals;
        combatRefs.FindProperty("aimText").objectReferenceValue = aim;
        combatRefs.FindProperty("combatStatusText").objectReferenceValue = status;
        combatRefs.FindProperty("hpFill").objectReferenceValue = hpFill;
        combatRefs.FindProperty("mpFill").objectReferenceValue = mpFill;
        combatRefs.ApplyModifiedPropertiesWithoutUndo();
        combat.enabled = true;

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
        fill.fillAmount = 0;
        fill.color = fillColor;
        fill.raycastTarget = false;
        return fill;
    }
}
