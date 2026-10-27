using System;
using C02;
using TMPro;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.UI;

// Re-skins the existing bound controls; no duplicate UI or runtime state owner.
public static class KnightInterfaceArtSetup
{
    private const string Art = "Assets/C02/Art/KnightInterface/";
    private static readonly Color Gold = new Color(.87f, .74f, .48f);
    private static readonly Color Ink = new Color(.075f, .069f, .066f, .98f);
    private static Sprite frame;
    private static Sprite leather;

    [MenuItem("Tools/Client Art/Apply Knight Interface")]
    public static void Apply()
    {
        var scene = EditorSceneManager.GetActiveScene();
        if (EditorApplication.isPlayingOrWillChangePlaymode || scene.path != "Assets/C02/C02.unity")
            throw new InvalidOperationException("Open the saved C02 scene outside Play Mode.");
        frame = ImportSprite(Art + "forged-gothic-frame.png", new Vector4(250, 250, 250, 250));
        leather = MakeLeather();
        var world = UnityEngine.Object.FindFirstObjectByType<C02WorldScreen>(FindObjectsInactive.Include);
        var refs = new SerializedObject(world);
        var background = ((GameObject)refs.FindProperty("background").objectReferenceValue).GetComponent<Image>();
        ApplyEntryBackdrop(background);
        var selection = ((GameObject)refs.FindProperty("selectionPanel").objectReferenceValue).transform;
        selection.GetComponent<Image>().color = Color.clear;
        var branding = selection.parent.Find("V01Branding");
        if (branding != null) branding.gameObject.SetActive(false);
        foreach (var root in scene.GetRootGameObjects())
            foreach (var part in root.GetComponentsInChildren<Transform>(true))
                if (part.name == "V01Branding") part.gameObject.SetActive(false);
        foreach (string name in new[] { "SignIn", "Results", "LoadingPanel" })
        {
            var p = selection.Find(name) as RectTransform;
            if (p == null) throw new InvalidOperationException("Missing entry stage " + name);
            SkinPanel(p, 5);
            p.anchorMin = p.anchorMax = new Vector2(.79f, .48f);
            p.anchoredPosition = Vector2.zero;
            foreach (var t in p.GetComponentsInChildren<TMP_Text>(true))
            { t.color = new Color(.91f, .86f, .74f); if (t.fontSize < 14) t.fontSize = 14; }
            foreach (var b in p.GetComponentsInChildren<Button>(true)) SkinButton(b, 12);
            foreach (var input in p.GetComponentsInChildren<TMP_InputField>(true))
            {
                var image = input.GetComponent<Image>();
                image.sprite = leather; image.type = Image.Type.Simple; image.color = new Color(.32f, .29f, .25f);
                Border(input.transform as RectTransform, 16);
                input.textComponent.fontSize = 20;
            }
        }
        // The background is already hidden/restored by the world's entry lifecycle.
        var title = Label(background.transform, "CitadelTitle", "SUNWARD", 48);
        Place(title.rectTransform, new Vector2(0, 1), new Vector2(60, -565), new Vector2(600, 76));
        title.fontStyle = FontStyles.Bold;
        title.characterSpacing = 6;
        title.color = new Color(.96f, .87f, .64f);
        var subtitle = Label(background.transform, "CitadelSubtitle", "A KNIGHT'S JOURNEY", 17);
        Place(subtitle.rectTransform, new Vector2(0, 1), new Vector2(64, -636), new Vector2(500, 35));
        subtitle.characterSpacing = 5;
        var signIn = selection.Find("SignIn");
        var signInLayout = signIn.GetComponent<VerticalLayoutGroup>();
        if (signInLayout != null) { signInLayout.padding = new RectOffset(26, 26, 34, 28); signInLayout.spacing = 10; }
        (signIn as RectTransform).sizeDelta = new Vector2(420, 390);
        foreach (var t in signIn.GetComponentsInChildren<TMP_Text>(true))
        {
            if (t.name == "EndpointNote") t.text = "Enter the realm";
            if (t.name == "StageLabel" || t.name == "Subtitle" || t.name == "EndpointNote") t.gameObject.SetActive(false);
            if (t.name == "Title") { t.text = "ENTER THE REALM"; t.fontSize = 24; t.characterSpacing = 2; }
        }
        ApplyRestrainedEntry();
        var hud = ((GameObject)refs.FindProperty("worldHud").objectReferenceValue).transform;
        C06ChatSetup.ApplyCleanStyle();
        C07InventorySetup.ApplyRestrainedInventoryDetails();
        C09BazaarSetup.ApplyCleanStyle();
        foreach (string name in new[] { "C07BagButton", "C09MarketButton" })
            SkinButton(hud.parent.Find(name).GetComponent<Button>(), 16);
        SkinButton(hud.Find("DisconnectButton").GetComponent<Button>(), 16);
        var shader = AssetDatabase.LoadAssetAtPath<Shader>("Assets/C02/Art/KnightRefinement/SteelSkillGlow.shader");
        if (shader == null) throw new InvalidOperationException("The vertex-color skill shader is required.");
        const string materialPath = "Assets/C02/Art/KnightRefinement/SteelSkillGlow.mat";
        var material = AssetDatabase.LoadAssetAtPath<Material>(materialPath);
        if (material == null) { material = new Material(shader); AssetDatabase.CreateAsset(material, materialPath); }
        material.shader = shader;
        material.color = Color.white;
        var combatRefs = new SerializedObject(world.GetComponent<C05CombatScreen>());
        combatRefs.FindProperty("skillEffectMaterial").objectReferenceValue = material;
        combatRefs.ApplyModifiedPropertiesWithoutUndo();
        var observed = world.GetComponent<C04ObservedWorld>();
        if (observed != null)
        {
            var observedRefs = new SerializedObject(observed);
            observedRefs.FindProperty("monsterRangeMaterial").objectReferenceValue = material;
            observedRefs.ApplyModifiedPropertiesWithoutUndo();
        }
        ApplyRestrainedHud();
        EditorUtility.SetDirty(material);
        AssetDatabase.SaveAssets();
        EditorSceneManager.MarkSceneDirty(scene);
        if (!EditorSceneManager.SaveScene(scene)) throw new InvalidOperationException("Could not save interface art.");
    }

    [MenuItem("Tools/Client Art/Refine Entry Layout")]
    public static void ApplyRestrainedEntry()
    {
        var scene = EditorSceneManager.GetActiveScene();
        if (EditorApplication.isPlayingOrWillChangePlaymode || scene.path != "Assets/C02/C02.unity")
            throw new InvalidOperationException("Open the C02 scene outside Play Mode.");
        var world = UnityEngine.Object.FindFirstObjectByType<C02WorldScreen>(FindObjectsInactive.Include);
        var refs = new SerializedObject(world);
        ApplyEntryBackdrop(((GameObject)refs.FindProperty("background").objectReferenceValue).GetComponent<Image>());
        var selection = ((GameObject)refs.FindProperty("selectionPanel").objectReferenceValue).transform;
        foreach (string name in new[] { "SignIn", "Results", "LoadingPanel" })
        {
            var panel = selection.Find(name) as RectTransform;
            EdgedSurface(panel, new Color(.045f, .05f, .055f, .94f));
            foreach (var part in panel.GetComponentsInChildren<Transform>(true))
                if (part.name == "ForgedFrame" || part.name == "FrameOrnaments") part.gameObject.SetActive(false);
            foreach (var input in panel.GetComponentsInChildren<TMP_InputField>(true))
            {
                EdgedSurface(input.transform as RectTransform, new Color(.085f, .095f, .105f));
                input.textComponent.fontSize = 18;
                if (input.placeholder is TMP_Text placeholder) placeholder.fontSize = 18;
            }
            foreach (var button in panel.GetComponentsInChildren<Button>(true))
            {
                bool primary = button.name == "LoginButton" || button.name == "EnterButton";
                EdgedSurface(button.transform as RectTransform, primary
                    ? new Color(.27f, .29f, .28f) : new Color(.115f, .13f, .14f));
                var colors = ColorBlock.defaultColorBlock;
                colors.highlightedColor = new Color(1.14f, 1.14f, 1.14f);
                colors.pressedColor = new Color(.8f, .8f, .8f);
                colors.disabledColor = new Color(.6f, .6f, .6f, .65f);
                button.colors = colors;
                foreach (var label in button.GetComponentsInChildren<TMP_Text>(true))
                { label.characterSpacing = 0; label.fontStyle = FontStyles.Normal; label.color = new Color(.92f, .93f, .92f); }
            }
        }
        var signIn = selection.Find("SignIn") as RectTransform;
        signIn.sizeDelta = new Vector2(360, 360);
        var layout = signIn.GetComponent<VerticalLayoutGroup>();
        layout.padding = new RectOffset(24, 24, 24, 24);
        layout.spacing = 8;
        foreach (var label in signIn.GetComponentsInChildren<TMP_Text>(true))
        {
            if (label.name == "Title")
            { label.text = "Sign in"; label.fontSize = 24; label.characterSpacing = 0; label.fontStyle = FontStyles.Normal; }
            if (label.name == "LoginIdLabel" || label.name == "PasswordLabel")
            { label.fontSize = 15; label.fontStyle = FontStyles.Normal; label.color = new Color(.72f, .75f, .76f); }
        }
        foreach (string name in new[] { "LoginIdInput", "PasswordInput", "LoginButton" })
            signIn.Find(name).GetComponent<LayoutElement>().preferredHeight = 44;
        EditorSceneManager.MarkSceneDirty(scene);
    }

    private static void ApplyEntryBackdrop(Image background)
    {
        var backdrop = ImportSprite(Art + "citadel-overcast-login-v2.png", Vector4.zero);
        background.sprite = backdrop;
        background.color = Color.white;
        background.type = Image.Type.Simple;
        background.preserveAspect = false;
        var aspect = background.GetComponent<AspectRatioFitter>() ?? background.gameObject.AddComponent<AspectRatioFitter>();
        aspect.aspectMode = AspectRatioFitter.AspectMode.EnvelopeParent;
        aspect.aspectRatio = (float)backdrop.texture.width / backdrop.texture.height;
    }
    private static void EdgedSurface(RectTransform rect, Color color)
    {
        PlainSurface(rect, color);
        var edge = rect.GetComponent<UnityEngine.UI.Outline>() ?? rect.gameObject.AddComponent<UnityEngine.UI.Outline>();
        edge.effectColor = new Color(.32f, .34f, .34f, .8f);
        edge.effectDistance = new Vector2(1, -1);
        edge.useGraphicAlpha = false;
    }

    [MenuItem("Tools/Client Art/Refine HUD Layout")]
    public static void ApplyRestrainedHud()
    {
        if (EditorApplication.isPlayingOrWillChangePlaymode)
            throw new InvalidOperationException("HUD authoring requires Edit Mode.");
        var world = UnityEngine.Object.FindFirstObjectByType<C02WorldScreen>(FindObjectsInactive.Include);
        if (world == null) throw new InvalidOperationException("The existing world screen is required.");
        var gaugeFace = AssetDatabase.LoadAssetAtPath<Sprite>("Assets/C02/Art/CitadelEnvironment/vitals-track-face-v1.png");
        var skillIcons = new[] {
            AssetDatabase.LoadAssetAtPath<Sprite>("Assets/C02/Art/Skills/skill-strike-medallion-v3.png"),
            AssetDatabase.LoadAssetAtPath<Sprite>("Assets/C02/Art/Skills/skill-ring-medallion-v3.png"),
            AssetDatabase.LoadAssetAtPath<Sprite>("Assets/C02/Art/Skills/skill-line-medallion-v3.png")
        };
        if (gaugeFace == null || Array.Exists(skillIcons, icon => icon == null))
            throw new InvalidOperationException("Import the selected gauge face and three skill medallions as Sprites before applying the HUD.");
        var worldRefs = new SerializedObject(world);
        var hud = (GameObject)worldRefs.FindProperty("worldHud").objectReferenceValue;
        // Keep diagnostic information clear of the centered currency display.
        (hud.transform as RectTransform).sizeDelta = new Vector2(440, 112);
        var combat = hud.transform.parent.Find("C05CombatPanel") as RectTransform;
        if (combat == null) throw new InvalidOperationException("The existing combat HUD is required.");
        combat.anchorMin = new Vector2(0, 0); combat.anchorMax = new Vector2(1, 0);
        combat.pivot = new Vector2(.5f, 0); combat.anchoredPosition = new Vector2(0, 8);
        combat.sizeDelta = new Vector2(0, 78);
        combat.GetComponent<Image>().color = Color.clear;
        combat.GetComponent<Image>().raycastTarget = false;
        foreach (var part in combat.GetComponentsInChildren<Transform>(true))
            if (part.name == "ForgedFrame" || part.name == "FrameOrnaments" || part.name == "GaugeGlint") part.gameObject.SetActive(false);
        var vitals = Child(combat, "VitalsPlate");
        Place(vitals, new Vector2(1, 0), new Vector2(-24, 16), new Vector2(292, 64));
        EdgedSurface(vitals, new Color(.086f, .153f, .161f, .9f));
        vitals.SetAsFirstSibling();
        Gauge(combat, "C12HpTrack", 48, new Color(.88f, .28f, .35f), gaugeFace);
        Gauge(combat, "C12MpTrack", 20, new Color(.23f, .55f, .86f), gaugeFace);
        var skills = Child(combat, "SkillPlate");
        Place(skills, new Vector2(.5f, 0), Vector2.zero, new Vector2(222, 78));
        PlainSurface(skills, Color.clear);
        var skillPlateEdge = skills.GetComponent<Outline>();
        if (skillPlateEdge != null) skillPlateEdge.enabled = false;
        skills.SetAsFirstSibling();
        for (int i = 0; i < 3; i++)
        {
            var skill = combat.Find("C05Skill" + i) as RectTransform;
            Place(skill, new Vector2(.5f, 0), new Vector2((i - 1) * 66, 8), new Vector2(56, 62));
            PlainSurface(skill, Color.clear);
            var skillEdge = skill.GetComponent<Outline>();
            if (skillEdge != null) skillEdge.enabled = false;
            var colors = ColorBlock.defaultColorBlock;
            colors.highlightedColor = new Color(1.16f, 1.16f, 1.16f);
            var button = skill.GetComponent<Button>();
            button.colors = colors;
            var skillIcon = skill.Find("SkillIcon") as RectTransform;
            if (skillIcon != null)
            {
                Place(skillIcon, new Vector2(.5f, 1), Vector2.zero, new Vector2(56, 62));
                var image = skillIcon.GetComponent<Image>();
                image.sprite = skillIcons[i]; image.type = Image.Type.Simple;
                image.color = Color.white; image.preserveAspect = true; image.raycastTarget = false;
                button.targetGraphic = image;
            }
            var badge = skill.Find("SkillKeyBadge") as RectTransform;
            if (badge != null)
            {
                EdgedSurface(badge, new Color(.067f, .141f, .165f));
                var edge = badge.GetComponent<Outline>();
                edge.effectColor = new Color(.89f, .8f, .6f); edge.effectDistance = new Vector2(.8f, -.8f);
                badge.SetAsLastSibling();
            }
            var key = badge != null ? badge.Find("SkillKey")?.GetComponent<TMP_Text>() : null;
            if (key != null) { key.fontSize = 22; key.color = Color.white; key.fontStyle = FontStyles.Bold; }
            var name = skill.Find("SkillName"); if (name != null) name.gameObject.SetActive(false);
        }
        // Match the existing runtime placements: short action feedback above skills, growth above vitals.
        var status = combat.Find("C05Status").GetComponent<TMP_Text>();
        status.gameObject.SetActive(true);
        Place(status.rectTransform, new Vector2(.5f, 0), new Vector2(0, 82), new Vector2(460, 40));
        status.fontSize = 16; status.alignment = TextAlignmentOptions.Center; status.raycastTarget = false;
        var aim = combat.Find("C05Aim").GetComponent<TMP_Text>();
        aim.gameObject.SetActive(true);
        Place(aim.rectTransform, new Vector2(1, 0), new Vector2(-24, 74), new Vector2(440, 24));
        aim.fontSize = 16; aim.alignment = TextAlignmentOptions.MidlineRight; aim.raycastTarget = false;

        var wallet = hud.transform.parent.Find("WalletHud") as RectTransform;
        if (wallet != null)
        {
            Place(wallet, new Vector2(.5f, 1), new Vector2(0, -18), new Vector2(280, 30));
            PlainSurface(wallet, new Color(.055f, .065f, .07f, .25f));
            Place(wallet.Find("Gold") as RectTransform, new Vector2(0, 1), new Vector2(28, 0), new Vector2(108, 30));
            Place(wallet.Find("Diamonds") as RectTransform, new Vector2(0, 1), new Vector2(164, 0), new Vector2(108, 30));
            Place(wallet.Find("GoldMark") as RectTransform, new Vector2(0, .5f), new Vector2(3, 0), new Vector2(20, 20));
            Place(wallet.Find("DiamondMark") as RectTransform, new Vector2(0, .5f), new Vector2(140, 0), new Vector2(20, 20));
            wallet.Find("GoldMark").localRotation = Quaternion.identity;
            wallet.Find("DiamondMark").localRotation = Quaternion.identity;
            foreach (var text in wallet.GetComponentsInChildren<TMP_Text>(true)) text.fontSize = 17;
        }
        MenuButton(hud.transform.parent.Find("C07BagButton") as RectTransform, false);
        MenuButton(hud.transform.parent.Find("C09MarketButton") as RectTransform, true);
        var disconnect = hud.transform.Find("DisconnectButton") as RectTransform;
        if (disconnect != null)
        {
            Place(disconnect, new Vector2(1, 0), new Vector2(0, -42), new Vector2(118, 32));
            EdgedSurface(disconnect, new Color(.08f, .10f, .12f, .8f));
            foreach (string name in new[] { "ForgedFrame", "FrameOrnaments" })
                disconnect.Find(name)?.gameObject.SetActive(false);
            foreach (var text in disconnect.GetComponentsInChildren<TMP_Text>(true)) text.fontSize = 14;
        }
        C06ChatSetup.ApplyCleanStyle();
        ConfigureMinimap(hud.transform.parent, world);
        EditorSceneManager.MarkSceneDirty(world.gameObject.scene);
    }

    private static void ConfigureMinimap(Transform canvas, C02WorldScreen world)
    {
        var panel = Child(canvas, "WorldMinimapPanel");
        Place(panel, new Vector2(1, 1), new Vector2(-24, -86), new Vector2(208, 156));
        PlainSurface(panel, Color.clear);
        var frame = panel.GetComponent<Outline>();
        if (frame != null) frame.enabled = false;
        var group = panel.GetComponent<CanvasGroup>();
        if (group == null) group = panel.gameObject.AddComponent<CanvasGroup>();
        group.alpha = 0; group.blocksRaycasts = false; group.interactable = false;
        foreach (string name in new[] { "MinimapTitle", "SelfDot", "SelfLegend", "PlayerDot", "PlayerLegend", "MonsterDot", "MonsterLegend" })
        {
            var old = panel.Find(name);
            if (old != null) UnityEngine.Object.DestroyImmediate(old.gameObject);
        }
        var north = Label(panel, "MinimapNorth", "N", 14);
        Place(north.rectTransform, new Vector2(0, 1), new Vector2(96, -2), new Vector2(16, 18));
        north.alignment = TextAlignmentOptions.Center;
        var map = panel.Find("Map") as RectTransform;
        if (map == null)
        {
            map = new GameObject("Map", typeof(RectTransform), typeof(CanvasRenderer), typeof(WorldMinimap))
                .GetComponent<RectTransform>();
            map.SetParent(panel, false);
        }
        Place(map, new Vector2(0, 1), Vector2.zero, new Vector2(208, 156));
        var graphic = map.GetComponent<WorldMinimap>();
        graphic.raycastTarget = false;
        const string materialPath = "Assets/C02/Art/CitadelEnvironment/WorldMinimap.mat";
        var material = AssetDatabase.LoadAssetAtPath<Material>(materialPath);
        if (material == null)
        {
            var shader = Shader.Find("Client Art/World Minimap");
            if (shader == null) throw new InvalidOperationException("The radar UI shader is required.");
            material = new Material(shader);
            AssetDatabase.CreateAsset(material, materialPath);
        }
        graphic.material = material;
        var refs = new SerializedObject(graphic);
        refs.FindProperty("worldScreen").objectReferenceValue = world;
        refs.FindProperty("observedWorld").objectReferenceValue = world.GetComponent<C04ObservedWorld>();
        refs.FindProperty("worldMap").objectReferenceValue = new SerializedObject(world).FindProperty("worldMap").objectReferenceValue;
        refs.FindProperty("panelGroup").objectReferenceValue = group;
        refs.FindProperty("viewRadius").floatValue = 40;
        WorldMinimapAtlasExport.TryLoad(out var atlas, out var bounds);
        refs.FindProperty("terrainAtlas").objectReferenceValue = atlas;
        refs.FindProperty("atlasBounds").rectValue = bounds;
        refs.ApplyModifiedPropertiesWithoutUndo();
        north.rectTransform.SetAsLastSibling();
        var innerRange = Label(panel, "MinimapInnerRange", "20m", 13);
        Place(innerRange.rectTransform, new Vector2(0, 1), new Vector2(155, -72), new Vector2(31, 18));
        var outerRange = Label(panel, "MinimapOuterRange", "40m", 13);
        Place(outerRange.rectTransform, new Vector2(0, 1), new Vector2(177, -82), new Vector2(31, 18));
        innerRange.color = outerRange.color = new Color(.83f, .86f, .79f);

    }

    private static void MenuButton(RectTransform button, bool market)
    {
        if (button == null) throw new InvalidOperationException("Both existing menu buttons are required.");
        var atlas = AssetDatabase.LoadAssetAtPath<Texture2D>("Assets/C02/Art/CitadelEnvironment/hud-menu-icons-v8.png");
        if (atlas == null) throw new InvalidOperationException("The authored menu icon atlas is required.");
        Place(button, new Vector2(1, 1), new Vector2(market ? -86 : -24, -16), new Vector2(54, 54));
        PlainSurface(button, Color.clear);
        button.GetComponent<UnityEngine.UI.Image>().raycastTarget = true;
        var outline = button.GetComponent<Outline>();
        if (outline != null) outline.enabled = false;
        foreach (string name in new[] { "ForgedFrame", "FrameOrnaments", "Label" }) button.Find(name)?.gameObject.SetActive(false);
        foreach (string name in new[] { "MarketStem", "MarketBeam", "MarketBase", "MarketChain-1", "MarketChain1", "MarketTray-1", "MarketTray1",
            "BagBody", "BagInset", "BagHandle", "BagHandleInset", "BagFlap", "BagClasp" })
        {
            var old = button.Find(name);
            if (old != null) UnityEngine.Object.DestroyImmediate(old.gameObject);
        }
        var iconRect = button.Find("MenuIcon") as RectTransform;
        if (iconRect == null)
        {
            iconRect = new GameObject("MenuIcon", typeof(RectTransform), typeof(CanvasRenderer), typeof(UnityEngine.UI.RawImage))
                .GetComponent<RectTransform>();
            iconRect.SetParent(button, false);
        }
        float aspect = atlas.width * .5f / atlas.height;
        Vector2 size = aspect >= 1 ? new Vector2(54, 54 / aspect) : new Vector2(54 * aspect, 54);
        Place(iconRect, new Vector2(.5f, .5f), new Vector2(-2, -2), size);
        var icon = iconRect.GetComponent<UnityEngine.UI.RawImage>();
        icon.texture = atlas;
        icon.uvRect = new Rect(market ? .5f : 0, 0, .5f, 1);
        icon.color = Color.white;
        icon.raycastTarget = false;
        // Preserve the existing click target and callbacks; hover/disabled tint belongs to the illustration.
        var control = button.GetComponent<UnityEngine.UI.Button>();
        control.targetGraphic = icon;
        control.colors = ColorBlock.defaultColorBlock;
        var key = Label(button, "MenuKey", market ? "B" : "I", 16);
        Place(key.rectTransform, new Vector2(1, 1), new Vector2(-1, 0), new Vector2(14, 20));
        key.fontStyle = FontStyles.Bold;
        key.alignment = TextAlignmentOptions.Center;
        key.color = new Color(.96f, .96f, .90f);
        key.rectTransform.SetAsLastSibling();
    }

    private static void PlainSurface(RectTransform rect, Color color)
    {
        var image = rect.GetComponent<Image>();
        image.sprite = null; image.type = Image.Type.Simple; image.color = color;
    }

    private static void Gauge(RectTransform panel, string name, float y, Color color, Sprite face)
    {
        var track = panel.Find(name) as RectTransform;
        Place(track, new Vector2(1, 0), new Vector2(-32, y), new Vector2(274, 21));
        EdgedSurface(track, new Color(.153f, .22f, .231f));
        var trackImage = track.GetComponent<Image>();
        trackImage.sprite = face; trackImage.preserveAspect = false;
        var edge = track.GetComponent<Outline>();
        edge.effectColor = new Color(.73f, .67f, .52f); edge.effectDistance = new Vector2(.8f, -.8f);
        var fill = track.Find("Fill").GetComponent<Image>();
        // The track and fill share their complete silhouette; the runtime still owns fillAmount.
        fill.sprite = face; fill.color = color; fill.preserveAspect = false;
        fill.rectTransform.anchorMin = Vector2.zero; fill.rectTransform.anchorMax = Vector2.one;
        fill.rectTransform.offsetMin = Vector2.zero; fill.rectTransform.offsetMax = Vector2.zero;
        foreach (var text in track.GetComponentsInChildren<TMP_Text>(true))
        { text.fontSize = 15; text.color = new Color(.94f, .96f, .98f); text.transform.SetAsLastSibling(); }
    }

    private static void SkinPanel(RectTransform rect, float multiplier)
    {
        if (rect == null) throw new InvalidOperationException("A bound UI panel is missing.");
        var image = rect.GetComponent<Image>(); image.sprite = leather; image.type = Image.Type.Simple; image.color = Ink;
        var old = rect.Find("FrameOrnaments"); if (old != null) old.gameObject.SetActive(false);
        Border(rect, multiplier);
    }

    private static void SkinButton(Button button, float multiplier)
    {
        var image = button.GetComponent<Image>();
        image.sprite = leather; image.type = Image.Type.Simple; image.color = new Color(.38f, .18f, .16f);
        var colors = button.colors;
        colors.normalColor = Color.white; colors.highlightedColor = new Color(1.25f, 1.13f, .92f);
        colors.pressedColor = new Color(.65f, .56f, .44f); colors.disabledColor = new Color(.5f, .5f, .5f, .6f);
        button.colors = colors;
        var old = button.transform.Find("FrameOrnaments"); if (old != null) old.gameObject.SetActive(false);
        Border(button.transform as RectTransform, multiplier);
    }

    private static void Border(RectTransform parent, float multiplier)
    {
        var rect = Child(parent, "ForgedFrame");
        rect.anchorMin = Vector2.zero; rect.anchorMax = Vector2.one;
        rect.offsetMin = new Vector2(-5, -5); rect.offsetMax = new Vector2(5, 5);
        var image = rect.GetComponent<Image>(); image.sprite = frame; image.type = Image.Type.Sliced;
        image.pixelsPerUnitMultiplier = multiplier; image.color = new Color(1, 1, 1, .94f);
        rect.SetAsLastSibling();
    }

    private static RectTransform Child(Transform parent, string name)
    {
        var rect = parent.Find(name) as RectTransform;
        if (rect == null) { rect = new GameObject(name, typeof(RectTransform), typeof(Image)).GetComponent<RectTransform>(); rect.SetParent(parent, false); }
        var layout = rect.GetComponent<LayoutElement>() ?? rect.gameObject.AddComponent<LayoutElement>();
        layout.ignoreLayout = true;
        rect.GetComponent<Image>().raycastTarget = false;
        return rect;
    }

    private static TMP_Text Label(Transform parent, string name, string content, int size)
    {
        var t = parent.Find(name)?.GetComponent<TMP_Text>();
        if (t == null) { var go = new GameObject(name, typeof(RectTransform), typeof(TextMeshProUGUI)); go.transform.SetParent(parent, false); t = go.GetComponent<TMP_Text>(); }
        t.font = TMP_Settings.defaultFontAsset; t.text = content; t.fontSize = size; t.color = Gold; t.raycastTarget = false;
        t.gameObject.SetActive(true);
        return t;
    }

    private static void Place(RectTransform rect, Vector2 anchor, Vector2 position, Vector2 size)
    { rect.anchorMin = rect.anchorMax = rect.pivot = anchor; rect.anchoredPosition = position; rect.sizeDelta = size; }

    private static Sprite ImportSprite(string path, Vector4 border)
    {
        var importer = AssetImporter.GetAtPath(path) as TextureImporter;
        if (importer == null) throw new InvalidOperationException("Missing art source " + path);
        importer.textureType = TextureImporterType.Sprite; importer.spriteImportMode = SpriteImportMode.Single;
        importer.spriteBorder = border; importer.mipmapEnabled = false; importer.alphaIsTransparency = true;
        importer.wrapMode = TextureWrapMode.Clamp; importer.maxTextureSize = 2048; importer.SaveAndReimport();
        return AssetDatabase.LoadAssetAtPath<Sprite>(path);
    }

    private static Sprite MakeLeather()
    {
        const string path = Art + "EmbossedLeather.asset";
        var texture = AssetDatabase.LoadAssetAtPath<Texture2D>(path);
        if (texture == null)
        {
            texture = new Texture2D(128, 128, TextureFormat.RGBA32, false) { name = "EmbossedLeather", wrapMode = TextureWrapMode.Repeat };
            for (int y = 0; y < 128; y++) for (int x = 0; x < 128; x++)
            {
                float grain = Mathf.PerlinNoise(x * .42f, y * .42f) * .14f + Mathf.PerlinNoise(x * .08f, y * .08f) * .1f;
                float crease = (x + y) % 13 == 0 ? -.04f : 0;
                texture.SetPixel(x, y, new Color(.42f + grain + crease, .36f + grain + crease, .29f + grain + crease));
            }
            texture.Apply(); AssetDatabase.CreateAsset(texture, path);
        }
        foreach (var item in AssetDatabase.LoadAllAssetsAtPath(path)) if (item is Sprite existing) return existing;
        var sprite = Sprite.Create(texture, new Rect(0, 0, 128, 128), new Vector2(.5f, .5f), 100);
        sprite.name = "EmbossedLeather"; AssetDatabase.AddObjectToAsset(sprite, texture);
        return sprite;
    }
}
