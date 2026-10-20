using C01;
using C02;
using TMPro;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

public static class C02SceneBuilder
{
    [MenuItem("C02/C08/Apply Zone Transition UI")]
    public static void ApplyC08ZoneTransitionUI()
    {
        if (EditorApplication.isPlaying) throw new System.InvalidOperationException("Stop Play before applying C08 UI.");
        if (EditorSceneManager.GetActiveScene().path != "Assets/C02/C02.unity")
            throw new System.InvalidOperationException("Open the C02 scene first.");

        var canvas = Object.FindFirstObjectByType<Canvas>();
        var world = Object.FindFirstObjectByType<C02WorldScreen>();
        if (canvas == null || canvas.renderMode != RenderMode.ScreenSpaceOverlay || world == null)
            throw new System.InvalidOperationException("Expected the existing C02 overlay Canvas and C02WorldScreen.");

        var existing = canvas.transform.Find("C08ZoneTransitionButton");
        UnityEngine.UI.Button button;
        if (existing == null)
        {
            var rect = new GameObject("C08ZoneTransitionButton", typeof(RectTransform), typeof(UnityEngine.UI.Image), typeof(UnityEngine.UI.Button)).GetComponent<RectTransform>();
            rect.SetParent(canvas.transform, false);
            button = rect.GetComponent<UnityEngine.UI.Button>();
            var label = Text("Label", rect, "ENTER ZONE 2  →", 14);
            label.color = new Color(.98f, .89f, .66f);
            label.alignment = TextAlignmentOptions.Center;
            label.rectTransform.anchorMin = Vector2.zero;
            label.rectTransform.anchorMax = Vector2.one;
            label.rectTransform.offsetMin = new Vector2(8, 2);
            label.rectTransform.offsetMax = new Vector2(-8, -2);
        }
        else button = existing.GetComponent<UnityEngine.UI.Button>();

        var buttonRect = button.GetComponent<RectTransform>();
        buttonRect.anchorMin = buttonRect.anchorMax = buttonRect.pivot = new Vector2(.5f, 0);
        buttonRect.anchoredPosition = new Vector2(0, 230);
        buttonRect.sizeDelta = new Vector2(224, 40);
        var image = button.GetComponent<UnityEngine.UI.Image>();
        image.color = new Color(.48f, .34f, .15f, .98f);
        image.raycastTarget = true;
        button.targetGraphic = image;
        button.interactable = true;
        var colors = button.colors;
        colors.normalColor = new Color(.48f, .34f, .15f, .98f);
        colors.highlightedColor = new Color(.62f, .45f, .19f, 1f);
        colors.pressedColor = new Color(.34f, .23f, .11f, 1f);
        colors.selectedColor = colors.normalColor;
        button.colors = colors;
        button.gameObject.SetActive(false);

        var serialized = new SerializedObject(world);
        serialized.FindProperty("zoneTransitionButton").objectReferenceValue = button;
        serialized.ApplyModifiedPropertiesWithoutUndo();
        EditorSceneManager.MarkSceneDirty(EditorSceneManager.GetActiveScene());
        EditorSceneManager.SaveScene(EditorSceneManager.GetActiveScene());
        AssetDatabase.SaveAssets();
    }

    public static void Build()
    {
        if (EditorApplication.isPlaying) throw new System.InvalidOperationException("Stop Play before building C02.");
        C01SceneBuilder.BuildAt("Assets/C02/C02.unity");
        var login = Object.FindFirstObjectByType<C01LoginScreen>();
        var canvas = Object.FindFirstObjectByType<Canvas>();
        var panel = GameObject.Find("C01Panel");
        panel.GetComponent<RectTransform>().sizeDelta = new Vector2(1080, 650);
        var results = GameObject.Find("Results").GetComponent<RectTransform>();
        results.sizeDelta = new Vector2(534, 586);
        GameObject.Find("StageLabel").GetComponent<TMP_Text>().text = "C02 / CHARACTER ENTRY";
        GameObject.Find("Subtitle").GetComponent<TMP_Text>().text = "Sign in and select a server character.";
        GameObject.Find("CharacterListText").SetActive(false);
        SetHeight(GameObject.Find("ListTitle"), 36);
        SetHeight(GameObject.Find("StatusText"), 60);

        var choices = new GameObject("CharacterChoices", typeof(RectTransform), typeof(UnityEngine.UI.VerticalLayoutGroup));
        choices.transform.SetParent(results, false);
        SetHeight(choices, 270);
        var layout = choices.GetComponent<UnityEngine.UI.VerticalLayoutGroup>();
        layout.spacing = 3;
        layout.childControlWidth = layout.childControlHeight = true;
        layout.childForceExpandWidth = true;
        layout.childForceExpandHeight = false;
        var buttons = new UnityEngine.UI.Button[10];
        for (int i = 0; i < buttons.Length; i++)
            buttons[i] = Button("CharacterChoice" + i, choices.transform, "", 24, 14);
        var entryStatus = Text("EntryStatusText", results, "Sign in to select a character.", 17);
        SetHeight(entryStatus.gameObject, 64);
        var enter = Button("EnterButton", results, "Enter field", 44, 20);

        var world = new GameObject("C02World");
        var plane = GameObject.CreatePrimitive(PrimitiveType.Plane);
        plane.name = "FieldPlane";
        plane.transform.SetParent(world.transform, false);
        plane.transform.localScale = new Vector3(6, 1, 6);
        var own = new GameObject("OwnCharacter");
        own.transform.SetParent(world.transform, false);
        var capsule = GameObject.CreatePrimitive(PrimitiveType.Capsule);
        capsule.name = "Capsule";
        capsule.transform.SetParent(own.transform, false);
        capsule.transform.localPosition = Vector3.up;
        var camera = new GameObject("QuarterViewCamera", typeof(Camera)).GetComponent<Camera>();
        camera.transform.SetParent(world.transform, false);
        camera.orthographic = true;
        camera.orthographicSize = 11;
        camera.depth = 1;
        camera.clearFlags = CameraClearFlags.SolidColor;
        camera.backgroundColor = new Color(0.05f, 0.08f, 0.12f);
        var light = new GameObject("FieldLight", typeof(Light)).GetComponent<Light>();
        light.transform.SetParent(world.transform, false);
        light.type = LightType.Directional;
        light.intensity = 1.4f;
        light.transform.rotation = Quaternion.Euler(50, -30, 0);
        SetMaterial(plane, "FieldMaterial", new Color(0.16f, 0.25f, 0.29f));
        SetMaterial(capsule, "CharacterMaterial", new Color(0.25f, 0.85f, 0.8f));

        var hud = new GameObject("WorldHud", typeof(RectTransform), typeof(UnityEngine.UI.Image));
        hud.transform.SetParent(canvas.transform, false);
        var hudRect = hud.GetComponent<RectTransform>();
        hudRect.anchorMin = new Vector2(0, 1);
        hudRect.anchorMax = Vector2.one;
        hudRect.pivot = new Vector2(0.5f, 1);
        hudRect.sizeDelta = new Vector2(0, 160);
        hud.GetComponent<UnityEngine.UI.Image>().color = new Color(0.03f, 0.06f, 0.1f, 0.95f);
        hud.GetComponent<UnityEngine.UI.Image>().raycastTarget = false;
        var worldText = Text("WorldStatusText", hud.transform, "", 20);
        var textRect = worldText.rectTransform;
        textRect.anchorMin = Vector2.zero;
        textRect.anchorMax = Vector2.one;
        textRect.offsetMin = new Vector2(28, 15);
        textRect.offsetMax = new Vector2(-220, -15);
        var disconnect = Button("DisconnectButton", hud.transform, "Disconnect", 46, 18);
        var disconnectRect = disconnect.GetComponent<RectTransform>();
        disconnectRect.anchorMin = disconnectRect.anchorMax = new Vector2(1, 0.5f);
        disconnectRect.pivot = new Vector2(1, 0.5f);
        disconnectRect.sizeDelta = new Vector2(170, 46);
        disconnectRect.anchoredPosition = new Vector2(-28, 0);

        var screen = new GameObject("C02Client").AddComponent<C02WorldScreen>();
        var serialized = new SerializedObject(screen);
        serialized.FindProperty("loginScreen").objectReferenceValue = login;
        var array = serialized.FindProperty("characterChoices");
        array.arraySize = buttons.Length;
        for (int i = 0; i < buttons.Length; i++) array.GetArrayElementAtIndex(i).objectReferenceValue = buttons[i];
        serialized.FindProperty("enterButton").objectReferenceValue = enter;
        serialized.FindProperty("disconnectButton").objectReferenceValue = disconnect;
        serialized.FindProperty("entryStatusText").objectReferenceValue = entryStatus;
        serialized.FindProperty("worldStatusText").objectReferenceValue = worldText;
        serialized.FindProperty("selectionPanel").objectReferenceValue = panel;
        serialized.FindProperty("background").objectReferenceValue = GameObject.Find("Background");
        serialized.FindProperty("worldRoot").objectReferenceValue = world;
        serialized.FindProperty("worldHud").objectReferenceValue = hud;
        serialized.FindProperty("ownCharacter").objectReferenceValue = own.transform;
        serialized.FindProperty("fieldPlane").objectReferenceValue = plane.transform;
        serialized.FindProperty("quarterViewCamera").objectReferenceValue = camera.transform;
        serialized.ApplyModifiedPropertiesWithoutUndo();
        foreach (var button in buttons) button.gameObject.SetActive(false);
        enter.interactable = false;
        world.SetActive(false);
        hud.SetActive(false);
        ApplyV01();
        ApplyC08ZoneTransitionUI();
        EditorSceneManager.SaveScene(EditorSceneManager.GetActiveScene(), "Assets/C02/C02.unity");
        AssetDatabase.SaveAssets();
    }

    // Visual-only representative. All server/UI state bindings remain on their original objects.
    public static void ApplyV01()
    {
        if (EditorApplication.isPlaying) throw new System.InvalidOperationException("Stop Play before styling C02.");
        if (EditorSceneManager.GetActiveScene().path != "Assets/C02/C02.unity")
            throw new System.InvalidOperationException("Open the C02 scene first.");
        if (!AssetDatabase.IsValidFolder("Assets/C02/Art")) AssetDatabase.CreateFolder("Assets/C02", "Art");
        var all = Object.FindObjectsByType<Transform>(FindObjectsInactive.Include, FindObjectsSortMode.None);
        Transform Find(string name) => System.Array.Find(all, t => t != null && t.name == name);
        var panel = Find("C01Panel");
        panel.GetComponent<UnityEngine.UI.Image>().color = Color.clear;
        var pr = panel.GetComponent<RectTransform>();
        pr.anchorMin = pr.anchorMax = pr.pivot = new Vector2(.5f, .5f);
        pr.anchoredPosition = Vector2.zero;
        pr.sizeDelta = new Vector2(1280, 720);
        Find("Background").GetComponent<UnityEngine.UI.Image>().color = Color.clear;
        foreach (string column in new[] { "SignIn", "Results" })
        {
            var r = Find(column).GetComponent<RectTransform>();
            r.anchoredPosition = column == "SignIn" ? new Vector2(898, -58) : new Vector2(898, -410);
            r.sizeDelta = new Vector2(340, column == "SignIn" ? 326 : 280);
            var group = r.GetComponent<UnityEngine.UI.VerticalLayoutGroup>();
            group.spacing = 4; group.padding = new RectOffset(18,18,14,14);
            Frame(r, new Color(.115f,.105f,.087f,1f));
        }
        foreach (var t in panel.GetComponentsInChildren<TMP_Text>(true))
        { t.color = new Color(.79f, .75f, .64f); t.fontSize = Mathf.Min(t.fontSize, 19); }
        void StyleText(string name, string value, int font, int height)
        { var t = Find(name).GetComponent<TMP_Text>(); t.text = value; t.fontSize = font; SetHeight(t.gameObject, height); }
        StyleText("StageLabel", "LOCAL TEST REALM", 11, 17);
        StyleText("Title", "ENTER THE REALM", 21, 34);
        StyleText("Subtitle", "Account sign in", 14, 21);
        StyleText("LoginIdLabel", "ACCOUNT", 12, 17);
        StyleText("PasswordLabel", "PASSWORD", 12, 17);
        SetHeight(Find("LoginIdInput").gameObject, 37);
        SetHeight(Find("PasswordInput").gameObject, 37);
        SetHeight(Find("LoginButton").gameObject, 40);
        StyleText("EndpointNote", "Choose a character below after sign in.", 11, 18);
        StyleText("ListTitle", "CHARACTER SELECT", 19, 29);
        SetHeight(Find("StatusText").gameObject, 35);
        Find("StatusText").GetComponent<TMP_Text>().fontSize = 13;
        Find("CharacterChoices").GetComponent<UnityEngine.UI.VerticalLayoutGroup>().spacing = 2;
        var choiceContent = Find("CharacterChoices").GetComponent<RectTransform>();
        if (choiceContent.parent.name != "Viewport")
        {
            int index = choiceContent.GetSiblingIndex();
            var scroll = new GameObject("CharacterScroll", typeof(RectTransform), typeof(UnityEngine.UI.ScrollRect)).GetComponent<UnityEngine.UI.ScrollRect>();
            scroll.transform.SetParent(Find("Results"), false); scroll.transform.SetSiblingIndex(index); SetHeight(scroll.gameObject, 87);
            var viewport = new GameObject("Viewport", typeof(RectTransform), typeof(UnityEngine.UI.Image), typeof(UnityEngine.UI.Mask)).GetComponent<RectTransform>();
            viewport.SetParent(scroll.transform, false); viewport.anchorMin = Vector2.zero; viewport.anchorMax = Vector2.one; viewport.offsetMin = viewport.offsetMax = Vector2.zero;
            viewport.GetComponent<UnityEngine.UI.Mask>().showMaskGraphic = false;
            choiceContent.SetParent(viewport,false); choiceContent.anchorMin = new Vector2(0,1); choiceContent.anchorMax = Vector2.one; choiceContent.pivot = new Vector2(.5f,1); choiceContent.anchoredPosition = Vector2.zero; choiceContent.sizeDelta = Vector2.zero;
            Object.DestroyImmediate(choiceContent.GetComponent<UnityEngine.UI.LayoutElement>());
            var fit = choiceContent.gameObject.AddComponent<UnityEngine.UI.ContentSizeFitter>(); fit.verticalFit = UnityEngine.UI.ContentSizeFitter.FitMode.PreferredSize;
            scroll.viewport = viewport; scroll.content = choiceContent; scroll.horizontal = false; scroll.vertical = true; scroll.scrollSensitivity = 22;
        }
        SetHeight(choiceContent.parent.parent.gameObject, 87);
        SetHeight(Find("EntryStatusText").gameObject, 36);
        Find("EntryStatusText").GetComponent<TMP_Text>().fontSize = 13;
        SetHeight(Find("EnterButton").gameObject, 44);
        foreach (var input in panel.GetComponentsInChildren<TMP_InputField>(true))
        {
            input.GetComponent<UnityEngine.UI.Image>().color = new Color(.71f,.67f,.56f);
            input.textComponent.color=new Color(.17f,.15f,.12f);
            input.placeholder.GetComponent<TMP_Text>().color=new Color(.33f,.30f,.24f);
        }
        foreach (var b in panel.GetComponentsInChildren<UnityEngine.UI.Button>(true))
        {
            bool row = b.name.StartsWith("CharacterChoice");
            b.GetComponent<UnityEngine.UI.Image>().color = row ? new Color(.23f,.24f,.21f) : new Color(.31f,.24f,.15f);
            var c = b.colors; c.normalColor = Color.white; c.highlightedColor = new Color(1f, .94f, .7f);
            c.pressedColor = new Color(.8f, .8f, .65f); c.selectedColor = new Color(1f, .87f, .45f); c.disabledColor = new Color(.65f, .68f, .64f, .6f); b.colors = c;
            b.GetComponentInChildren<TMP_Text>(true).color = new Color(.89f,.84f,.70f);
            if (row) { SetHeight(b.gameObject, 30); b.GetComponentInChildren<TMP_Text>(true).fontSize = 14; }
            Frame(b.GetComponent<RectTransform>(), b.GetComponent<UnityEngine.UI.Image>().color);
        }
        var hud = Find("WorldHud").GetComponent<RectTransform>();
        hud.anchorMin = hud.anchorMax = hud.pivot = new Vector2(0, 1);
        hud.anchoredPosition = new Vector2(24, -20); hud.sizeDelta = new Vector2(560, 112);
        hud.GetComponent<UnityEngine.UI.Image>().color = new Color(.12f,.115f,.095f,.88f);
        var wt = Find("WorldStatusText").GetComponent<TMP_Text>();
        wt.fontSize = 13; wt.color = new Color(.84f,.81f,.70f);
        wt.rectTransform.offsetMin = new Vector2(16, 10); wt.rectTransform.offsetMax = new Vector2(-16, -10);
        var disconnect = Find("DisconnectButton").GetComponent<RectTransform>();
        disconnect.anchorMin = disconnect.anchorMax = disconnect.pivot = new Vector2(1, 0);
        disconnect.anchoredPosition = new Vector2(0, -49); disconnect.sizeDelta = new Vector2(142, 36);
        disconnect.GetComponent<UnityEngine.UI.Image>().color = new Color(.16f, .32f, .32f, .93f);
        disconnect.GetComponentInChildren<TMP_Text>(true).fontSize = 14;
        var world = Find("C02World");
        Find("FieldPlane").GetComponent<Renderer>().enabled = false;
        var capsule = Find("Capsule"); capsule.GetComponent<Renderer>().enabled = false;
        var oldHero = Find("OwnCharacter").Find("V01Hero"); if (oldHero != null) Object.DestroyImmediate(oldHero.gameObject);
        Hero(Find("OwnCharacter"), Vector3.zero, 1.20f);
        var oldField = world.Find("V01Outpost"); if (oldField != null) Object.DestroyImmediate(oldField.gameObject);
        Outpost(world, new Vector3(10, 0, 10));
        var camera = Find("QuarterViewCamera").GetComponent<Camera>();
        camera.orthographicSize = 7.2f; camera.backgroundColor = new Color(.64f,.71f,.73f);
        var light = Find("FieldLight").GetComponent<Light>();
        light.color = new Color(1f, .94f, .82f); light.intensity = 1.4f;
        light.shadows = LightShadows.Soft; light.transform.rotation = Quaternion.Euler(48, -35, 0);
        RenderSettings.ambientMode = UnityEngine.Rendering.AmbientMode.Flat;
        RenderSettings.ambientLight = new Color(.61f,.65f,.68f);
        RenderSettings.fog = false;
        var previous = GameObject.Find("V01LoginDiorama"); if (previous != null) Object.DestroyImmediate(previous);
        var backdrop = new GameObject("V01LoginDiorama"); backdrop.transform.position = new Vector3(1000, 0, 1000);
        Outpost(backdrop.transform, Vector3.zero);
        Hero(backdrop.transform, Vector3.zero, 2.1f);
        var sun = new GameObject("PreviewSun", typeof(Light)).GetComponent<Light>();
        sun.transform.SetParent(backdrop.transform, false); sun.type = LightType.Directional; sun.color = light.color; sun.intensity = 1.6f; sun.shadows = LightShadows.Soft; sun.transform.rotation = light.transform.rotation;
        // PreviewSun alone lights both views; keep the field light disabled to avoid double exposure.
        light.enabled = false;
        var mainCamera = Find("Main Camera").GetComponent<Camera>();
        mainCamera.orthographic = false; mainCamera.fieldOfView = 36;
        mainCamera.transform.position = backdrop.transform.position + new Vector3(5.2f, 3.8f, -11.2f);
        mainCamera.transform.LookAt(backdrop.transform.position + new Vector3(1.8f, 2.0f, 1.1f));
        mainCamera.backgroundColor = camera.backgroundColor;
        var branding = Find("Background").Find("V01Branding"); if (branding != null) Object.DestroyImmediate(branding.gameObject);
        var brand = Text("V01Branding", Find("Background"), "S U N W A R D", 29);
        brand.richText = true; brand.fontStyle = FontStyles.Normal; brand.color = new Color(.87f,.82f,.66f);
        brand.rectTransform.anchorMin = brand.rectTransform.anchorMax = brand.rectTransform.pivot = new Vector2(0,1);
        brand.rectTransform.anchoredPosition = new Vector2(44,-33); brand.rectTransform.sizeDelta = new Vector2(670,52);
        var brandShadow=brand.gameObject.AddComponent<UnityEngine.UI.Shadow>(); brandShadow.effectColor=new Color(.18f,.22f,.17f,.8f); brandShadow.effectDistance=new Vector2(2,-3);
        var note = Text("PreviewCaption", brand.transform, "CHARACTER & ENVIRONMENT PREVIEW", 12);
        note.rectTransform.anchorMin = note.rectTransform.anchorMax = note.rectTransform.pivot = new Vector2(0,0);
        note.rectTransform.anchoredPosition = new Vector2(0,-13); note.rectTransform.sizeDelta = new Vector2(500,25);
        // Clear the former Results layout's driven anchors before persisting the reparented content.
        UnityEngine.UI.LayoutRebuilder.ForceRebuildLayoutImmediate(Find("Results").GetComponent<RectTransform>());
        Canvas.ForceUpdateCanvases();
        choiceContent.anchorMin = new Vector2(0, 1);
        choiceContent.anchorMax = Vector2.one;
        choiceContent.pivot = new Vector2(.5f, 1);
        choiceContent.anchoredPosition = Vector2.zero;
        choiceContent.sizeDelta = new Vector2(0, choiceContent.sizeDelta.y);
        UnityEngine.UI.LayoutRebuilder.ForceRebuildLayoutImmediate(choiceContent);
        EditorUtility.SetDirty(choiceContent);
        BindC04();
        ApplyV03();
        ApplyC05();
        EditorSceneManager.MarkSceneDirty(EditorSceneManager.GetActiveScene());
        EditorSceneManager.SaveScene(EditorSceneManager.GetActiveScene()); AssetDatabase.SaveAssets();
    }

    public static void ApplyC05()
    {
        if (EditorApplication.isPlaying || EditorSceneManager.GetActiveScene().path != "Assets/C02/C02.unity")
            throw new System.InvalidOperationException("C05 requires the C02 scene in Edit Mode.");
        var screen = Object.FindFirstObjectByType<C02WorldScreen>();
        var hud = ((GameObject)new SerializedObject(screen).FindProperty("worldHud").objectReferenceValue).transform;
        var panel = (hud.Find("C05CombatPanel") ?? hud.parent.Find("C05CombatPanel")) as RectTransform;
        if (panel == null)
        {
            panel = new GameObject("C05CombatPanel", typeof(RectTransform), typeof(UnityEngine.UI.Image)).GetComponent<RectTransform>();
        }
        panel.SetParent(hud, false);
        panel.anchorMin = panel.anchorMax = panel.pivot = new Vector2(0, 0);
        panel.anchoredPosition = new Vector2(0, -246);
        panel.sizeDelta = new Vector2(560, 184);
        panel.GetComponent<UnityEngine.UI.Image>().color = new Color(.12f, .105f, .08f, .95f);
        panel.GetComponent<UnityEngine.UI.Image>().raycastTarget = true;
        Frame(panel, panel.GetComponent<UnityEngine.UI.Image>().color);
        TMP_Text Label(string name, int size, Vector2 position, Vector2 dimensions)
        {
            var found = panel.Find(name);
            var text = found != null ? found.GetComponent<TMP_Text>() : Text(name, panel, "", size);
            text.fontSize = size; text.color = new Color(.89f, .84f, .70f); text.raycastTarget = false;
            text.rectTransform.anchorMin = text.rectTransform.anchorMax = text.rectTransform.pivot = new Vector2(0, 1);
            text.rectTransform.anchoredPosition = position; text.rectTransform.sizeDelta = dimensions;
            return text;
        }
        var vitals = Label("C05Vitals", 16, new Vector2(14, -10), new Vector2(532, 25));
        var aim = Label("C05Aim", 12, new Vector2(14, -37), new Vector2(532, 22));
        var status = Label("C05Status", 12, new Vector2(14, -115), new Vector2(532, 62));
        var buttons = new UnityEngine.UI.Button[3];
        var labels = new[] { "Z  STRIKE", "X  RING", "C  LINE" };
        for (int slot = 0; slot < 3; slot++)
        {
            string name = "C05Skill" + slot;
            var found = panel.Find(name);
            var button = found != null ? found.GetComponent<UnityEngine.UI.Button>() : Button(name, panel, labels[slot], 42, 16);
            var rect = button.GetComponent<RectTransform>();
            rect.anchorMin = rect.anchorMax = rect.pivot = new Vector2(0, 1);
            rect.anchoredPosition = new Vector2(14 + slot * 180, -65); rect.sizeDelta = new Vector2(172, 42);
            button.GetComponent<UnityEngine.UI.Image>().color = new Color(.31f, .24f, .15f);
            button.GetComponent<UnityEngine.UI.Image>().raycastTarget = true;
            button.GetComponentInChildren<TMP_Text>().text = labels[slot];
            button.GetComponentInChildren<TMP_Text>().color = new Color(.94f, .86f, .65f);
            var labelRect = button.GetComponentInChildren<TMP_Text>().rectTransform;
            labelRect.anchorMin = Vector2.zero; labelRect.anchorMax = Vector2.one;
            labelRect.offsetMin = labelRect.offsetMax = Vector2.zero;
            var icon = rect.Find("SkillIcon"); if (icon != null) icon.gameObject.SetActive(false);
            var keyBadge = rect.Find("SkillKeyBadge"); if (keyBadge != null) keyBadge.gameObject.SetActive(false);
            Frame(rect, button.GetComponent<UnityEngine.UI.Image>().color);
            buttons[slot] = button;
        }
        var combat = screen.GetComponent<C05CombatScreen>() ?? screen.gameObject.AddComponent<C05CombatScreen>();
        var refs = new SerializedObject(combat);
        refs.FindProperty("loginScreen").objectReferenceValue = Object.FindFirstObjectByType<C01LoginScreen>();
        refs.FindProperty("worldScreen").objectReferenceValue = screen;
        refs.FindProperty("combatPanel").objectReferenceValue = panel.gameObject;
        var slots = refs.FindProperty("skillButtons"); slots.arraySize = 3;
        for (int slot = 0; slot < 3; slot++) slots.GetArrayElementAtIndex(slot).objectReferenceValue = buttons[slot];
        refs.FindProperty("vitalsText").objectReferenceValue = vitals;
        refs.FindProperty("aimText").objectReferenceValue = aim;
        refs.FindProperty("combatStatusText").objectReferenceValue = status;
        refs.ApplyModifiedPropertiesWithoutUndo();
        combat.enabled = true;
        EditorSceneManager.MarkSceneDirty(EditorSceneManager.GetActiveScene());
        EditorSceneManager.SaveScene(EditorSceneManager.GetActiveScene());
    }

    // Visual coverage only. Zone limits, picking surface and runtime follow camera stay unchanged.
    public static void ApplyV03()
    {
        if (EditorApplication.isPlaying || EditorSceneManager.GetActiveScene().path != "Assets/C02/C02.unity")
            throw new System.InvalidOperationException("V03 requires the C02 scene in Edit Mode.");
        const string folder = "Assets/C02/Art/V03";
        if (!AssetDatabase.IsValidFolder(folder)) AssetDatabase.CreateFolder("Assets/C02/Art", "V03");
        var world = Object.FindObjectsByType<Transform>(FindObjectsInactive.Include, FindObjectsSortMode.None);
        var parent = System.Array.Find(world, t => t.name == "C02World");
        var outpost = parent.Find("V01Outpost");
        var old = parent.Find("V03FieldSurround");
        if (old != null) Object.DestroyImmediate(old.gameObject);
        var root = new GameObject("V03FieldSurround").transform;
        root.SetParent(parent, false);
        Material CopyMaterial(string name, string source, Color tint)
        {
            string path = folder + "/" + name + ".mat";
            var mat = AssetDatabase.LoadAssetAtPath<Material>(path);
            var basis = AssetDatabase.LoadAssetAtPath<Material>("Assets/C02/Art/" + source + ".mat");
            if (mat == null) { mat = new Material(basis); AssetDatabase.CreateAsset(mat, path); }
            else mat.CopyPropertiesFromMaterial(basis);
            mat.name = name; mat.color = tint; EditorUtility.SetDirty(mat); return mat;
        }
        var meadow = CopyMaterial("Meadow", "EarthAndGrass", Color.white);
        // Match the original 64 m ground's world-space texture frequency and phase exactly.
        meadow.mainTextureScale = new Vector2(240f / 64f, 240f / 64f);
        meadow.mainTextureOffset = Vector2.zero;
        // 240 m square: [-70,170], safely beyond every Zone 1 camera footprint.
        var ground = Shape("Zone1Meadow", PrimitiveType.Plane, root,
            new Vector3(50, -.035f, 50), new Vector3(24, 1, 24), meadow);
        ground.GetComponent<Renderer>().receiveShadows = true;
        // Render a single continuous surface; retain the original mesh/material for the diorama.
        outpost.Find("TexturedGround").GetComponent<Renderer>().enabled = false;
        // The former short ridge crossed playable coordinates; keep it only in the login diorama.
        var shortRidge = outpost.Find("RollingDistantRidge");
        shortRidge.gameObject.SetActive(false);
        var ridgeMaterial = CopyMaterial("DistantRidge", "MountainHaze", new Color(.43f, .51f, .48f));
        for (int side = 0; side < 4; side++)
        {
            var ridge = Object.Instantiate(shortRidge.gameObject, root);
            ridge.name = "DistantRidge" + side; ridge.SetActive(true);
            var tr = ridge.transform;
            tr.localPosition = side == 0 ? new Vector3(50, -.5f, 128) : side == 1 ? new Vector3(128, -.5f, 50) : side == 2 ? new Vector3(50, -.5f, -28) : new Vector3(-28, -.5f, 50);
            tr.localRotation = Quaternion.Euler(0, side * 90, 0);
            tr.localScale = new Vector3(1.65f, .7f, 1.3f);
            ridge.GetComponent<Renderer>().sharedMaterial = ridgeMaterial;
        }
        var pine = outpost.Find("Pine0");
        var rock = outpost.Find("RoadsideRock1");
        var paleStone = CopyMaterial("OldArenaStone", "WeatheredLimestone", new Color(.48f, .49f, .43f));
        var pathMaterial = CopyMaterial("WornTrail", "EarthAndGrass", Color.white);
        // Sample the existing texture's earth strip; no new bitmap or shader is needed.
        pathMaterial.mainTextureScale = new Vector2(.14f, .7f);
        pathMaterial.mainTextureOffset = new Vector2(.43f, .15f);
        // A worn trail links the existing gate to the central landmark. All surfaces are flat,
        // decorative and below actor foot height; they do not define server collision or range.
        var trail = Shape("WornCentralTrail", PrimitiveType.Plane, root,
            new Vector3(30, -.025f, 30), new Vector3(.38f, 1, 5.4f), pathMaterial);
        trail.transform.localRotation = Quaternion.Euler(0, 45, 0);
        for (int i = 0; i < 12; i++)
        {
            // Three short remnants occupy about 20% of the old circumference, not a closed ring.
            float angle = (22 + i / 4 * 120 + i % 4 * 8) * Mathf.Deg2Rad;
            var slab = Object.Instantiate(rock.gameObject, root);
            slab.name = "BrokenArenaSlab" + i;
            float radius = 9.3f + Mathf.Sin(i * 2.3f) * .22f;
            slab.transform.localPosition = new Vector3(50 + Mathf.Cos(angle) * radius, -.06f, 50 + Mathf.Sin(angle) * radius);
            slab.transform.localRotation = Quaternion.Euler(i % 3 * 5 - 4, -angle * Mathf.Rad2Deg + Mathf.Sin(i * 3.1f) * 19, i % 4 * 4 - 5);
            slab.transform.localScale = new Vector3(.48f + i % 3 * .23f, .28f + i % 2 * .15f, .7f + i % 4 * .12f);
            slab.GetComponent<Renderer>().sharedMaterial = paleStone;
        }
        for (int ruin = 0; ruin < 2; ruin++) for (int block = 0; block < 3; block++)
        {
            var fragment = Object.Instantiate(rock.gameObject, root);
            fragment.name = "BrokenOuterMasonry" + ruin + "_" + block;
            var basePosition = ruin == 0 ? new Vector3(39, 0, 55) : new Vector3(57, 0, 61);
            fragment.transform.localPosition = basePosition + new Vector3(block == 0 ? -.35f : .05f * block, .19f + block * .43f, block * .08f);
            fragment.transform.localRotation = Quaternion.Euler(block * 3, 22 + ruin * 57 + block * 3, block == 2 ? -8 : 1);
            fragment.transform.localScale = new Vector3(1.75f - block * .4f, .45f, .75f);
            fragment.GetComponent<Renderer>().sharedMaterial = paleStone;
        }
        for (int i = 0; i < 7; i++)
        {
            var stone = Object.Instantiate(rock.gameObject, root);
            stone.name = "ScatteredArenaPaving" + i;
            float angle = i * 2.39996f;
            float radius = 2 + (i % 3) * 1.7f;
            stone.transform.localPosition = new Vector3(50 + Mathf.Cos(angle) * radius, -.025f, 50 + Mathf.Sin(angle) * radius);
            stone.transform.localRotation = Quaternion.Euler(0, i * 31, 0);
            stone.transform.localScale = new Vector3(.6f + i % 3 * .2f, .12f, .5f + i % 2 * .2f);
            stone.GetComponent<Renderer>().sharedMaterial = paleStone;
        }
        // Sparse, ankle-height clusters break up the large field without hiding server actors.
        var grass = outpost.Find("GrassClump0");
        for (int row = 0; row < 5; row++) for (int col = 0; col < 5; col++)
        {
            float x = 3 + col * 24 + Mathf.Sin(row * 2.7f + col) * 3;
            float z = 3 + row * 24 + Mathf.Cos(col * 1.9f + row) * 3;
            if ((x < 24 && z < 28) || Vector2.Distance(new Vector2(x, z), new Vector2(50, 50)) < 14) continue;
            var cluster = new GameObject("LowFieldCluster" + row + "_" + col).transform;
            cluster.SetParent(root, false); cluster.localPosition = new Vector3(x, 0, z);
            for (int tuft = 0; tuft < 5; tuft++)
            {
                var clump = Object.Instantiate(grass.gameObject, cluster);
                clump.name = "Grass" + tuft;
                clump.transform.localPosition = new Vector3(Mathf.Sin(tuft * 2.4f) * 1.4f, -.015f, Mathf.Cos(tuft * 2.4f) * 1.4f);
                clump.transform.localRotation = Quaternion.Euler(0, tuft * 67, 0);
                clump.transform.localScale = Vector3.one * (1.8f + tuft * .23f);
            }
            var stone = Object.Instantiate(rock.gameObject, cluster);
            stone.name = "LowWeatheredStone";
            stone.transform.localPosition = new Vector3(.1f, -.08f, .2f);
            stone.transform.localRotation = Quaternion.Euler(0, (row + col) * 49, 0);
            stone.transform.localScale = new Vector3(1.1f, .45f, .8f);
        }
        // Four sparse, staggered tree lines are outside [-5,105]. No gameplay obstacle implied.
        for (int side = 0; side < 4; side++) for (int i = 0; i < 12; i++)
        {
            float along = -16 + i * 12;
            float across = 117 + (i % 3) * 5;
            var tree = Object.Instantiate(pine.gameObject, root);
            tree.name = "OuterPine" + side + "_" + i;
            tree.transform.localPosition = side == 0 ? new Vector3(along, 0, across) : side == 1 ? new Vector3(across, 0, along) : side == 2 ? new Vector3(along, 0, 100 - across) : new Vector3(100 - across, 0, along);
            tree.transform.localRotation = Quaternion.Euler(0, i * 71 + side * 37, 0);
            tree.transform.localScale = Vector3.one * (.85f + (i % 4) * .14f);
            if (i % 2 != 0) continue;
            var stone = Object.Instantiate(rock.gameObject, root);
            stone.name = "OuterStone" + side + "_" + i;
            stone.transform.localPosition = tree.transform.localPosition + new Vector3(2.4f, -.12f, -2.1f);
            stone.transform.localRotation = Quaternion.Euler(0, i * 53, 0);
            stone.transform.localScale = new Vector3(2.4f, 1.6f, 2.1f);
        }
        EditorSceneManager.MarkSceneDirty(EditorSceneManager.GetActiveScene());
        EditorSceneManager.SaveScene(EditorSceneManager.GetActiveScene());
        AssetDatabase.SaveAssets();
    }

    // V04: a unique, continuous meadow painting and meandering trail, not repeated tiles.
    // Only visual authoring: server coordinates, picking plane and all runtime contracts stay intact.
    public static void ApplyV04()
    {
        if (EditorApplication.isPlaying || EditorSceneManager.GetActiveScene().path != "Assets/C02/C02.unity")
            throw new System.InvalidOperationException("V04 requires C02 in Edit Mode.");
        const string folder = "Assets/C02/Art/V04";
        if (!AssetDatabase.IsValidFolder(folder)) AssetDatabase.CreateFolder("Assets/C02/Art", "V04");
        var all = Object.FindObjectsByType<Transform>(FindObjectsInactive.Include, FindObjectsSortMode.None);
        var world = System.Array.Find(all, t => t.name == "C02World");
        var outpost = world.Find("V01Outpost");
        var previous = world.Find("V03FieldSurround");
        var old = world.Find("V04MeanderingMeadow");
        if (old != null) Object.DestroyImmediate(old.gameObject);
        var root = new GameObject("V04MeanderingMeadow").transform; root.SetParent(world, false);
        previous.gameObject.SetActive(false);
        // The gate and its adult guard remain. Remove old alternating roadside rows from this field only.
        foreach (Transform child in outpost)
            if (child.name.StartsWith("Pine") || child.name.StartsWith("GrassClump") || child.name.StartsWith("RoadsideRock")) child.gameObject.SetActive(false);
        Material V04Material(string name, Color tint, string source = null)
        {
            string path = folder + "/" + name + ".mat";
            var mat = AssetDatabase.LoadAssetAtPath<Material>(path);
            if (mat == null)
            {
                mat = source == null ? new Material(Shader.Find("Universal Render Pipeline/Lit")) : new Material(AssetDatabase.LoadAssetAtPath<Material>("Assets/C02/Art/" + source + ".mat"));
                AssetDatabase.CreateAsset(mat, path);
            }
            mat.color = tint; mat.SetFloat("_Smoothness", .1f); EditorUtility.SetDirty(mat); return mat;
        }
        var route = new System.Collections.Generic.List<Vector2>();
        // Three smooth adjoining arcs; the southwest gate leads through a generous central clearing.
        void Curve(Vector2 a, Vector2 b, Vector2 c, Vector2 d)
        {
            for (int i = 0; i <= 28; i++)
            {
                float t = i / 28f, u = 1 - t;
                route.Add(u*u*u*a + 3*u*u*t*b + 3*u*t*t*c + t*t*t*d);
            }
        }
        Curve(new Vector2(10,-15), new Vector2(7,9), new Vector2(10,26), new Vector2(28,32));
        Curve(new Vector2(28,32), new Vector2(48,36), new Vector2(37,54), new Vector2(54,59));
        Curve(new Vector2(54,59), new Vector2(81,66), new Vector2(69,86), new Vector2(98,119));
        float TrailDistance(Vector2 p)
        {
            float distance = float.MaxValue;
            for (int i = 1; i < route.Count; i++)
            {
                var a = route[i - 1]; var delta = route[i] - a;
                float t = Mathf.Clamp01(Vector2.Dot(p-a,delta) / Mathf.Max(.0001f,delta.sqrMagnitude));
                distance = Mathf.Min(distance, Vector2.Distance(p, a + delta*t));
            }
            return distance;
        }
        const int size = 1024;
        string texturePath = folder + "/ContinuousMeadow.asset";
        var texture = AssetDatabase.LoadAssetAtPath<Texture2D>(texturePath);
        bool created = texture == null;
        if (created) texture = new Texture2D(size,size,TextureFormat.RGB24,true,false) {name="ContinuousMeadow"};
        texture.wrapMode=TextureWrapMode.Clamp; texture.filterMode=FilterMode.Trilinear; texture.anisoLevel=4;
        var pixels = new Color[size*size];
        for (int y=0; y<size; y++) for (int x=0; x<size; x++)
        {
            var p = new Vector2(-70 + x*240f/(size-1), -70 + y*240f/(size-1));
            float broad = Mathf.PerlinNoise(p.x*.045f+42,p.y*.045f+17);
            float detail = Mathf.PerlinNoise(p.x*.62f+19,p.y*.62f+31);
            float patch = Mathf.PerlinNoise(p.x*.15f+71,p.y*.15f+11);
            var color = Color.Lerp(new Color(.24f,.30f,.16f),new Color(.48f,.49f,.28f), broad);
            color = Color.Lerp(color,new Color(.32f,.34f,.21f),Mathf.SmoothStep(0,1,Mathf.InverseLerp(.60f,.83f,patch))*.35f);
            float distance = TrailDistance(p);
            float worn = 1-Mathf.SmoothStep(0,1,Mathf.InverseLerp(1.15f,3.05f,distance+(patch-.5f)*1.6f));
            // Low contrast, sub-metre grain avoids a checker-like alias at the play camera distance.
            var earth = Color.Lerp(new Color(.45f,.385f,.27f),new Color(.49f,.425f,.305f),detail);
            color = Color.Lerp(color,earth,worn*.87f);
            color *= .97f + detail*.06f; pixels[y*size+x] = color;
        }
        texture.SetPixels(pixels); texture.Apply(true,false);
        if(created)AssetDatabase.CreateAsset(texture,texturePath);else EditorUtility.SetDirty(texture);
        var meadow = V04Material("MeadowAndOldTrail",Color.white); meadow.mainTexture=texture;
        meadow.mainTextureScale=Vector2.one; meadow.mainTextureOffset=Vector2.zero;
        // Keep local ground grain independent of the 240m colour/trail mask. These are the
        // same two noise frequencies as V02 GroundBlend, at its original metre density.
        string detailPath=folder+"/LocalGroundGrain.asset";
        var detailTexture=AssetDatabase.LoadAssetAtPath<Texture2D>(detailPath);
        bool newDetail=detailTexture==null;
        if(newDetail)detailTexture=new Texture2D(128,128,TextureFormat.RGB24,true,true){name="LocalGroundGrain"};
        detailTexture.wrapMode=TextureWrapMode.Repeat;detailTexture.filterMode=FilterMode.Trilinear;detailTexture.anisoLevel=4;
        var grain=new Color[128*128];
        float PeriodicNoise(int x,int y,float frequency,float offsetX,float offsetY)
        {
            float u=x/128f,v=y/128f;
            float a=Mathf.PerlinNoise(x*frequency+offsetX,y*frequency+offsetY);
            float b=Mathf.PerlinNoise((x-128)*frequency+offsetX,y*frequency+offsetY);
            float c=Mathf.PerlinNoise(x*frequency+offsetX,(y-128)*frequency+offsetY);
            float d=Mathf.PerlinNoise((x-128)*frequency+offsetX,(y-128)*frequency+offsetY);
            return Mathf.Lerp(Mathf.Lerp(a,b,u),Mathf.Lerp(c,d,u),v);
        }
        for(int y=0;y<128;y++)for(int x=0;x<128;x++)
        {
            float noise=PeriodicNoise(x,y,.11f,5,7)*.6f+PeriodicNoise(x,y,.37f,0,0)*.4f;
            // Linear 0.5 is neutral in URP's detail-albedo multiplier; modest contrast only.
            float gray=.5f+(noise-.5f)*.72f;grain[y*128+x]=new Color(gray,gray,gray);
        }
        detailTexture.SetPixels(grain);detailTexture.Apply(true,false);
        if(newDetail)AssetDatabase.CreateAsset(detailTexture,detailPath);else EditorUtility.SetDirty(detailTexture);
        meadow.SetTexture("_DetailAlbedoMap",detailTexture);meadow.SetTextureScale("_DetailAlbedoMap",Vector2.one*15);
        meadow.SetFloat("_DetailAlbedoMapScale",.85f);meadow.SetFloat("_DetailNormalMapScale",0);
        meadow.DisableKeyword("_DETAIL_MULX2");meadow.EnableKeyword("_DETAIL_SCALED");EditorUtility.SetDirty(meadow);
        string meshPath=folder+"/ContinuousGround.asset";
        var mesh=AssetDatabase.LoadAssetAtPath<Mesh>(meshPath);
        if(mesh==null){mesh=new Mesh {name="ContinuousGround"};AssetDatabase.CreateAsset(mesh,meshPath);}
        mesh.vertices=new[]{new Vector3(-70,-.035f,-70),new Vector3(-70,-.035f,170),new Vector3(170,-.035f,170),new Vector3(170,-.035f,-70)};
        mesh.uv=new[]{Vector2.zero,Vector2.up,Vector2.one,Vector2.right}; mesh.triangles=new[]{0,1,2,0,2,3}; mesh.RecalculateNormals();mesh.RecalculateBounds();EditorUtility.SetDirty(mesh);
        var ground=new GameObject("ContinuousWalkableMeadow",typeof(MeshFilter),typeof(MeshRenderer));ground.transform.SetParent(root,false);
        ground.GetComponent<MeshFilter>().sharedMesh=mesh; ground.GetComponent<MeshRenderer>().sharedMaterial=meadow;
        // Existing original meshes are reused; no external or generated model dependency.
        var rock = outpost.Find("RoadsideRock1"); var grass = outpost.Find("GrassClump0"); var pine=outpost.Find("Pine0");
        var meadowStoneMesh = MeadowStoneMesh();
        var limestone=V04Material("MossWeatheredStone",new Color(.46f,.47f,.38f),"WeatheredLimestone");
        var grassMat=V04Material("MeadowTufts",new Color(.31f,.39f,.18f),"Moss");
        var random=new System.Random(42613);
        float Rand(float min,float max) => min+(float)random.NextDouble()*(max-min);
        // Irregular low clusters remain ankle-height and traversable in the server's flat field.
        for(int i=0;i<285;i++)
        {
            var p=new Vector2(Rand(-12,112),Rand(-12,112));
            if(TrailDistance(p)<4.5f || Vector2.Distance(p,new Vector2(50,50))<7 || (p.x<23&&p.y<24))continue;
            float fertility=Mathf.PerlinNoise(p.x*.065f+9,p.y*.065f+23);
            if(fertility<.46f)continue;
            var group=new GameObject("WildMeadowCluster"+i).transform;group.SetParent(root,false);group.localPosition=new Vector3(p.x,0,p.y);
            for(int j=0;j<8;j++)
            {
                var tuft=Object.Instantiate(grass.gameObject,group);tuft.name="MeadowGrass"+j;tuft.SetActive(true);
                tuft.transform.localPosition=new Vector3(Rand(-2.5f,2.5f),-.015f,Rand(-2,2));tuft.transform.localRotation=Quaternion.Euler(0,Rand(0,360),0);tuft.transform.localScale=Vector3.one*Rand(1.8f,2.8f);tuft.GetComponent<Renderer>().sharedMaterial=grassMat;
            }
            if(i%3==0)
            {
                var stone=Object.Instantiate(rock.gameObject,group);stone.SetActive(true);stone.name="BuriedOutcrop";stone.transform.localPosition=new Vector3(.7f,-.08f,.5f);stone.transform.localRotation=Quaternion.Euler(0,Rand(0,360),0);stone.transform.localScale=new Vector3(Rand(.7f,1.7f),.22f,Rand(.6f,1.4f));stone.GetComponent<Renderer>().sharedMaterial=limestone;
                stone.GetComponent<MeshFilter>().sharedMesh = meadowStoneMesh;
            }
        }
        // Forest masses stay outside the playable square, rather than implying invisible interior walls.
        for(int i=0;i<92;i++)
        {
            float a=Rand(0,Mathf.PI*2); var p=new Vector2(Mathf.Cos(a),Mathf.Sin(a));
            p*=Rand(65,85)/Mathf.Max(Mathf.Abs(p.x),Mathf.Abs(p.y));p+=Vector2.one*50;
            if(TrailDistance(p)<8)continue;
            var tree=Object.Instantiate(pine.gameObject,root);tree.SetActive(true);tree.name="AsymmetricForest"+i;
            tree.transform.localPosition=new Vector3(p.x,-.05f,p.y);tree.transform.localRotation=Quaternion.Euler(0,Rand(0,360),0);tree.transform.localScale=Vector3.one*Rand(.8f,1.65f);
        }
        // An open broken shrine gives the middle a directional landmark, never an enclosed arena.
        for(int i=0;i<11;i++)
        {
            float x=41+Rand(-2.8f,2.8f),z=55+Rand(-2.5f,2.5f);
            var stone=Object.Instantiate(rock.gameObject,root);stone.SetActive(true);stone.name="BuriedShrineStone"+i;stone.transform.localPosition=new Vector3(x,-.02f,z);stone.transform.localRotation=Quaternion.Euler(0,Rand(-25,35),0);stone.transform.localScale=new Vector3(Rand(.7f,1.6f),Rand(.12f,.28f),Rand(.5f,1.1f));stone.GetComponent<Renderer>().sharedMaterial=limestone;
            stone.GetComponent<MeshFilter>().sharedMesh = meadowStoneMesh;
        }
        for(int i=0;i<3;i++)
        {
            var stone=Object.Instantiate(rock.gameObject,root);stone.SetActive(true);stone.name="LowShrineRemnant"+i;stone.transform.localPosition=new Vector3(40.5f+i*.15f,.15f+i*.32f,57);stone.transform.localRotation=Quaternion.Euler(0,18+i*4,i==2?9:0);stone.transform.localScale=new Vector3(1.7f-i*.3f,.34f,.72f);stone.GetComponent<Renderer>().sharedMaterial=limestone;
        }
        // A low, asymmetric outcrop at the clearing's near corner gives scale without blocking sight.
        var verge=new GameObject("ClearingVergeOutcrop").transform;verge.SetParent(root,false);verge.localPosition=new Vector3(59,0,44);
        for(int i=0;i<5;i++)
        {
            var stone=Object.Instantiate(rock.gameObject,verge);stone.SetActive(true);stone.name="MossyVergeRock"+i;stone.transform.localPosition=new Vector3(Rand(-2,2),.02f,Rand(-1.5f,1.5f));stone.transform.localRotation=Quaternion.Euler(Rand(-9,9),Rand(0,360),Rand(-8,8));stone.transform.localScale=new Vector3(Rand(.8f,1.8f),Rand(.28f,.65f),Rand(.7f,1.3f));stone.GetComponent<Renderer>().sharedMaterial=limestone;
        }
        for(int i=0;i<22;i++)
        {
            var tuft=Object.Instantiate(grass.gameObject,verge);tuft.SetActive(true);tuft.name="VergeGrass"+i;tuft.transform.localPosition=new Vector3(Rand(-3,3),-.015f,Rand(-2.3f,2.3f));tuft.transform.localRotation=Quaternion.Euler(0,Rand(0,360),0);tuft.transform.localScale=Vector3.one*Rand(2,3);tuft.GetComponent<Renderer>().sharedMaterial=grassMat;
        }
        for(int i=0;i<4;i++)
        {
            var ridge=Object.Instantiate(previous.Find("DistantRidge"+i).gameObject,root);ridge.SetActive(true);ridge.name="DistantForestRidge"+i;
        }
        AssetDatabase.SaveAssets();EditorSceneManager.MarkSceneDirty(EditorSceneManager.GetActiveScene());EditorSceneManager.SaveScene(EditorSceneManager.GetActiveScene());
    }

    [MenuItem("C02/Art/Apply Meadow Stone Mesh")]
    public static void ApplyMeadowStoneMesh()
    {
        var scene = EditorSceneManager.GetActiveScene();
        if (EditorApplication.isPlaying || scene.path != "Assets/C02/C02.unity")
            throw new System.InvalidOperationException("Meadow stone authoring requires C02 in Edit Mode.");
        if (scene.isDirty) throw new System.InvalidOperationException("Save existing C02 edits before applying and saving meadow stones.");
        Transform field = null;
        foreach (var sceneRoot in scene.GetRootGameObjects())
        {
            var world = System.Array.Find(sceneRoot.GetComponentsInChildren<Transform>(true), t => t.name == "C02World");
            if (world != null) { field = world.Find("V04MeanderingMeadow"); break; }
        }
        if (field == null) throw new System.InvalidOperationException("Apply V04 before replacing meadow stones.");
        var mesh = MeadowStoneMesh();
        int count = 0;
        Undo.IncrementCurrentGroup();
        int undoGroup = Undo.GetCurrentGroup();
        Undo.SetCurrentGroupName("Apply Meadow Stone Mesh");
        foreach (var filter in field.GetComponentsInChildren<MeshFilter>(true))
        {
            if (filter.name != "BuriedOutcrop" && !filter.name.StartsWith("BuriedShrineStone", System.StringComparison.Ordinal)) continue;
            if (filter.sharedMesh == mesh) continue;
            Undo.RecordObject(filter, "Replace meadow stone mesh");
            filter.sharedMesh = mesh;
            EditorUtility.SetDirty(filter);
            count++;
        }
        Undo.CollapseUndoOperations(undoGroup);
        AssetDatabase.SaveAssetIfDirty(mesh);
        if (count > 0)
        {
            EditorSceneManager.MarkSceneDirty(scene);
            if (!EditorSceneManager.SaveScene(scene)) throw new System.InvalidOperationException("Meadow stone scene save failed.");
        }
        Debug.Log("Meadow stone mesh applied to " + count + " low stones. Scene saved when changed; mesh replacement supports Undo.");
    }

    private static Mesh MeadowStoneMesh()
    {
        const string folder = "Assets/C02/Art/V04";
        const string path = folder + "/MeadowChamferedStone.asset";
        var mesh = AssetDatabase.LoadAssetAtPath<Mesh>(path);
        if (mesh != null) return mesh;
        if (!AssetDatabase.IsValidFolder(folder)) AssetDatabase.CreateFolder("Assets/C02/Art", "V04");
        // Unequal eight-sided footprint, broad sloping shoulders and an offset crown.
        // Unit-height bounds preserve the buried stones' existing ankle-height transforms.
        var outline = new[] { new Vector2(-.48f,-.19f), new Vector2(-.26f,-.46f), new Vector2(.13f,-.50f),
            new Vector2(.45f,-.28f), new Vector2(.50f,.10f), new Vector2(.23f,.43f),
            new Vector2(-.12f,.50f), new Vector2(-.50f,.23f) };
        var shoulders = new[] { .20f,.31f,.24f,.18f,.28f,.34f,.23f,.29f };
        var vertices = new System.Collections.Generic.List<Vector3>();
        var triangles = new System.Collections.Generic.List<int>();
        var uv = new System.Collections.Generic.List<Vector2>();
        void Triangle(Vector3 a, Vector3 b, Vector3 c)
        {
            int start = vertices.Count;
            vertices.Add(a); vertices.Add(b); vertices.Add(c);
            uv.Add(new Vector2(a.x+.5f,a.z+.5f)); uv.Add(new Vector2(b.x+.5f,b.z+.5f)); uv.Add(new Vector2(c.x+.5f,c.z+.5f));
            triangles.Add(start); triangles.Add(start+1); triangles.Add(start+2);
        }
        for (int i = 0; i < outline.Length; i++)
        {
            int next = (i+1)%outline.Length;
            var a = outline[i]; var b = outline[next];
            var lowA = new Vector3(a.x*.83f,-.5f,a.y*.83f);
            var lowB = new Vector3(b.x*.83f,-.5f,b.y*.83f);
            var rimA = new Vector3(a.x,-.10f,a.y); var rimB = new Vector3(b.x,-.10f,b.y);
            var topA = new Vector3(a.x*.69f+.035f,shoulders[i],a.y*.72f-.025f);
            var topB = new Vector3(b.x*.69f+.035f,shoulders[next],b.y*.72f-.025f);
            // Counterclockwise outline viewed from above: reverse top winding for +Y.
            Triangle(lowA,rimA,rimB); Triangle(lowA,rimB,lowB);
            Triangle(rimA,topA,topB); Triangle(rimA,topB,rimB);
            Triangle(topA,new Vector3(-.055f,.5f,.045f),topB);
            Triangle(lowA,lowB,new Vector3(0,-.5f,0));
        }
        mesh = new Mesh { name = "MeadowChamferedStone" };
        mesh.SetVertices(vertices); mesh.SetTriangles(triangles,0); mesh.SetUVs(0,uv);
        mesh.RecalculateNormals(); mesh.RecalculateBounds();
        AssetDatabase.CreateAsset(mesh,path);
        return mesh;
    }

    private static Material Material(string name, Color color)
    {
        string path = "Assets/C02/Art/" + name + ".mat";
        var m = AssetDatabase.LoadAssetAtPath<Material>(path);
        if (m == null) { m = new Material(Shader.Find("Universal Render Pipeline/Lit")); AssetDatabase.CreateAsset(m,path); }
        m.color = color; m.SetFloat("_Smoothness", .15f); EditorUtility.SetDirty(m); return m;
    }

    private static void Frame(RectTransform parent, Color fill)
    {
        var image = parent.GetComponent<UnityEngine.UI.Image>();
        if (image == null) { image = parent.gameObject.AddComponent<UnityEngine.UI.Image>(); image.raycastTarget = false; }
        image.color = fill;
        var old = parent.Find("FrameOrnaments"); if (old != null) Object.DestroyImmediate(old.gameObject);
        var root = new GameObject("FrameOrnaments", typeof(RectTransform), typeof(UnityEngine.UI.LayoutElement)).GetComponent<RectTransform>();
        root.SetParent(parent,false); root.GetComponent<UnityEngine.UI.LayoutElement>().ignoreLayout = true;
        root.anchorMin=Vector2.zero; root.anchorMax=Vector2.one; root.offsetMin=root.offsetMax=Vector2.zero;
        for(int side=0;side<4;side++)
        {
            var line=new GameObject("BrassEdge",typeof(RectTransform),typeof(UnityEngine.UI.Image)).GetComponent<RectTransform>(); line.SetParent(root,false);
            var edge=line.GetComponent<UnityEngine.UI.Image>(); edge.raycastTarget=false; edge.color=new Color(.41f,.34f,.23f,1f);
            line.anchorMin = side<2 ? new Vector2(0,side) : new Vector2(side-2,0);
            line.anchorMax = side<2 ? new Vector2(1,side) : new Vector2(side-2,1);
            line.sizeDelta = side<2 ? new Vector2(-8,1) : new Vector2(1,-8); line.anchoredPosition=Vector2.zero;
        }
        for(int i=0;i<4;i++)
        {
            var jewel=new GameObject("CornerStud",typeof(RectTransform),typeof(UnityEngine.UI.Image)).GetComponent<RectTransform>(); jewel.SetParent(root,false);
            jewel.anchorMin=jewel.anchorMax=new Vector2(i%2,i/2); jewel.sizeDelta=new Vector2(3,3);
            jewel.GetComponent<UnityEngine.UI.Image>().color=new Color(.57f,.47f,.30f); jewel.GetComponent<UnityEngine.UI.Image>().raycastTarget=false;
        }
    }

    // Original low polygon meshes authored through the Editor API; no imported models.
    private static GameObject Facet(string name, Transform parent, Vector3 position, Vector3 scale, Material material, float top = .7f)
    {
        string meshName=top==0 ? "PineCone8" : "ArmorTaper8";
        string path="Assets/C02/Art/"+meshName+".asset";
        var mesh=AssetDatabase.LoadAssetAtPath<Mesh>(path);
        if(mesh==null)
        {
            var vertices=new System.Collections.Generic.List<Vector3>(); var triangles=new System.Collections.Generic.List<int>();
            void Triangle(Vector3 a,Vector3 b,Vector3 c) { int start=vertices.Count; vertices.Add(a);vertices.Add(b);vertices.Add(c);triangles.Add(start);triangles.Add(start+1);triangles.Add(start+2); }
            for(int i=0;i<8;i++)
            {
                float a=i*Mathf.PI/4,b=(i+1)*Mathf.PI/4;
                var p=new Vector3(Mathf.Cos(a)*.5f,-.5f,Mathf.Sin(a)*.5f); var q=new Vector3(Mathf.Cos(b)*.5f,-.5f,Mathf.Sin(b)*.5f);
                var r=new Vector3(p.x*top,.5f,p.z*top); var s=new Vector3(q.x*top,.5f,q.z*top);
                Triangle(p,r,q); Triangle(q,r,s); Triangle(Vector3.down*.5f,p,q); if(top>0)Triangle(Vector3.up*.5f,s,r);
            }
            mesh=new Mesh {name=meshName};mesh.SetVertices(vertices);mesh.SetTriangles(triangles,0);mesh.RecalculateNormals();mesh.RecalculateBounds();AssetDatabase.CreateAsset(mesh,path);
        }
        var go=new GameObject(name,typeof(MeshFilter),typeof(MeshRenderer));go.transform.SetParent(parent,false);go.transform.localPosition=position;go.transform.localScale=scale;
        go.GetComponent<MeshFilter>().sharedMesh=mesh;go.GetComponent<MeshRenderer>().sharedMaterial=material;return go;
    }

    private static GameObject Shape(string name, PrimitiveType type, Transform parent, Vector3 position, Vector3 scale, Material mat)
    {
        var go = GameObject.CreatePrimitive(type); go.name = name; go.transform.SetParent(parent,false);
        go.transform.localPosition = position; go.transform.localScale = scale;
        go.GetComponent<Renderer>().sharedMaterial = mat;
        Object.DestroyImmediate(go.GetComponent<Collider>()); return go;
    }

    // V02: one authored adult guard. Proportions are measured before the display scale.
    private static void Hero(Transform parent, Vector3 position, float scale)
    {
        var root = new GameObject("V01Hero").transform; root.SetParent(parent,false); root.localPosition=position; root.localScale=Vector3.one*scale;
        var steel=Material("GuardSteel",new Color(.85f,.87f,.88f)); steel.SetFloat("_Metallic",.72f); steel.SetFloat("_Smoothness",.43f);
        steel.mainTexture=AssetDatabase.LoadAssetAtPath<Texture2D>("Assets/C02/Art/Textures/worn-steel-albedo-v1.png"); steel.mainTextureScale=new Vector2(2,2);
        var edge=Material("WornSteelEdge",new Color(.85f,.87f,.88f)); edge.SetFloat("_Metallic",.72f); edge.SetFloat("_Smoothness",.43f);
        edge.mainTexture=steel.mainTexture; edge.mainTextureScale=steel.mainTextureScale;
        var bronze=Material("AgedBronze",new Color(.40f,.31f,.19f)); bronze.SetFloat("_Metallic",.65f);
        var leather=Material("OiledLeather",new Color(.56f,.48f,.41f)); leather.SetFloat("_Metallic",0); leather.SetFloat("_Smoothness",.25f);
        leather.mainTexture=AssetDatabase.LoadAssetAtPath<Texture2D>("Assets/C02/Art/Textures/oiled-leather-albedo-v1.png"); leather.mainTextureScale=new Vector2(2,2);
        var cloth=Material("FadedCrimson",new Color(.29f,.075f,.065f)); cloth.SetFloat("_Cull",0);
        var chain=Material("Chainmail",new Color(.24f,.26f,.26f)); chain.SetFloat("_Metallic",.6f); chain.mainTexture=DetailTexture("MailLinks",2);
        var black=Material("VisorRecess",new Color(.035f,.039f,.038f));
        // Adult 7.3 head figure: crown2.05, head0.28, hip1.03, shoulders1.65.
        Profile("MailHauberk",root,chain,new[]{.88f,1.05f,1.25f,1.49f,1.65f},new[]{new Vector2(.23f,.145f),new Vector2(.19f,.13f),new Vector2(.19f,.135f),new Vector2(.265f,.16f),new Vector2(.25f,.12f)});
        Profile("Breastplate",root,steel,new[]{1.13f,1.20f,1.38f,1.54f,1.63f},new[]{new Vector2(.185f,.145f),new Vector2(.19f,.16f),new Vector2(.24f,.185f),new Vector2(.265f,.17f),new Vector2(.215f,.125f)});
        Profile("Gorget",root,edge,new[]{1.61f,1.66f,1.70f},new[]{new Vector2(.22f,.14f),new Vector2(.16f,.12f),new Vector2(.115f,.10f)});
        for(int i=0;i<3;i++) Profile("FauldPlate"+i,root,steel,new[]{1.12f-i*.065f,1.185f-i*.065f},new[]{new Vector2(.215f+i*.012f,.159f),new Vector2(.198f+i*.012f,.15f)});
        Profile("LeatherBelt",root,leather,new[]{1.065f,1.115f},new[]{new Vector2(.219f,.171f),new Vector2(.212f,.167f)});
        Shape("BeltBuckle",PrimitiveType.Cube,root,new Vector3(.02f,1.088f,-.174f),new Vector3(.065f,.048f,.018f),bronze);
        Profile("HelmetShell",root,steel,new[]{1.745f,1.82f,1.96f,2.025f,2.05f},new[]{new Vector2(.112f,.116f),new Vector2(.133f,.146f),new Vector2(.125f,.14f),new Vector2(.082f,.09f),new Vector2(.025f,.035f)});
        Profile("HelmetRolledRim",root,edge,new[]{1.748f,1.762f},new[]{new Vector2(.121f,.127f),new Vector2(.120f,.126f)});
        CurvedPlate("Visor",root,steel,new Vector3(0,1.85f,-.013f),.127f,.155f,.125f);
        Shape("EyeSlit",PrimitiveType.Cube,root,new Vector3(0,1.918f,-.164f),new Vector3(.186f,.017f,.013f),black);
        Shape("NasalRidge",PrimitiveType.Cube,root,new Vector3(0,1.88f,-.172f),new Vector3(.017f,.105f,.014f),edge);
        for(int i=0;i<5;i++) Shape("VisorVent",PrimitiveType.Cube,root,new Vector3((i-2)*.025f,1.835f,-.168f),new Vector3(.006f,.021f,.008f),black);
        // Shoulder/limb segments overlap at the natural joint instead of ball decorations.
        for(int sign=-1;sign<=1;sign+=2)
        {
            float s=sign;
            Limb("MailSleeve"+sign,root,new Vector3(s*.27f,1.62f,0),new Vector3(s*.365f,1.34f,.008f),.082f,.061f,chain);
            for(int plate=0;plate<3;plate++)
                CurvedPlate("ShoulderLame"+sign+plate,root,plate==0?edge:steel,new Vector3(s*(.275f+plate*.022f),1.62f-plate*.045f,0),.135f-plate*.012f,.13f,.075f);
            Limb("UpperArmPlate"+sign,root,new Vector3(s*.31f,1.53f,-.002f),new Vector3(s*.365f,1.36f,-.005f),.073f,.060f,steel);
            Limb("ElbowCop"+sign,root,new Vector3(s*.37f,1.37f,-.01f),new Vector3(s*.38f,1.30f,-.018f),.068f,.065f,edge);
            Limb("Vambrace"+sign,root,new Vector3(s*.38f,1.30f,-.024f),new Vector3(s*.365f,1.09f,-.05f),.065f,.043f,steel);
            Limb("GauntletCuff"+sign,root,new Vector3(s*.366f,1.125f,-.05f),new Vector3(s*.365f,1.075f,-.06f),.052f,.042f,edge);
            Limb("Glove"+sign,root,new Vector3(s*.365f,1.075f,-.065f),new Vector3(s*.363f,.985f,-.075f),.043f,.037f,leather);
            Limb("Trouser"+sign,root,new Vector3(s*.115f,1.025f,.01f),new Vector3(s*.133f,.59f,.01f),.107f,.075f,cloth);
            Limb("Cuissard"+sign,root,new Vector3(s*.13f,.95f,-.012f),new Vector3(s*.135f,.64f,-.015f),.093f,.072f,steel);
            Limb("KneeCop"+sign,root,new Vector3(s*.135f,.64f,-.03f),new Vector3(s*.142f,.55f,-.033f),.076f,.068f,edge);
            Limb("Greave"+sign,root,new Vector3(s*.142f,.55f,-.005f),new Vector3(s*.155f,.16f,.015f),.075f,.046f,steel);
            var boot=Profile("Boot"+sign,root,leather,new[]{.015f,.06f,.13f,.21f},new[]{new Vector2(.067f,.13f),new Vector2(.071f,.145f),new Vector2(.060f,.115f),new Vector2(.048f,.062f)});
            boot.transform.localPosition=new Vector3(s*.155f,0,-.035f);
            var toe=Profile("Sabatons"+sign,root,steel,new[]{.045f,.08f,.115f},new[]{new Vector2(.066f,.135f),new Vector2(.068f,.13f),new Vector2(.055f,.095f)});
            toe.transform.localPosition=new Vector3(s*.155f,0,-.06f);
            // Small rivets and leather fixing straps follow armor edges.
            for(int i=0;i<3;i++)
                Shape("FauldRivet",PrimitiveType.Sphere,root,new Vector3(s*.16f,1.14f-i*.065f,-.13f),Vector3.one*.012f,bronze);
            Shape("ThighStrap",PrimitiveType.Cube,root,new Vector3(s*.133f,.76f,.002f),new Vector3(.184f,.022f,.155f),leather);
        }
        ClothMesh("GuardCape",root,cloth,false);
        ClothMesh("SplitTabard",root,cloth,true);
        Shield(root,bronze,cloth,steel);
        // Thin diamond blade, believable grip and restrained crossguard.
        var blade=Profile("SwordBlade",root,edge,new[]{.12f,.18f,.86f,.90f},new[]{new Vector2(.001f,.001f),new Vector2(.022f,.006f),new Vector2(.027f,.007f),new Vector2(.025f,.008f)},4);
        blade.transform.localPosition=new Vector3(.40f,0,-.095f);
        Shape("SwordGuard",PrimitiveType.Cube,root,new Vector3(.40f,.923f,-.095f),new Vector3(.185f,.020f,.032f),steel);
        Limb("SwordGrip",root,new Vector3(.40f,.935f,-.095f),new Vector3(.40f,1.055f,-.095f),.015f,.017f,leather);
        Shape("SwordPommel",PrimitiveType.Sphere,root,new Vector3(.40f,1.071f,-.095f),Vector3.one*.042f,bronze);
        Shape("CapeClasp",PrimitiveType.Cylinder,root,new Vector3(.13f,1.62f,-.14f),new Vector3(.033f,.009f,.033f),bronze).transform.localRotation=Quaternion.Euler(90,0,0);
    }

    private static GameObject Profile(string name,Transform parent,Material mat,float[] heights,Vector2[] radii,int sides=20)
    {
        var v=new System.Collections.Generic.List<Vector3>();var t=new System.Collections.Generic.List<int>();var uv=new System.Collections.Generic.List<Vector2>();
        for(int j=0;j<heights.Length;j++)for(int i=0;i<=sides;i++)
        {float a=i*Mathf.PI*2/sides;v.Add(new Vector3(Mathf.Cos(a)*radii[j].x,heights[j],Mathf.Sin(a)*radii[j].y));uv.Add(new Vector2((float)i/sides,heights[j]*3));}
        for(int j=0;j<heights.Length-1;j++)for(int i=0;i<sides;i++)
        {int a=j*(sides+1)+i,b=a+sides+1;t.AddRange(new[]{a,b,a+1,a+1,b,b+1});}
        return MeshObject(name,parent,v,t,uv,mat);
    }

    private static void Limb(string name,Transform parent,Vector3 a,Vector3 b,float radiusA,float radiusB,Material mat)
    {
        var go=Profile(name,parent,mat,new[]{0f,Vector3.Distance(a,b)*.15f,Vector3.Distance(a,b)*.85f,Vector3.Distance(a,b)},new[]{new Vector2(radiusA*.93f,radiusA*.85f),new Vector2(radiusA,radiusA*.9f),new Vector2(radiusB,radiusB*.9f),new Vector2(radiusB*.9f,radiusB*.8f)},16);
        go.transform.localPosition=a;go.transform.localRotation=Quaternion.FromToRotation(Vector3.up,b-a);
    }

    private static void CurvedPlate(string name,Transform parent,Material mat,Vector3 position,float width,float depth,float height)
    {
        var v=new System.Collections.Generic.List<Vector3>();var t=new System.Collections.Generic.List<int>();var uv=new System.Collections.Generic.List<Vector2>();
        for(int y=0;y<3;y++)for(int i=0;i<=12;i++)
        {float a=Mathf.PI*.14f+Mathf.PI*.72f*i/12;v.Add(position+new Vector3(-Mathf.Cos(a)*width,(y/2f-.5f)*height,-Mathf.Sin(a)*depth*(1-.08f*y)));uv.Add(new Vector2(i/12f,y/2f));}
        for(int y=0;y<2;y++)for(int i=0;i<12;i++){int a=y*13+i;t.AddRange(new[]{a,a+13,a+1,a+1,a+13,a+14});}
        MeshObject(name,parent,v,t,uv,mat);
    }

    private static void ClothMesh(string name,Transform parent,Material mat,bool tabard)
    {
        var v=new System.Collections.Generic.List<Vector3>();var t=new System.Collections.Generic.List<int>();var uv=new System.Collections.Generic.List<Vector2>();
        int columns=20,rows=12;
        for(int j=0;j<=rows;j++)for(int i=0;i<=columns;i++)
        {
            float u=i/(float)columns,f=j/(float)rows;float width=tabard?.31f:Mathf.Lerp(.40f,.66f,f);
            float y=tabard?Mathf.Lerp(1.11f,.66f,f):Mathf.Lerp(1.64f,.39f,f);
            float z=tabard?-.18f:-Mathf.Lerp(-.14f,-.28f,f);
            z+=Mathf.Sin(u*25+f*.8f)*(.007f+f*.018f);
            if(tabard&&Mathf.Abs(u-.5f)<.08f)y+=f*.13f;
            v.Add(new Vector3((u-.5f)*width,y,z));uv.Add(new Vector2(u,f));
        }
        for(int j=0;j<rows;j++)for(int i=0;i<columns;i++){int a=j*(columns+1)+i;t.AddRange(new[]{a,a+1,a+columns+1,a+1,a+columns+2,a+columns+1});}
        MeshObject(name,parent,v,t,uv,mat);
    }

    private static void Shield(Transform parent,Material rim,Material face,Material metal)
    {
        Vector2[] outline={new Vector2(-.19f,.29f),new Vector2(.19f,.29f),new Vector2(.20f,.03f),new Vector2(.14f,-.18f),new Vector2(0,-.34f),new Vector2(-.14f,-.18f),new Vector2(-.20f,.03f)};
        for(int layer=0;layer<2;layer++)
        {
            var v=new System.Collections.Generic.List<Vector3>();var t=new System.Collections.Generic.List<int>();var uv=new System.Collections.Generic.List<Vector2>();
            v.Add(new Vector3(-.435f,1.19f,-.19f-layer*.016f));uv.Add(new Vector2(.5f,.5f));
            foreach(var p in outline){float s=layer==0?1f:.90f;v.Add(new Vector3(-.435f+p.x*s,1.19f+p.y*s,-.17f-layer*.02f+Mathf.Abs(p.x)*.16f));uv.Add(p+Vector2.one*.5f);}
            for(int i=0;i<outline.Length;i++)t.AddRange(new[]{0,1+i,1+(i+1)%outline.Length});
            MeshObject(layer==0?"ShieldForgedRim":"ShieldClothInset",parent,v,t,uv,layer==0?rim:face);
        }
        Shape("ShieldInsignia",PrimitiveType.Cube,parent,new Vector3(-.435f,1.21f,-.218f),new Vector3(.025f,.22f,.009f),rim);
        Shape("ShieldInsigniaBar",PrimitiveType.Cube,parent,new Vector3(-.435f,1.25f,-.218f),new Vector3(.13f,.023f,.009f),rim);
    }

    private static GameObject MeshObject(string name,Transform parent,System.Collections.Generic.List<Vector3> vertices,System.Collections.Generic.List<int> triangles,System.Collections.Generic.List<Vector2> uv,Material mat)
    {
        string path="Assets/C02/Art/V02_"+name+".asset";var mesh=AssetDatabase.LoadAssetAtPath<Mesh>(path);
        if(mesh==null){mesh=new Mesh{name="V02_"+name};AssetDatabase.CreateAsset(mesh,path);}else mesh.Clear();
        mesh.SetVertices(vertices);mesh.SetTriangles(triangles,0);mesh.SetUVs(0,uv);mesh.RecalculateNormals();mesh.RecalculateBounds();EditorUtility.SetDirty(mesh);
        var go=new GameObject(name,typeof(MeshFilter),typeof(MeshRenderer));go.transform.SetParent(parent,false);go.GetComponent<MeshFilter>().sharedMesh=mesh;go.GetComponent<MeshRenderer>().sharedMaterial=mat;return go;
    }

    private static Texture2D DetailTexture(string name,int kind)
    {
        string path="Assets/C02/Art/V02_"+name+".asset";var texture=AssetDatabase.LoadAssetAtPath<Texture2D>(path);
        int size=kind==0?512:128;bool created=texture==null;if(created)texture=new Texture2D(size,size,TextureFormat.RGBA32,true){name=name,wrapMode=TextureWrapMode.Repeat,filterMode=FilterMode.Trilinear,anisoLevel=4};
        var pixels=new Color[size*size];
        for(int y=0;y<size;y++)for(int x=0;x<size;x++)
        {
            float noise=Mathf.PerlinNoise(x*.11f+5,y*.11f+7)*.6f+Mathf.PerlinNoise(x*.37f,y*.37f)*.4f;
            Color color;
            if(kind==2)
            {float dx=(x%16-8)/8f,dy=(y%12-6)/6f;float ring=Mathf.Abs(Mathf.Sqrt(dx*dx+dy*dy)-.68f);float n=ring<.15f?.62f:.23f;color=new Color(n,n,n);}
            else if(kind==1)color=Color.Lerp(new Color(.75f,.74f,.69f),new Color(.95f,.94f,.89f),noise);
            else
            {float xx=(x/(float)size-.5f)*64,zz=(y/(float)size-.5f)*64;float pathWidth=2.5f+Mathf.Sin(zz*.32f)*.4f;float edge=Mathf.SmoothStep(0,1,(Mathf.Abs(xx)-pathWidth+noise*.8f)*2);color=Color.Lerp(new Color(.40f,.34f,.23f),new Color(.23f,.32f,.14f),edge);color*=.79f+noise*.44f;}
            pixels[y*size+x]=color;
        }
        texture.SetPixels(pixels);texture.Apply(true,false);if(created)AssetDatabase.CreateAsset(texture,path);else EditorUtility.SetDirty(texture);return texture;
    }

    private static void Outpost(Transform parent, Vector3 origin)
    {
        var root=new GameObject("V01Outpost").transform;root.SetParent(parent,false);root.localPosition=origin;
        var stone=Material("WeatheredLimestone",new Color(.78f,.76f,.68f));stone.mainTexture=DetailTexture("StoneGrain",1);stone.mainTextureScale=new Vector2(2,2);
        var darkStone=Material("FoundationStone",new Color(.47f,.48f,.42f));darkStone.mainTexture=stone.mainTexture;
        var earth=Material("EarthAndGrass",Color.white);earth.mainTexture=DetailTexture("GroundBlend",0);
        var wood=Material("PineBark",new Color(.16f,.115f,.075f));var leaf=Material("PineNeedles",new Color(.17f,.26f,.16f));var moss=Material("Moss",new Color(.30f,.35f,.18f));leaf.SetFloat("_Cull",0);moss.SetFloat("_Cull",0);
        var banner=Material("FadedCrimson",new Color(.29f,.075f,.065f));
        var ground=Shape("TexturedGround",PrimitiveType.Plane,root,new Vector3(0,-.022f,0),new Vector3(6.4f,1,6.4f),earth);
        // Mortared walls have staggered joints and depth; they do not define server collision.
        for(int side=-1;side<=1;side+=2)
        {
            for(int row=0;row<8;row++)for(int block=0;block<9;block++)
            {
                float x=side*(4.1f+block*.94f)+(row%2)*.43f;
                var go=Rock("WallStone"+side+"_"+row+"_"+block,root,new Vector3(x,.26f+row*.50f,7.4f),new Vector3(.925f,.48f,.94f),stone,block+row*7);
                go.transform.localRotation=Quaternion.Euler(0,(block%3-1)*.4f,0);
            }
            for(int row=0;row<7;row++)Rock("GatePier"+side+row,root,new Vector3(side*3.55f,.35f+row*.65f,7.3f),new Vector3(1.4f,.61f,1.75f),row%2==0?stone:darkStone,row+3);
            for(int b=0;b<5;b++)Rock("Crenellation"+side+b,root,new Vector3(side*(4.4f+b*1.75f),4.5f,7.4f),new Vector3(.9f,.85f,1.1f),stone,b+19);
            Shape("BannerPole",PrimitiveType.Cylinder,root,new Vector3(side*4.2f,3.2f,6.7f),new Vector3(.055f,1.2f,.055f),wood);
            Banner(root,new Vector3(side*4.2f,3.45f,6.55f),banner,side);
        }
        for(int i=0;i<13;i++)
        {
            float a=Mathf.PI*i/12;var block=Rock("ArchStone"+i,root,new Vector3(Mathf.Cos(a)*3.55f,4.15f+Mathf.Sin(a)*1.85f,7.3f),new Vector3(.90f,.68f,1.70f),stone,i+5);
            block.transform.localRotation=Quaternion.Euler(0,0,a*Mathf.Rad2Deg-90);
        }
        // Individually irregular roadside outcrops instead of repeated green beads.
        for(int i=0;i<14;i++)
        {float side=i%2==0?-1:1;float z=-8+(i/2)*3.1f;Rock("RoadsideRock"+i,root,new Vector3(side*(4.7f+(i%4)*.51f),.19f,z),new Vector3(.7f+(i%3)*.4f,.44f+(i%2)*.3f,.65f+(i%4)*.2f),i%4==0?moss:darkStone,i*11);}
        for(int i=0;i<13;i++)
        {float side=i%2==0?-1:1;float z=-9+(i/2)*5.0f;Pine(root,new Vector3(side*(8+(i%4)*1.4f),0,z),.75f+(i%4)*.17f,i,wood,leaf);}
        for(int i=0;i<90;i++)
        {float z=-12+(i/2)*.68f;float side=i%2==0?-1:1;float x=side*(2.9f+Mathf.PerlinNoise(i*.54f,9)*3.5f);Grass(root,new Vector3(x,0,z),moss,i);}
        Ridge(root,new Vector3(0,0,35),Material("MountainHaze",new Color(.43f,.51f,.48f)));
    }

    private static GameObject Rock(string name,Transform parent,Vector3 p,Vector3 scale,Material mat,int seed)
    {
        var v=new System.Collections.Generic.List<Vector3>();var t=new System.Collections.Generic.List<int>();var uv=new System.Collections.Generic.List<Vector2>();
        // Chamfered uneven cuboid: the silhouette is stone, not an eight-sided cone.
        var corners=new[]{new Vector3(-.5f,-.5f,-.5f),new Vector3(.5f,-.5f,-.5f),new Vector3(.5f,-.5f,.5f),new Vector3(-.5f,-.5f,.5f),new Vector3(-.48f,.5f,-.48f),new Vector3(.48f,.5f,-.48f),new Vector3(.48f,.5f,.48f),new Vector3(-.48f,.5f,.48f)};
        int[][] faces={new[]{0,4,5,1},new[]{1,5,6,2},new[]{2,6,7,3},new[]{3,7,4,0},new[]{4,7,6,5},new[]{3,0,1,2}};
        foreach(var face in faces)
        {int start=v.Count;for(int i=0;i<4;i++){var c=corners[face[i]];c+=new Vector3(Mathf.Sin(seed+face[i]*1.7f),Mathf.Cos(seed*.3f+face[i]*2.3f),Mathf.Sin(seed*.7f+face[i]))*.012f;v.Add(c);uv.Add(new Vector2(i==1||i==2?1:0,i>1?1:0));}t.AddRange(new[]{start,start+1,start+2,start,start+2,start+3});}
        var go=MeshObject(name,parent,v,t,uv,mat);go.transform.localPosition=p;go.transform.localScale=scale;return go;
    }

    private static void Pine(Transform parent,Vector3 p,float scale,int seed,Material bark,Material needle)
    {
        var root=new GameObject("Pine"+seed).transform;root.SetParent(parent,false);root.localPosition=p;root.localScale=Vector3.one*scale;
        Limb("PineTrunk"+seed,root,Vector3.zero,new Vector3(.12f,6.4f,.05f),.12f,.025f,bark);
        var v=new System.Collections.Generic.List<Vector3>();var t=new System.Collections.Generic.List<int>();var uv=new System.Collections.Generic.List<Vector2>();
        for(int tier=0;tier<11;tier++)for(int branch=0;branch<7;branch++)
        {
            float a=branch*Mathf.PI*2/7+tier*1.7f+seed;float y=1.2f+tier*.44f;float length=(1-tier/12f)*(1.5f+.18f*Mathf.Sin(branch*3+seed));
            Vector3 center=new Vector3(0,y,0),tip=new Vector3(Mathf.Cos(a)*length,y-.27f,Mathf.Sin(a)*length);
            Vector3 tangent=new Vector3(-Mathf.Sin(a),0,Mathf.Cos(a))*length*.48f;
            Vector3 shoulder=Vector3.Lerp(center,tip,.60f);
            int n=v.Count;v.Add(center+Vector3.up*.17f);v.Add(shoulder-tangent);v.Add(tip);v.Add(shoulder+tangent);v.Add(shoulder+Vector3.up*.27f);
            uv.AddRange(new[]{Vector2.up,Vector2.zero,Vector2.right,Vector2.one,Vector2.one*.5f});
            t.AddRange(new[]{n,n+1,n+4,n+1,n+2,n+4,n+2,n+3,n+4,n+3,n,n+4});
        }
        MeshObject("PineBranches"+seed,root,v,t,uv,needle);
    }

    private static void Grass(Transform parent,Vector3 p,Material mat,int seed)
    {
        var v=new System.Collections.Generic.List<Vector3>();var t=new System.Collections.Generic.List<int>();var uv=new System.Collections.Generic.List<Vector2>();
        for(int i=0;i<5;i++){float a=i*2.4f+seed;Vector3 offset=new Vector3(Mathf.Sin(a)*.12f,0,Mathf.Cos(a)*.12f);int n=v.Count;v.Add(offset+Vector3.left*.018f);v.Add(offset+Vector3.right*.018f);v.Add(offset+new Vector3(Mathf.Cos(a)*.05f,.17f+(seed%4)*.025f,0));uv.AddRange(new[]{Vector2.zero,Vector2.right,Vector2.up});t.AddRange(new[]{n,n+2,n+1});}
        var go=MeshObject("GrassClump"+seed,parent,v,t,uv,mat);go.transform.localPosition=p;
    }

    private static void Banner(Transform parent,Vector3 p,Material mat,int side)
    {
        var v=new System.Collections.Generic.List<Vector3>();var t=new System.Collections.Generic.List<int>();var uv=new System.Collections.Generic.List<Vector2>();
        for(int y=0;y<9;y++)for(int x=0;x<7;x++){float u=x/6f,f=y/8f;v.Add(p+new Vector3((u-.5f)*.8f,-f*1.5f,.08f*Mathf.Sin(u*10+f)*f));uv.Add(new Vector2(u,f));}
        for(int y=0;y<8;y++)for(int x=0;x<6;x++){int a=y*7+x;t.AddRange(new[]{a,a+1,a+7,a+1,a+8,a+7});}
        MeshObject("GateBanner"+side,parent,v,t,uv,mat);
    }

    private static void Ridge(Transform parent,Vector3 p,Material mat)
    {
        var v=new System.Collections.Generic.List<Vector3>();var t=new System.Collections.Generic.List<int>();var uv=new System.Collections.Generic.List<Vector2>();
        for(int row=0;row<9;row++)for(int x=0;x<=64;x++)
        {float xx=-55+x*1.7f,z=row*3.1f;float ridge=(3+Mathf.PerlinNoise(xx*.045f+10,row*.12f)*10)*Mathf.Sin(Mathf.PI*row/9);v.Add(new Vector3(xx,ridge,z));uv.Add(new Vector2(x/64f,row/8f));}
        for(int row=0;row<8;row++)for(int x=0;x<64;x++){int a=row*65+x;t.AddRange(new[]{a,a+65,a+1,a+1,a+65,a+66});}
        var go=MeshObject("RollingDistantRidge",parent,v,t,uv,mat);go.transform.localPosition=p;
    }
    public static void ApplyC04()
    {
        BuildC04Assets();
        BindC04();
        EditorSceneManager.MarkSceneDirty(EditorSceneManager.GetActiveScene());
        EditorSceneManager.SaveScene(EditorSceneManager.GetActiveScene());
        AssetDatabase.SaveAssets();
    }

    private static void BindC04()
    {
        if(EditorApplication.isPlaying)throw new System.InvalidOperationException("C04 binding requires Edit Mode.");
        const string folder="Assets/C02/Art/C04/";
        if(AssetDatabase.LoadAssetAtPath<GameObject>(folder+"ObservedPlayer.prefab")==null ||
           AssetDatabase.LoadAssetAtPath<GameObject>(folder+"MonsterType0.prefab")==null ||
           AssetDatabase.LoadAssetAtPath<GameObject>(folder+"MonsterType1.prefab")==null ||
           AssetDatabase.LoadAssetAtPath<GameObject>(folder+"MonsterType2.prefab")==null)BuildC04Assets();
        var worldScreen=Object.FindFirstObjectByType<C02WorldScreen>();
        var worldRoot=((GameObject)new SerializedObject(worldScreen).FindProperty("worldRoot").objectReferenceValue).transform;
        var display=worldRoot.Find("C04Observations");
        if(display==null){display=new GameObject("C04Observations").transform;display.SetParent(worldRoot,false);}
        var observed=worldScreen.GetComponent<C04ObservedWorld>()??worldScreen.gameObject.AddComponent<C04ObservedWorld>();
        var refs=new SerializedObject(observed);
        refs.FindProperty("loginScreen").objectReferenceValue=Object.FindFirstObjectByType<C01LoginScreen>();
        refs.FindProperty("worldScreen").objectReferenceValue=worldScreen;
        refs.FindProperty("displayRoot").objectReferenceValue=display;
        refs.FindProperty("playerPrefab").objectReferenceValue=AssetDatabase.LoadAssetAtPath<GameObject>(folder+"ObservedPlayer.prefab");
        var types=refs.FindProperty("monsterPrefabs");types.arraySize=3;
        for(int type=0;type<3;type++)types.GetArrayElementAtIndex(type).objectReferenceValue=AssetDatabase.LoadAssetAtPath<GameObject>(folder+"MonsterType"+type+".prefab");
        refs.ApplyModifiedPropertiesWithoutUndo();observed.enabled=true;
    }

    // C04 visuals are independent prefabs. This does not recreate or modify the V02 own hero.
    public static void BuildC04Assets()
    {
        if(EditorApplication.isPlaying)throw new System.InvalidOperationException("C04 authoring requires Edit Mode.");
        if(!AssetDatabase.IsValidFolder("Assets/C02/Art/C04"))AssetDatabase.CreateFolder("Assets/C02/Art","C04");
        var worldScreen=Object.FindFirstObjectByType<C02WorldScreen>();
        var ownRef=new SerializedObject(worldScreen).FindProperty("ownCharacter").objectReferenceValue as Transform;
        var hero=ownRef.Find("V01Hero");
        if(hero==null)throw new System.InvalidOperationException("The existing V02 own-character visual is required.");
        var player=new GameObject("ObservedPlayer");
        var copy=Object.Instantiate(hero.gameObject,player.transform,false);copy.name="GuardVisual";copy.transform.localPosition=Vector3.zero;copy.transform.localRotation=Quaternion.identity;
        PrefabUtility.SaveAsPrefabAsset(player,"Assets/C02/Art/C04/ObservedPlayer.prefab");Object.DestroyImmediate(player);
        for(int type=0;type<3;type++)
        {
            var monster=new GameObject("MonsterType"+type);
            if(type==0)C04Goblin(monster.transform);else if(type==1)C04Orc(monster.transform);else C04Golem(monster.transform);
            PrefabUtility.SaveAsPrefabAsset(monster,"Assets/C02/Art/C04/MonsterType"+type+".prefab");Object.DestroyImmediate(monster);
        }
        AssetDatabase.SaveAssets();
    }

    private static Material C04Material(string name,Color color,float metallic=0)
    {
        string path="Assets/C02/Art/C04/"+name+".mat";var mat=AssetDatabase.LoadAssetAtPath<Material>(path);
        if(mat==null){mat=new Material(Shader.Find("Universal Render Pipeline/Lit"));AssetDatabase.CreateAsset(mat,path);}
        mat.color=color;mat.SetFloat("_Metallic",metallic);mat.SetFloat("_Smoothness",metallic>0?.28f:.14f);EditorUtility.SetDirty(mat);return mat;
    }

    private static GameObject C04Body(string name,Transform parent,Material mat,Vector3[] centers,Vector2[] radii,int sides=16,bool stone=false)
    {
        var vertices=new System.Collections.Generic.List<Vector3>();var triangles=new System.Collections.Generic.List<int>();var uv=new System.Collections.Generic.List<Vector2>();
        for(int ring=0;ring<centers.Length;ring++)for(int side=0;side<=sides;side++)
        {
            float angle=side*Mathf.PI*2/sides;float uneven=stone?1+.09f*Mathf.Sin(side*4.1f+ring*2.3f):1;
            vertices.Add(centers[ring]+new Vector3(Mathf.Cos(angle)*radii[ring].x*uneven,0,Mathf.Sin(angle)*radii[ring].y*uneven));uv.Add(new Vector2(side/(float)sides,ring/(float)(centers.Length-1)));
        }
        for(int ring=0;ring<centers.Length-1;ring++)for(int side=0;side<sides;side++){int a=ring*(sides+1)+side,b=a+sides+1;triangles.AddRange(new[]{a,b,a+1,a+1,b,b+1});}
        return C04Mesh(name,parent,mat,vertices,triangles,uv,stone);
    }

    private static void C04Limb(string name,Transform parent,Material mat,Vector3 start,Vector3 end,float top,float bottom,bool stone=false)
    {
        float length=Vector3.Distance(start,end);
        var go=C04Body(name,parent,mat,new[]{Vector3.zero,Vector3.up*length*.16f,Vector3.up*length*.52f,Vector3.up*length*.86f,Vector3.up*length},new[]{new Vector2(top*.70f,top*.65f),new Vector2(top,top*.85f),new Vector2((top+bottom)*.55f,(top+bottom)*.45f),new Vector2(bottom,bottom*.8f),new Vector2(bottom*.68f,bottom*.60f)},stone?7:14,stone);
        go.transform.localPosition=start;go.transform.localRotation=Quaternion.FromToRotation(Vector3.up,end-start);
    }

    private static GameObject C04Mesh(string name,Transform parent,Material mat,System.Collections.Generic.List<Vector3> vertices,System.Collections.Generic.List<int> triangles,System.Collections.Generic.List<Vector2> uv,bool flat=false)
    {
        if(flat)
        {
            var flatVertices=new System.Collections.Generic.List<Vector3>();var flatUv=new System.Collections.Generic.List<Vector2>();
            for(int i=0;i<triangles.Count;i++){flatVertices.Add(vertices[triangles[i]]);flatUv.Add(uv[triangles[i]]);triangles[i]=i;}vertices=flatVertices;uv=flatUv;
        }
        string path="Assets/C02/Art/C04/"+name+".asset";var mesh=AssetDatabase.LoadAssetAtPath<Mesh>(path);
        if(mesh==null){mesh=new Mesh{name=name};AssetDatabase.CreateAsset(mesh,path);}else mesh.Clear();
        mesh.SetVertices(vertices);mesh.SetTriangles(triangles,0);mesh.SetUVs(0,uv);mesh.RecalculateNormals();mesh.RecalculateBounds();EditorUtility.SetDirty(mesh);
        var go=new GameObject(name,typeof(MeshFilter),typeof(MeshRenderer));go.transform.SetParent(parent,false);go.GetComponent<MeshFilter>().sharedMesh=mesh;go.GetComponent<MeshRenderer>().sharedMaterial=mat;return go;
    }

    private static void C04Ear(string name,Transform parent,Material skin,Vector3 center,float side,float length)
    {
        var v=new System.Collections.Generic.List<Vector3>{center+new Vector3(0,-.07f,0),center+new Vector3(side*length,.045f,.015f),center+new Vector3(0,.08f,.01f),center+new Vector3(side*length*.24f,.015f,-.045f)};
        var t=new System.Collections.Generic.List<int>{0,2,3,2,1,3,1,0,3,0,1,2};
        C04Mesh(name,parent,skin,v,t,new System.Collections.Generic.List<Vector2>{Vector2.zero,Vector2.right,Vector2.up,Vector2.one});
    }

    private static void C04Face(Transform parent,string prefix,Material skin,Material recess,float y,float width,float depth)
    {
        C04Body(prefix+"Muzzle",parent,skin,new[]{new Vector3(0,y-.07f,-depth*.7f),new Vector3(0,y-.01f,-depth),new Vector3(0,y+.025f,-depth*.9f)},new[]{new Vector2(width*.58f,.025f),new Vector2(width*.75f,.055f),new Vector2(width*.55f,.035f)},12);
        for(int side=-1;side<=1;side+=2)
        {
            Shape(prefix+"EyeRecess",PrimitiveType.Cube,parent,new Vector3(side*width*.52f,y+.085f,-depth*.98f),new Vector3(width*.32f,.017f,.014f),recess).transform.localRotation=Quaternion.Euler(0,0,side*12);
            C04Limb(prefix+"Brow"+side,parent,skin,new Vector3(side*width*.14f,y+.118f,-depth),new Vector3(side*width*.85f,y+.126f,-depth*.77f),.025f,.018f);
        }
        C04Body(prefix+"Nose",parent,skin,new[]{new Vector3(0,y+.02f,-depth*1.15f),new Vector3(0,y+.09f,-depth*1.13f),new Vector3(0,y+.14f,-depth*.88f)},new[]{new Vector2(width*.20f,.035f),new Vector2(width*.13f,.026f),new Vector2(width*.10f,.015f)},10);
    }

    private static void C04Goblin(Transform parent)
    {
        var skin=C04Material("GoblinAshOlive",new Color(.27f,.32f,.21f));var leather=C04Material("ScavengedLeather",new Color(.20f,.135f,.085f));
        var rag=C04Material("TornUmberCloth",new Color(.28f,.22f,.14f));var iron=C04Material("DarkForgedIron",new Color(.25f,.27f,.26f),.45f);var dark=C04Material("FaceRecess",new Color(.055f,.048f,.035f));
        C04Body("GoblinTorso",parent,skin,new[]{new Vector3(0,.66f,0),new Vector3(0,.80f,.005f),new Vector3(0,1.04f,-.055f),new Vector3(0,1.14f,-.075f),new Vector3(0,1.18f,-.095f)},new[]{new Vector2(.13f,.09f),new Vector2(.12f,.08f),new Vector2(.18f,.10f),new Vector2(.19f,.10f),new Vector2(.105f,.07f)});
        C04Body("GoblinHead",parent,skin,new[]{new Vector3(0,1.15f,-.10f),new Vector3(0,1.22f,-.135f),new Vector3(0,1.34f,-.13f),new Vector3(0,1.44f,-.11f),new Vector3(0,1.48f,-.10f)},new[]{new Vector2(.065f,.065f),new Vector2(.10f,.10f),new Vector2(.125f,.115f),new Vector2(.098f,.10f),new Vector2(.025f,.025f)});
        C04Ear("GoblinEarL",parent,skin,new Vector3(-.10f,1.34f,-.11f),-1,.17f);C04Ear("GoblinEarR",parent,skin,new Vector3(.10f,1.34f,-.11f),1,.17f);
        C04Face(parent,"Goblin",skin,dark,1.24f,.11f,.235f);
        C04Body("GoblinRagSkirt",parent,rag,new[]{new Vector3(0,.57f,.005f),new Vector3(0,.67f,0),new Vector3(0,.77f,0)},new[]{new Vector2(.175f,.112f),new Vector2(.15f,.10f),new Vector2(.135f,.09f)});
        C04Body("GoblinBelt",parent,leather,new[]{new Vector3(0,.72f,0),new Vector3(0,.756f,0)},new[]{new Vector2(.147f,.107f),new Vector2(.143f,.103f)});
        for(int side=-1;side<=1;side+=2)
        {
            float s=side;C04Limb("GoblinThigh"+side,parent,skin,new Vector3(s*.09f,.69f,.015f),new Vector3(s*.14f,.39f,-.045f),.08f,.055f);
            C04Limb("GoblinShin"+side,parent,skin,new Vector3(s*.14f,.39f,-.045f),new Vector3(s*.13f,.105f,.025f),.057f,.036f);
            C04Body("GoblinBoot"+side,parent,leather,new[]{new Vector3(s*.13f,.015f,-.04f),new Vector3(s*.13f,.065f,-.04f),new Vector3(s*.13f,.13f,.015f)},new[]{new Vector2(.054f,.105f),new Vector2(.057f,.11f),new Vector2(.04f,.045f)});
            C04Limb("GoblinUpperArm"+side,parent,skin,new Vector3(s*.19f,1.10f,-.05f),new Vector3(s*.25f,.91f,-.08f),.058f,.044f);
            C04Limb("GoblinForearm"+side,parent,skin,new Vector3(s*.25f,.91f,-.08f),new Vector3(s*.28f,.73f,-.13f),.046f,.031f);
            C04Limb("GoblinWristWrap"+side,parent,leather,new Vector3(s*.27f,.78f,-.12f),new Vector3(s*.28f,.72f,-.13f),.038f,.035f);
            C04Limb("GoblinHand"+side,parent,skin,new Vector3(s*.28f,.72f,-.13f),new Vector3(s*.285f,.655f,-.145f),.035f,.03f);
        }
        C04Limb("GoblinChestStrap",parent,leather,new Vector3(-.11f,1.10f,-.135f),new Vector3(.11f,.78f,-.10f),.026f,.025f);
        C04Limb("GoblinDaggerGrip",parent,leather,new Vector3(.29f,.69f,-.145f),new Vector3(.29f,.61f,-.145f),.016f,.016f);
        C04Body("GoblinDagger",parent,iron,new[]{new Vector3(.29f,.36f,-.145f),new Vector3(.29f,.41f,-.145f),new Vector3(.29f,.61f,-.145f)},new[]{new Vector2(.001f,.001f),new Vector2(.027f,.005f),new Vector2(.025f,.006f)},4);
    }

    private static void C04Orc(Transform parent)
    {
        var skin=C04Material("OrcWeatheredOlive",new Color(.29f,.28f,.20f));var leather=C04Material("ScavengedLeather",new Color(.20f,.135f,.085f));var iron=C04Material("DarkForgedIron",new Color(.25f,.27f,.26f),.45f);var rag=C04Material("TornUmberCloth",new Color(.28f,.22f,.14f));var dark=C04Material("FaceRecess",new Color(.055f,.048f,.035f));var bone=C04Material("AgedBone",new Color(.62f,.57f,.42f));
        C04Body("OrcTorso",parent,skin,new[]{new Vector3(0,.99f,0),new Vector3(0,1.16f,0),new Vector3(0,1.48f,-.005f),new Vector3(0,1.70f,-.035f),new Vector3(0,1.82f,-.06f)},new[]{new Vector2(.21f,.15f),new Vector2(.23f,.16f),new Vector2(.34f,.20f),new Vector2(.37f,.22f),new Vector2(.21f,.14f)});
        C04Body("OrcHead",parent,skin,new[]{new Vector3(0,1.78f,-.07f),new Vector3(0,1.88f,-.11f),new Vector3(0,2.03f,-.11f),new Vector3(0,2.15f,-.08f),new Vector3(0,2.20f,-.065f)},new[]{new Vector2(.105f,.10f),new Vector2(.15f,.13f),new Vector2(.16f,.14f),new Vector2(.125f,.12f),new Vector2(.04f,.035f)});
        C04Ear("OrcEarL",parent,skin,new Vector3(-.14f,2.04f,-.07f),-1,.12f);C04Ear("OrcEarR",parent,skin,new Vector3(.14f,2.04f,-.07f),1,.12f);
        C04Face(parent,"Orc",skin,dark,1.90f,.15f,.26f);
        for(int s=-1;s<=1;s+=2)
        {
            C04Body("OrcTusk"+s,parent,bone,new[]{new Vector3(s*.075f,1.88f,-.30f),new Vector3(s*.071f,1.94f,-.315f),new Vector3(s*.064f,1.965f,-.30f)},new[]{new Vector2(.015f,.016f),new Vector2(.01f,.009f),new Vector2(.001f,.001f)},8);
            C04Limb("OrcThigh"+s,parent,skin,new Vector3(s*.15f,1.03f,.01f),new Vector3(s*.20f,.59f,-.025f),.145f,.105f);
            C04Limb("OrcShin"+s,parent,skin,new Vector3(s*.20f,.59f,-.025f),new Vector3(s*.19f,.15f,.02f),.105f,.063f);
            C04Body("OrcBoot"+s,parent,leather,new[]{new Vector3(s*.19f,.015f,-.06f),new Vector3(s*.19f,.08f,-.06f),new Vector3(s*.19f,.23f,.015f)},new[]{new Vector2(.09f,.15f),new Vector2(.093f,.155f),new Vector2(.073f,.077f)});
            C04Limb("OrcUpperArm"+s,parent,skin,new Vector3(s*.34f,1.68f,-.02f),new Vector3(s*.49f,1.40f,-.025f),.12f,.085f);
            C04Limb("OrcForearm"+s,parent,skin,new Vector3(s*.49f,1.40f,-.025f),new Vector3(s*.52f,1.12f,-.08f),.098f,.061f);
            C04Limb("OrcBracer"+s,parent,leather,new Vector3(s*.515f,1.24f,-.065f),new Vector3(s*.52f,1.11f,-.08f),.083f,.071f);
            C04Limb("OrcHand"+s,parent,skin,new Vector3(s*.52f,1.11f,-.08f),new Vector3(s*.52f,1.00f,-.095f),.069f,.06f);
        }
        C04Body("OrcRagSkirt",parent,rag,new[]{new Vector3(0,.82f,0),new Vector3(0,1.05f,0)},new[]{new Vector2(.28f,.19f),new Vector2(.235f,.16f)});
        C04Body("OrcBelt",parent,leather,new[]{new Vector3(0,1.035f,0),new Vector3(0,1.095f,0)},new[]{new Vector2(.25f,.18f),new Vector2(.246f,.178f)});
        C04Limb("OrcBaldric",parent,leather,new Vector3(-.24f,1.70f,-.20f),new Vector3(.18f,1.10f,-.17f),.04f,.038f);
        var pauldron=C04Body("OrcPauldron",parent,iron,new[]{new Vector3(-.36f,1.56f,0),new Vector3(-.36f,1.70f,0),new Vector3(-.36f,1.79f,0)},new[]{new Vector2(.15f,.19f),new Vector2(.20f,.22f),new Vector2(.09f,.10f)},10);
        C04Limb("OrcAxeShaft",parent,leather,new Vector3(.53f,.12f,-.10f),new Vector3(.53f,1.34f,-.10f),.022f,.023f);
        var v=new System.Collections.Generic.List<Vector3>{new Vector3(.52f,1.37f,-.13f),new Vector3(.84f,1.40f,-.13f),new Vector3(.88f,1.10f,-.13f),new Vector3(.55f,1.13f,-.13f),new Vector3(.53f,1.25f,-.075f)};
        C04Mesh("OrcAxeHead",parent,iron,v,new System.Collections.Generic.List<int>{0,1,4,1,2,4,2,3,4,3,0,4,0,3,2,0,2,1},new System.Collections.Generic.List<Vector2>{Vector2.zero,Vector2.up,Vector2.one,Vector2.right,Vector2.one*.5f});
    }

    private static void C04Golem(Transform parent)
    {
        var baseRock=C04Material("GolemDeepSlate",new Color(.25f,.27f,.26f));var faceRock=C04Material("GolemWeatheredStone",new Color(.46f,.46f,.40f));var moss=C04Material("GolemMossSeams",new Color(.22f,.27f,.16f));var recess=C04Material("FaceRecess",new Color(.055f,.048f,.035f));
        C04Body("GolemTorso",parent,baseRock,new[]{new Vector3(0,1.25f,0),new Vector3(0,1.57f,.04f),new Vector3(0,2.18f,0),new Vector3(0,2.57f,-.02f),new Vector3(0,2.70f,-.04f)},new[]{new Vector2(.36f,.27f),new Vector2(.44f,.32f),new Vector2(.68f,.37f),new Vector2(.70f,.40f),new Vector2(.30f,.22f)},9,true);
        C04Body("GolemHead",parent,faceRock,new[]{new Vector3(0,2.58f,-.10f),new Vector3(0,2.74f,-.16f),new Vector3(0,3.04f,-.15f),new Vector3(0,3.19f,-.10f)},new[]{new Vector2(.19f,.17f),new Vector2(.28f,.25f),new Vector2(.29f,.23f),new Vector2(.12f,.12f)},7,true);
        for(int side=-1;side<=1;side+=2)
        {
            float s=side;C04Limb("GolemUpperLeg"+side,parent,baseRock,new Vector3(s*.28f,1.44f,.04f),new Vector3(s*.38f,.78f,-.05f),.25f,.20f,true);
            C04Limb("GolemLowerLeg"+side,parent,faceRock,new Vector3(s*.38f,.78f,-.05f),new Vector3(s*.39f,.18f,.015f),.23f,.18f,true);
            C04Body("GolemFoot"+side,parent,faceRock,new[]{new Vector3(s*.39f,.025f,-.14f),new Vector3(s*.39f,.14f,-.14f),new Vector3(s*.39f,.28f,-.015f)},new[]{new Vector2(.24f,.34f),new Vector2(.25f,.35f),new Vector2(.18f,.23f)},7,true);
            C04Limb("GolemUpperArm"+side,parent,faceRock,new Vector3(s*.69f,2.46f,0),new Vector3(s*.96f,1.87f,-.025f),.31f,.245f,true);
            C04Limb("GolemForearm"+side,parent,baseRock,new Vector3(s*.96f,1.87f,-.025f),new Vector3(s*1.02f,1.15f,-.10f),.285f,.23f,true);
            C04Limb("GolemHand"+side,parent,faceRock,new Vector3(s*1.02f,1.18f,-.10f),new Vector3(s*1.015f,.84f,-.135f),.27f,.23f,true);
            C04Body("GolemChestSlab"+side,parent,faceRock,new[]{new Vector3(s*.29f,1.97f,-.30f),new Vector3(s*.30f,2.28f,-.34f),new Vector3(s*.28f,2.50f,-.31f)},new[]{new Vector2(.24f,.10f),new Vector2(.30f,.12f),new Vector2(.23f,.10f)},7,true);
            C04Limb("GolemShoulderMoss"+side,parent,moss,new Vector3(s*.65f,2.51f,-.03f),new Vector3(s*.78f,2.34f,-.13f),.20f,.15f,true);
            Shape("GolemEyeRecess",PrimitiveType.Cube,parent,new Vector3(s*.12f,2.92f,-.38f),new Vector3(.12f,.035f,.018f),recess);
        }
    }
    private static TextMeshProUGUI Text(string name, Transform parent, string value, int fontSize)
    {
        var text = new GameObject(name, typeof(RectTransform), typeof(TextMeshProUGUI)).GetComponent<TextMeshProUGUI>();
        text.transform.SetParent(parent, false);
        text.font = TMP_Settings.defaultFontAsset;
        text.text = value;
        text.fontSize = fontSize;
        text.enableAutoSizing = false;
        text.richText = false;
        text.color = Color.white;
        text.raycastTarget = false;
        return text;
    }

    private static UnityEngine.UI.Button Button(string name, Transform parent, string label, int height, int fontSize)
    {
        var rect = new GameObject(name, typeof(RectTransform), typeof(UnityEngine.UI.Image), typeof(UnityEngine.UI.Button)).GetComponent<RectTransform>();
        rect.SetParent(parent, false);
        SetHeight(rect.gameObject, height);
        var image = rect.GetComponent<UnityEngine.UI.Image>();
        image.color = new Color(0.12f, 0.38f, 0.46f);
        var button = rect.GetComponent<UnityEngine.UI.Button>();
        button.targetGraphic = image;
        var text = Text("Label", rect, label, fontSize);
        text.alignment = TextAlignmentOptions.Center;
        text.rectTransform.anchorMin = Vector2.zero;
        text.rectTransform.anchorMax = Vector2.one;
        text.rectTransform.offsetMin = new Vector2(5, 0);
        text.rectTransform.offsetMax = new Vector2(-5, 0);
        return button;
    }

    private static void SetHeight(GameObject item, int height)
    {
        var element = item.GetComponent<UnityEngine.UI.LayoutElement>() ?? item.AddComponent<UnityEngine.UI.LayoutElement>();
        element.minHeight = element.preferredHeight = height;
    }

    private static void SetMaterial(GameObject item, string name, Color color)
    {
        string path = "Assets/C02/" + name + ".mat";
        var material = AssetDatabase.LoadAssetAtPath<Material>(path);
        if (material == null)
        {
            material = new Material(Shader.Find("Universal Render Pipeline/Lit"));
            AssetDatabase.CreateAsset(material, path);
        }
        material.color = color;
        item.GetComponent<Renderer>().sharedMaterial = material;
    }
}
