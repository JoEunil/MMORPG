using C01;
using C02;
using TMPro;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;
using UnityEngine.UI;

public static class C09BazaarSetup
{
    public static void Apply()
    {
        if (EditorApplication.isPlaying) throw new System.InvalidOperationException("Stop Play before applying C09 bazaar UI.");
        if (EditorSceneManager.GetActiveScene().path != "Assets/C02/C02.unity")
            throw new System.InvalidOperationException("Open the C02 scene first.");

        var canvas = Object.FindFirstObjectByType<Canvas>();
        var login = Object.FindFirstObjectByType<C01LoginScreen>();
        var world = Object.FindFirstObjectByType<C02WorldScreen>();
        var combat = Object.FindFirstObjectByType<C05CombatScreen>();
        if (canvas == null || canvas.renderMode != RenderMode.ScreenSpaceOverlay || login == null || world == null || combat == null ||
            canvas.GetComponent<GraphicRaycaster>() == null)
            throw new System.InvalidOperationException("Expected the C02 overlay Canvas, GraphicRaycaster, login, world, and combat screens.");

        var market = Button("C09MarketButton", canvas.transform, "MARKET", new Color(.35f, .25f, .12f, .98f));
        var marketRect = market.GetComponent<RectTransform>();
        marketRect.anchorMin = marketRect.anchorMax = marketRect.pivot = new Vector2(1, 1);
        marketRect.anchoredPosition = new Vector2(-150, -24);
        marketRect.sizeDelta = new Vector2(118, 42);
        market.GetComponentInChildren<TMP_Text>(true).fontSize = 13;
        market.GetComponentInChildren<TMP_Text>(true).color = new Color(.98f, .88f, .64f);

        var panel = FindOrCreate("C09BazaarPanel", canvas.transform, typeof(RectTransform), typeof(Image));
        var panelRect = panel.GetComponent<RectTransform>();
        panelRect.anchorMin = panelRect.anchorMax = panelRect.pivot = new Vector2(1, 0);
        panelRect.anchoredPosition = new Vector2(-24, 22);
        panelRect.sizeDelta = new Vector2(500, 590);
        panel.GetComponent<Image>().color = new Color(.51f, .36f, .16f, .99f);
        panel.GetComponent<Image>().raycastTarget = true;

        var paper = FindOrCreate("BazaarParchment", panel.transform, typeof(RectTransform), typeof(Image));
        Stretch(paper.GetComponent<RectTransform>(), 4);
        paper.GetComponent<Image>().color = new Color(.83f, .75f, .56f, 1);
        paper.GetComponent<Image>().raycastTarget = true;

        var title = Text("BazaarTitle", paper.transform, "BAZAAR  /  LISTINGS", 19, new Color(.22f, .17f, .10f));
        title.alignment = TextAlignmentOptions.MidlineLeft;
        SetTop(title.rectTransform, 15, 30);
        var close = Button("BazaarCloseButton", paper.transform, "CLOSE", new Color(.39f, .27f, .14f, 1));
        AnchorTopRight(close.GetComponent<RectTransform>(), 12, 84, 32);

        var typeButtons = new Button[3];
        string[] names = { "POTIONS", "WEAPONS / SHIELDS", "ARMOR" };
        for (int i = 0; i < typeButtons.Length; i++)
        {
            typeButtons[i] = Button("BazaarType" + (i + 1), paper.transform, names[i], new Color(.42f, .30f, .16f, 1));
            var rect = typeButtons[i].GetComponent<RectTransform>();
            rect.anchorMin = new Vector2(0, 1);
            rect.anchorMax = new Vector2(0, 1);
            rect.pivot = new Vector2(0, 1);
            rect.anchoredPosition = new Vector2(15 + i * 154, -58);
            rect.sizeDelta = new Vector2(142, 34);
            typeButtons[i].GetComponentInChildren<TMP_Text>(true).fontSize = 10;
        }

        var scrollObject = FindOrCreate("BazaarScroll", paper.transform, typeof(RectTransform), typeof(ScrollRect));
        Stretch(scrollObject.GetComponent<RectTransform>(), new RectOffset(14, 14, 100, 142));
        var viewport = FindOrCreate("BazaarViewport", scrollObject.transform, typeof(RectTransform), typeof(Image), typeof(Mask));
        Stretch(viewport.GetComponent<RectTransform>(), 0);
        viewport.GetComponent<Image>().color = new Color(.91f, .85f, .70f, .84f);
        viewport.GetComponent<Image>().raycastTarget = true;
        viewport.GetComponent<Mask>().showMaskGraphic = false;
        var rowsObject = FindOrCreate("BazaarRows", viewport.transform, typeof(RectTransform), typeof(TextMeshProUGUI), typeof(ContentSizeFitter));
        var rowsRect = rowsObject.GetComponent<RectTransform>();
        rowsRect.anchorMin = new Vector2(0, 1);
        rowsRect.anchorMax = Vector2.one;
        rowsRect.pivot = new Vector2(.5f, 1);
        rowsRect.anchoredPosition = Vector2.zero;
        rowsRect.sizeDelta = Vector2.zero;
        var rows = rowsObject.GetComponent<TextMeshProUGUI>();
        rows.font = TMP_Settings.defaultFontAsset;
        rows.fontSize = 13;
        rows.color = new Color(.20f, .16f, .10f);
        rows.richText = false;
        rows.enableWordWrapping = true;
        rows.overflowMode = TextOverflowModes.Overflow;
        rows.lineSpacing = 5;
        rows.margin = new Vector4(12, 10, 12, 8);
        rows.raycastTarget = false;
        var fitter = rowsObject.GetComponent<ContentSizeFitter>();
        fitter.horizontalFit = ContentSizeFitter.FitMode.Unconstrained;
        fitter.verticalFit = ContentSizeFitter.FitMode.PreferredSize;
        var scroll = scrollObject.GetComponent<ScrollRect>();
        scroll.viewport = viewport.GetComponent<RectTransform>();
        scroll.content = rowsRect;
        scroll.horizontal = false;
        scroll.vertical = true;
        scroll.movementType = ScrollRect.MovementType.Clamped;
        scroll.scrollSensitivity = 28;

        var status = Text("BazaarStatus", paper.transform, string.Empty, 12, new Color(.30f, .22f, .12f));
        status.alignment = TextAlignmentOptions.MidlineLeft;
        status.rectTransform.anchorMin = new Vector2(0, 0);
        status.rectTransform.anchorMax = new Vector2(1, 0);
        status.rectTransform.pivot = new Vector2(.5f, 0);
        status.rectTransform.anchoredPosition = new Vector2(0, 52);
        status.rectTransform.sizeDelta = new Vector2(-30, 34);

        var search = Button("BazaarSearchButton", paper.transform, "SEARCH / REFRESH", new Color(.39f, .27f, .14f, 1));
        AnchorBottomLeft(search.GetComponent<RectTransform>(), 14, 148, 34);
        var page = Text("BazaarPage", paper.transform, "Page 1", 12, new Color(.27f, .20f, .12f));
        page.alignment = TextAlignmentOptions.Center;
        page.rectTransform.anchorMin = new Vector2(.5f, 0);
        page.rectTransform.anchorMax = page.rectTransform.pivot = new Vector2(.5f, 0);
        page.rectTransform.anchoredPosition = new Vector2(50, 16);
        page.rectTransform.sizeDelta = new Vector2(76, 26);
        var previous = Button("BazaarPreviousPageButton", paper.transform, "PREV", new Color(.44f, .32f, .17f, 1));
        AnchorBottomCenter(previous.GetComponent<RectTransform>(), -32, 68, 32);
        var next = Button("BazaarNextPageButton", paper.transform, "NEXT", new Color(.44f, .32f, .17f, 1));
        AnchorBottomCenter(next.GetComponent<RectTransform>(), 132, 68, 32);

        var screen = canvas.GetComponent<C09BazaarScreen>() ?? Undo.AddComponent<C09BazaarScreen>(canvas.gameObject);
        var serialized = new SerializedObject(screen);
        serialized.FindProperty("loginScreen").objectReferenceValue = login;
        serialized.FindProperty("worldScreen").objectReferenceValue = world;
        serialized.FindProperty("bazaarPanel").objectReferenceValue = panel;
        serialized.FindProperty("marketButton").objectReferenceValue = market;
        serialized.FindProperty("itemTypeButtons").arraySize = typeButtons.Length;
        for (int i = 0; i < typeButtons.Length; i++) serialized.FindProperty("itemTypeButtons").GetArrayElementAtIndex(i).objectReferenceValue = typeButtons[i];
        serialized.FindProperty("searchButton").objectReferenceValue = search;
        serialized.FindProperty("previousPageButton").objectReferenceValue = previous;
        serialized.FindProperty("nextPageButton").objectReferenceValue = next;
        serialized.FindProperty("closeButton").objectReferenceValue = close;
        serialized.FindProperty("pageText").objectReferenceValue = page;
        serialized.FindProperty("rowsText").objectReferenceValue = rows;
        serialized.FindProperty("statusText").objectReferenceValue = status;
        serialized.ApplyModifiedPropertiesWithoutUndo();

        var combatSerialized = new SerializedObject(combat);
        combatSerialized.FindProperty("bazaarScreen").objectReferenceValue = screen;
        combatSerialized.ApplyModifiedPropertiesWithoutUndo();
        panel.SetActive(false);
        market.gameObject.SetActive(false);
        EditorSceneManager.MarkSceneDirty(SceneManager.GetActiveScene());
        EditorSceneManager.SaveScene(SceneManager.GetActiveScene());
    }

    private static GameObject FindOrCreate(string name, Transform parent, params System.Type[] components)
    {
        var existing = parent.Find(name);
        if (existing != null) return existing.gameObject;
        var created = new GameObject(name, components);
        created.transform.SetParent(parent, false);
        return created;
    }

    private static TMP_Text Text(string name, Transform parent, string value, int size, Color color)
    {
        var text = FindOrCreate(name, parent, typeof(RectTransform), typeof(TextMeshProUGUI)).GetComponent<TextMeshProUGUI>();
        text.font = TMP_Settings.defaultFontAsset;
        text.text = value;
        text.fontSize = size;
        text.color = color;
        text.richText = false;
        text.raycastTarget = false;
        return text;
    }

    private static Button Button(string name, Transform parent, string label, Color color)
    {
        var button = FindOrCreate(name, parent, typeof(RectTransform), typeof(Image), typeof(Button)).GetComponent<Button>();
        var image = button.GetComponent<Image>();
        image.color = color;
        button.targetGraphic = image;
        var text = Text("Label", button.transform, label, 12, new Color(.98f, .88f, .67f));
        text.alignment = TextAlignmentOptions.Center;
        Stretch(text.rectTransform, 2);
        return button;
    }

    private static void Stretch(RectTransform rect, int margin) => Stretch(rect, new RectOffset(margin, margin, margin, margin));
    private static void Stretch(RectTransform rect, RectOffset padding)
    {
        rect.anchorMin = Vector2.zero;
        rect.anchorMax = Vector2.one;
        rect.offsetMin = new Vector2(padding.left, padding.bottom);
        rect.offsetMax = new Vector2(-padding.right, -padding.top);
    }
    private static void SetTop(RectTransform rect, float inset, float height)
    {
        rect.anchorMin = new Vector2(0, 1);
        rect.anchorMax = new Vector2(1, 1);
        rect.pivot = new Vector2(.5f, 1);
        rect.anchoredPosition = new Vector2(0, -inset);
        rect.sizeDelta = new Vector2(-24, height);
    }
    private static void AnchorTopRight(RectTransform rect, float inset, float width, float height)
    {
        rect.anchorMin = rect.anchorMax = rect.pivot = new Vector2(1, 1);
        rect.anchoredPosition = new Vector2(-inset, -inset);
        rect.sizeDelta = new Vector2(width, height);
    }
    private static void AnchorBottomLeft(RectTransform rect, float bottom, float width, float height)
    {
        rect.anchorMin = rect.anchorMax = rect.pivot = new Vector2(0, 0);
        rect.anchoredPosition = new Vector2(14, bottom);
        rect.sizeDelta = new Vector2(width, height);
    }
    private static void AnchorBottomCenter(RectTransform rect, float x, float width, float height)
    {
        rect.anchorMin = rect.anchorMax = rect.pivot = new Vector2(.5f, 0);
        rect.anchoredPosition = new Vector2(x, 14);
        rect.sizeDelta = new Vector2(width, height);
    }
}
