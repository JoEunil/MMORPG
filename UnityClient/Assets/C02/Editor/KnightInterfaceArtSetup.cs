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
        Sprite backdrop = ImportSprite(Art + "citadel-dawn-login.png", Vector4.zero);
        leather = MakeLeather();
        var world = UnityEngine.Object.FindFirstObjectByType<C02WorldScreen>(FindObjectsInactive.Include);
        var refs = new SerializedObject(world);
        var background = ((GameObject)refs.FindProperty("background").objectReferenceValue).GetComponent<Image>();
        background.sprite = backdrop;
        background.color = Color.white;
        background.type = Image.Type.Simple;
        background.preserveAspect = false;
        var aspect = background.GetComponent<AspectRatioFitter>() ?? background.gameObject.AddComponent<AspectRatioFitter>();
        aspect.aspectMode = AspectRatioFitter.AspectMode.EnvelopeParent;
        aspect.aspectRatio = (float)backdrop.texture.width / backdrop.texture.height;
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
        foreach (string name in new[] { "C07InventoryPanel" })
        {
            var panel = hud.parent.Find(name) as RectTransform;
            if (panel == null) throw new InvalidOperationException("The existing inventory panel is required.");
            // The four-pixel inset reveals a restrained edge around the inventory surface.
            var panelImage = panel.GetComponent<Image>();
            panelImage.sprite = null;
            panelImage.color = new Color(.40f, .47f, .52f, .98f);
            foreach (string part in new[] { "ForgedFrame", "FrameOrnaments" })
                panel.Find(part)?.gameObject.SetActive(false);
            if (name != "C06ChatPanel") panel.anchoredPosition = new Vector2(-24, 118);
        }
        C06ChatSetup.ApplyCleanStyle();
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
        EdgedSurface(vitals, new Color(.055f, .065f, .07f, .55f));
        vitals.SetAsFirstSibling();
        Gauge(combat, "C12HpTrack", 50, 18, new Color(.64f, .12f, .15f));
        Gauge(combat, "C12MpTrack", 30, 14, new Color(.12f, .34f, .65f));
        var skills = Child(combat, "SkillPlate");
        Place(skills, new Vector2(.5f, 0), Vector2.zero, new Vector2(222, 78));
        EdgedSurface(skills, new Color(.055f, .065f, .07f, .35f));
        skills.SetAsFirstSibling();
        for (int i = 0; i < 3; i++)
        {
            var skill = combat.Find("C05Skill" + i) as RectTransform;
            Place(skill, new Vector2(.5f, 0), new Vector2((i - 1) * 66, 8), new Vector2(56, 62));
            EdgedSurface(skill, new Color(.12f, .14f, .15f, .8f));
            var colors = ColorBlock.defaultColorBlock;
            colors.highlightedColor = new Color(1.16f, 1.16f, 1.16f);
            skill.GetComponent<Button>().colors = colors;
            var badge = skill.Find("SkillKeyBadge") as RectTransform;
            if (badge != null) PlainSurface(badge, new Color(.04f, .05f, .065f, .92f));
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
            Place(wallet.Find("GoldMark") as RectTransform, new Vector2(0, .5f), new Vector2(8, 0), new Vector2(13, 8));
            Place(wallet.Find("DiamondMark") as RectTransform, new Vector2(0, .5f), new Vector2(145, 0), new Vector2(10, 10));
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
        EditorSceneManager.MarkSceneDirty(world.gameObject.scene);
    }

    private static void MenuButton(RectTransform button, bool market)
    {
        if (button == null) throw new InvalidOperationException("Both existing menu buttons are required.");
        Place(button, new Vector2(1, 1), new Vector2(market ? -106 : -24, -18), new Vector2(74, 56));
        EdgedSurface(button, new Color(.055f, .065f, .07f, .68f));
        foreach (string name in new[] { "ForgedFrame", "FrameOrnaments" }) button.Find(name)?.gameObject.SetActive(false);
        var label = button.Find("Label").GetComponent<TMP_Text>();
        Place(label.rectTransform, new Vector2(.5f, 0), new Vector2(0, 3), new Vector2(66, 18));
        label.text = market ? "Market" : "Bag";
        label.fontSize = 14; label.characterSpacing = 0; label.alignment = TextAlignmentOptions.Center;
        label.color = new Color(.86f, .87f, .82f); label.raycastTarget = false;
        var key = Label(button, "MenuKey", market ? "B" : "I", 14);
        Place(key.rectTransform, new Vector2(1, 1), new Vector2(-5, -3), new Vector2(14, 18));
        key.alignment = TextAlignmentOptions.Center; key.color = new Color(.93f, .93f, .87f);
        button.GetComponent<Button>().colors = ColorBlock.defaultColorBlock;
        // Small authored line icons stay inside the existing clickable control.
        Color line = new Color(.76f, .74f, .65f);
        if (market)
        {
            IconPart(button, "MarketStem", new Vector2(0, 12), new Vector2(2, 22), line);
            IconPart(button, "MarketBeam", new Vector2(0, 20), new Vector2(28, 2), line);
            IconPart(button, "MarketBase", new Vector2(0, 2), new Vector2(16, 2), line);
            for (int side = -1; side <= 1; side += 2)
            {
                IconPart(button, "MarketChain" + side, new Vector2(side * 11, 15), new Vector2(1, 9), line);
                IconPart(button, "MarketTray" + side, new Vector2(side * 11, 10), new Vector2(10, 3), line);
            }
        }
        else
        {
            IconPart(button, "BagBody", new Vector2(0, 11), new Vector2(22, 18), line);
            IconPart(button, "BagInset", new Vector2(0, 11), new Vector2(18, 14), new Color(.08f, .10f, .11f));
            IconPart(button, "BagHandle", new Vector2(0, 23), new Vector2(10, 6), line);
            IconPart(button, "BagHandleInset", new Vector2(0, 22), new Vector2(6, 4), new Color(.08f, .10f, .11f));
            IconPart(button, "BagFlap", new Vector2(0, 15), new Vector2(18, 2), line);
            IconPart(button, "BagClasp", new Vector2(0, 11), new Vector2(3, 4), line);
        }
    }

    private static void IconPart(Transform parent, string name, Vector2 position, Vector2 size, Color color)
    {
        var part = parent.Find(name) as RectTransform;
        if (part == null)
        {
            part = new GameObject(name, typeof(RectTransform), typeof(UnityEngine.UI.Image)).GetComponent<RectTransform>();
            part.SetParent(parent, false);
        }
        Place(part, new Vector2(.5f, .5f), position, size);
        part.GetComponent<UnityEngine.UI.Image>().color = color;
        part.GetComponent<UnityEngine.UI.Image>().raycastTarget = false;
    }

    private static void PlainSurface(RectTransform rect, Color color)
    {
        var image = rect.GetComponent<Image>();
        image.sprite = null; image.type = Image.Type.Simple; image.color = color;
    }

    private static void Gauge(RectTransform panel, string name, float y, float height, Color color)
    {
        var track = panel.Find(name) as RectTransform;
        Place(track, new Vector2(1, 0), new Vector2(-40, y), new Vector2(260, height));
        EdgedSurface(track, new Color(.08f, .095f, .11f, .9f));
        track.Find("Fill").GetComponent<Image>().color = color;
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
