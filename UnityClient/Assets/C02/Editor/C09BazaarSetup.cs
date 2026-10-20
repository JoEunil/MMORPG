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
            rect.anchoredPosition = new Vector2(15 + i * 154, -98);
            rect.sizeDelta = new Vector2(142, 34);
            typeButtons[i].GetComponentInChildren<TMP_Text>(true).fontSize = 10;
        }

        var searchListings = Button("BazaarSearchListingsButton", paper.transform, "SALES", new Color(.42f, .30f, .16f, 1));
        AnchorTopLeft(searchListings.GetComponent<RectTransform>(), 15, 58, 226, 34);
        var myListings = Button("BazaarMyListingsButton", paper.transform, "MY LISTINGS", new Color(.42f, .30f, .16f, 1));
        AnchorTopLeft(myListings.GetComponent<RectTransform>(), 249, 58, 226, 34);

        Header("BazaarItemHeader", paper.transform, "ITEM", .10f, .48f, TextAlignmentOptions.MidlineLeft);
        Header("BazaarQuantityHeader", paper.transform, "QTY", .49f, .61f, TextAlignmentOptions.Center);
        Header("BazaarPriceHeader", paper.transform, "PRICE", .62f, .81f, TextAlignmentOptions.MidlineRight);
        Header("BazaarStateHeader", paper.transform, "STATE", .82f, .99f, TextAlignmentOptions.Center);

        var scrollObject = FindOrCreate("BazaarScroll", paper.transform, typeof(RectTransform), typeof(ScrollRect));
        Stretch(scrollObject.GetComponent<RectTransform>(), new RectOffset(14, 14, 158, 232));
        var viewport = FindOrCreate("BazaarViewport", scrollObject.transform, typeof(RectTransform), typeof(Image), typeof(Mask));
        Stretch(viewport.GetComponent<RectTransform>(), 0);
        viewport.GetComponent<Image>().color = new Color(.91f, .85f, .70f, .84f);
        viewport.GetComponent<Image>().raycastTarget = true;
        viewport.GetComponent<Mask>().showMaskGraphic = false;
        var rowsObject = FindOrCreate("BazaarRows", viewport.transform, typeof(RectTransform));
        var oldRowsText = rowsObject.GetComponent<TextMeshProUGUI>();
        if (oldRowsText != null) Undo.DestroyObjectImmediate(oldRowsText);
        var rowsFitter = rowsObject.GetComponent<ContentSizeFitter>() ?? Undo.AddComponent<ContentSizeFitter>(rowsObject);
        var rowsLayout = rowsObject.GetComponent<VerticalLayoutGroup>() ?? Undo.AddComponent<VerticalLayoutGroup>(rowsObject);
        var rowsRect = rowsObject.GetComponent<RectTransform>();
        rowsRect.anchorMin = new Vector2(0, 1);
        rowsRect.anchorMax = new Vector2(1, 1);
        rowsRect.pivot = new Vector2(.5f, 1);
        rowsRect.anchoredPosition = Vector2.zero;
        rowsRect.sizeDelta = new Vector2(0, 0);
        rowsLayout.padding = new RectOffset(6, 6, 5, 5);
        rowsLayout.spacing = 4;
        rowsLayout.childControlWidth = true;
        rowsLayout.childControlHeight = true;
        rowsLayout.childForceExpandWidth = true;
        rowsLayout.childForceExpandHeight = false;
        rowsFitter.horizontalFit = ContentSizeFitter.FitMode.Unconstrained;
        rowsFitter.verticalFit = ContentSizeFitter.FitMode.PreferredSize;
        var scroll = scrollObject.GetComponent<ScrollRect>();
        scroll.viewport = viewport.GetComponent<RectTransform>();
        scroll.content = rowsRect;
        scroll.horizontal = false;
        scroll.vertical = true;
        scroll.movementType = ScrollRect.MovementType.Clamped;
        scroll.scrollSensitivity = 28;

        var empty = Text("BazaarEmptyState", viewport.transform, "No listings on this page.", 14, new Color(.38f, .31f, .20f));
        empty.alignment = TextAlignmentOptions.Center;
        Stretch(empty.rectTransform, 18);
        empty.gameObject.SetActive(false);

        var detailPanel = FindOrCreate("BazaarSelectedDetailPanel", paper.transform, typeof(RectTransform), typeof(Image));
        AnchorBottomStretch(detailPanel.GetComponent<RectTransform>(), 132, 14, 88);
        detailPanel.GetComponent<Image>().color = new Color(.90f, .84f, .70f, 1);
        var detail = Text("BazaarSelectedDetail", detailPanel.transform, "Select a listing to see its details.", 11,
            new Color(.28f, .22f, .14f));
        detail.alignment = TextAlignmentOptions.MidlineLeft;
        Stretch(detail.rectTransform, 8);

        var status = Text("BazaarStatus", paper.transform, string.Empty, 12, new Color(.30f, .22f, .12f));
        status.alignment = TextAlignmentOptions.MidlineLeft;
        status.rectTransform.anchorMin = new Vector2(0, 0);
        status.rectTransform.anchorMax = new Vector2(1, 0);
        status.rectTransform.pivot = new Vector2(.5f, 0);
        status.rectTransform.anchoredPosition = new Vector2(0, 54);
        status.rectTransform.sizeDelta = new Vector2(-30, 28);

        var search = Button("BazaarSearchButton", paper.transform, "SEARCH / REFRESH", new Color(.39f, .27f, .14f, 1));
        AnchorBottomLeft(search.GetComponent<RectTransform>(), 14, 128, 34);
        var claim = Button("BazaarClaimButton", paper.transform, "COLLECT PROCEEDS", new Color(.24f, .36f, .22f, 1));
        AnchorBottomRight(claim.GetComponent<RectTransform>(), 14, 190, 34);
        claim.gameObject.SetActive(false);
        var page = Text("BazaarPage", paper.transform, "Page 1", 12, new Color(.27f, .20f, .12f));
        page.alignment = TextAlignmentOptions.Center;
        page.rectTransform.anchorMin = new Vector2(1, 0);
        page.rectTransform.anchorMax = page.rectTransform.pivot = new Vector2(1, 0);
        page.rectTransform.anchoredPosition = new Vector2(-150, 18);
        page.rectTransform.sizeDelta = new Vector2(60, 26);
        var previous = Button("BazaarPreviousPageButton", paper.transform, "PREV", new Color(.44f, .32f, .17f, 1));
        AnchorBottomRight(previous.GetComponent<RectTransform>(), 82, 60, 32);
        var next = Button("BazaarNextPageButton", paper.transform, "NEXT", new Color(.44f, .32f, .17f, 1));
        AnchorBottomRight(next.GetComponent<RectTransform>(), 14, 60, 32);

        var screen = canvas.GetComponent<C09BazaarScreen>() ?? Undo.AddComponent<C09BazaarScreen>(canvas.gameObject);
        var serialized = new SerializedObject(screen);
        serialized.FindProperty("loginScreen").objectReferenceValue = login;
        serialized.FindProperty("worldScreen").objectReferenceValue = world;
        serialized.FindProperty("bazaarPanel").objectReferenceValue = panel;
        serialized.FindProperty("marketButton").objectReferenceValue = market;
        serialized.FindProperty("searchListingsButton").objectReferenceValue = searchListings;
        serialized.FindProperty("myListingsButton").objectReferenceValue = myListings;
        serialized.FindProperty("itemTypeButtons").arraySize = typeButtons.Length;
        for (int i = 0; i < typeButtons.Length; i++) serialized.FindProperty("itemTypeButtons").GetArrayElementAtIndex(i).objectReferenceValue = typeButtons[i];
        serialized.FindProperty("searchButton").objectReferenceValue = search;
        serialized.FindProperty("claimButton").objectReferenceValue = claim;
        serialized.FindProperty("previousPageButton").objectReferenceValue = previous;
        serialized.FindProperty("nextPageButton").objectReferenceValue = next;
        serialized.FindProperty("closeButton").objectReferenceValue = close;
        serialized.FindProperty("pageText").objectReferenceValue = page;
        serialized.FindProperty("listingRows").objectReferenceValue = rowsRect;
        serialized.FindProperty("emptyStateText").objectReferenceValue = empty;
        serialized.FindProperty("selectedDetailText").objectReferenceValue = detail;
        serialized.FindProperty("inventoryScreen").objectReferenceValue = canvas.GetComponent<C07InventoryScreen>();
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
    private static void AnchorTopLeft(RectTransform rect, float x, float inset, float width, float height)
    {
        rect.anchorMin = rect.anchorMax = rect.pivot = new Vector2(0, 1);
        rect.anchoredPosition = new Vector2(x, -inset);
        rect.sizeDelta = new Vector2(width, height);
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
    private static void AnchorBottomRight(RectTransform rect, float inset, float width, float height)
    {
        rect.anchorMin = rect.anchorMax = rect.pivot = new Vector2(1, 0);
        rect.anchoredPosition = new Vector2(-inset, 14);
        rect.sizeDelta = new Vector2(width, height);
    }
    private static void AnchorBottomStretch(RectTransform rect, float bottom, float horizontalInset, float height)
    {
        rect.anchorMin = new Vector2(0, 0);
        rect.anchorMax = new Vector2(1, 0);
        rect.pivot = new Vector2(.5f, 0);
        rect.anchoredPosition = new Vector2(0, bottom);
        rect.sizeDelta = new Vector2(-horizontalInset * 2, height);
    }
    private static void Header(string name, Transform parent, string value, float left, float right, TextAlignmentOptions alignment)
    {
        var text = Text(name, parent, value, 9, new Color(.38f, .29f, .17f));
        text.alignment = alignment;
        var rect = text.rectTransform;
        rect.anchorMin = new Vector2(left, 1);
        rect.anchorMax = new Vector2(right, 1);
        rect.pivot = new Vector2(.5f, 1);
        rect.anchoredPosition = new Vector2(0, -132);
        rect.sizeDelta = new Vector2(0, 22);
    }
}
