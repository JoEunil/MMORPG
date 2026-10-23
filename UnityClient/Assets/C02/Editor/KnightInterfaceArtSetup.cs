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
        var hud = ((GameObject)refs.FindProperty("worldHud").objectReferenceValue).transform;
        var combat = hud.parent.Find("C05CombatPanel") as RectTransform;
        combat.GetComponent<Image>().color = Color.clear;
        foreach (string ornament in new[] { "ForgedFrame", "FrameOrnaments" })
            if (combat.Find(ornament) != null) combat.Find(ornament).gameObject.SetActive(false);
        Place(combat, new Vector2(1, 0), new Vector2(-24, 8), new Vector2(440, 158));
        var vitalsPlate = Child(combat, "VitalsPlate");
        Place(vitalsPlate, new Vector2(0, 1), new Vector2(0, -90), new Vector2(440, 68));
        SkinPanel(vitalsPlate, 8);
        vitalsPlate.SetAsFirstSibling();
        Gauge(combat, "C12HpTrack", -100.667f, 22.667f, new Color(.68f, .055f, .075f));
        Gauge(combat, "C12MpTrack", -129.333f, 18, new Color(.065f, .29f, .64f));
        var skillPlate = Child(combat, "SkillPlate");
        Place(skillPlate, new Vector2(0, 1), new Vector2(109, -8), new Vector2(222, 78));
        SkinPanel(skillPlate, 10);
        skillPlate.SetAsFirstSibling();
        for (int i = 0; i < 3; i++)
        {
            var skill = combat.Find("C05Skill" + i) as RectTransform;
            Place(skill, new Vector2(0, 1), new Vector2(126 + i * 66, -16), new Vector2(56, 62));
            SkinButton(skill.GetComponent<Button>(), 12);
            var key = skill.Find("SkillKeyBadge/SkillKey")?.GetComponent<TMP_Text>();
            if (key != null) { key.fontSize = 22; key.color = Color.white; key.fontStyle = FontStyles.Bold; }
            var label = skill.Find("SkillName")?.GetComponent<TMP_Text>();
            if (label != null) label.gameObject.SetActive(false); // icon + large Z/X/C already identify the action
        }
        var aim = combat.Find("C05Aim")?.GetComponent<TMP_Text>();
        if (aim != null) aim.gameObject.SetActive(false); // facing remains in the existing upper-left world information
        var status = combat.Find("C05Status")?.GetComponent<TMP_Text>();
        if (status != null) status.gameObject.SetActive(false); // server response remains available in the upper-left info
        foreach (string name in new[] { "C07InventoryPanel", "C09BazaarPanel", "C06ChatPanel" })
        {
            var panel = hud.parent.Find(name) as RectTransform;
            SkinPanel(panel, 6);
            if (name != "C06ChatPanel") panel.anchoredPosition = new Vector2(-24, 118);
            foreach (var image in panel.GetComponentsInChildren<Image>(true))
                if (image.name == "InventoryParchment" || image.name == "BazaarParchment" || image.name == "ChatInset")
                { image.sprite = leather; image.type = Image.Type.Simple; image.color = new Color(.62f, .54f, .43f); }
            foreach (var button in panel.GetComponentsInChildren<Button>(true)) SkinButton(button, 20);
        }
        var chat = hud.parent.Find("C06ChatPanel") as RectTransform;
        chat.sizeDelta = new Vector2(400, 260);
        chat.anchoredPosition = new Vector2(24, 8);
        var inset = chat.Find("ChatInset");
        var inputRect = inset.Find("ChatInput") as RectTransform;
        inputRect.sizeDelta = new Vector2(266, 34);
        string[] scopes = { "ChatGlobalButton", "ChatZoneButton", "ChatWhisperButton" };
        for (int i = 0; i < scopes.Length; i++)
            Place(inset.Find(scopes[i]) as RectTransform, new Vector2(0, 1), new Vector2(12 + i * 122, -36), new Vector2(116, 28));
        (inset.Find("ChatWhisperTargetButton") as RectTransform).sizeDelta = new Vector2(370, 24);
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
        EditorUtility.SetDirty(material);
        AssetDatabase.SaveAssets();
        EditorSceneManager.MarkSceneDirty(scene);
        if (!EditorSceneManager.SaveScene(scene)) throw new InvalidOperationException("Could not save interface art.");
    }

    private static void Gauge(RectTransform panel, string name, float y, float height, Color color)
    {
        var track = panel.Find(name) as RectTransform;
        Place(track, new Vector2(0, 1), new Vector2(18.667f, y), new Vector2(402.667f, height));
        track.GetComponent<Image>().color = new Color(.045f, .035f, .03f);
        var fill = track.Find("Fill").GetComponent<Image>();
        fill.color = color;
        var shine = Child(track, "GaugeGlint");
        shine.anchorMin = new Vector2(0, .7f); shine.anchorMax = new Vector2(1, .84f);
        shine.offsetMin = new Vector2(3, 0); shine.offsetMax = new Vector2(-3, 0);
        shine.GetComponent<Image>().color = new Color(1, .94f, .78f, .16f);
        Border(track, 16);
        foreach (var text in track.GetComponentsInChildren<TMP_Text>(true))
        { text.fontSize = height > 20 ? 18 : 15; text.color = new Color(.98f, .94f, .83f); text.transform.SetAsLastSibling(); }
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
