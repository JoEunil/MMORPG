using C01;
using C02;
using TMPro;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.SceneManagement;
using UnityEngine.UI;

public static class C07InventorySetup
{
    public static void Apply()
    {
        if (EditorApplication.isPlaying) throw new System.InvalidOperationException("Stop Play before applying C07 inventory UI.");
        if (EditorSceneManager.GetActiveScene().path != "Assets/C02/C02.unity")
            throw new System.InvalidOperationException("Open the C02 scene first.");

        var canvas = Object.FindFirstObjectByType<Canvas>();
        var login = Object.FindFirstObjectByType<C01LoginScreen>();
        var world = Object.FindFirstObjectByType<C02WorldScreen>();
        var combat = Object.FindFirstObjectByType<C05CombatScreen>();
        var bazaar = Object.FindFirstObjectByType<C09BazaarScreen>();
        var eventSystems = Object.FindObjectsByType<EventSystem>(FindObjectsSortMode.None);
        var hud = canvas != null ? canvas.transform.Find("WorldHud") : null;
        if (canvas == null || canvas.renderMode != RenderMode.ScreenSpaceOverlay || canvas.GetComponent<GraphicRaycaster>() == null ||
            login == null || world == null || combat == null || bazaar == null || hud == null || eventSystems.Length != 1)
            throw new System.InvalidOperationException("Expected the C02 overlay Canvas, one EventSystem, WorldHud, login, world, combat, and bazaar screens.");

        var bag = Button("C07BagButton", canvas.transform, "BAG", new Color(.48f, .34f, .15f, .98f));
        var bagRect = bag.GetComponent<RectTransform>();
        bagRect.anchorMin = bagRect.anchorMax = bagRect.pivot = new Vector2(1, 1);
        bagRect.anchoredPosition = new Vector2(-24, -24);
        bagRect.sizeDelta = new Vector2(118, 42);
        bag.GetComponentInChildren<TMP_Text>(true).fontSize = 14;
        bag.GetComponentInChildren<TMP_Text>(true).color = new Color(.98f, .89f, .66f);

        var panel = FindOrCreate("C07InventoryPanel", canvas.transform, typeof(RectTransform), typeof(Image));
        var panelRect = panel.GetComponent<RectTransform>();
        panelRect.anchorMin = panelRect.anchorMax = panelRect.pivot = new Vector2(1, 0);
        panelRect.anchoredPosition = new Vector2(-24, 22);
        panelRect.sizeDelta = new Vector2(450, 560);
        var frame = panel.GetComponent<Image>();
        frame.color = new Color(.54f, .39f, .18f, .98f);
        frame.raycastTarget = true;

        var inner = FindOrCreate("InventoryParchment", panel.transform, typeof(RectTransform), typeof(Image));
        Stretch(inner.GetComponent<RectTransform>(), 4);
        var paper = inner.GetComponent<Image>();
        paper.color = new Color(.78f, .70f, .51f, 1f);
        paper.raycastTarget = true;

        var title = Text("InventoryTitle", inner.transform, "INVENTORY", 20, new Color(.24f, .19f, .12f));
        title.alignment = TextAlignmentOptions.MidlineLeft;
        SetTop(title.rectTransform, 15, 32);
        var close = Button("InventoryCloseButton", inner.transform, "CLOSE", new Color(.40f, .29f, .17f, 1f));
        var closeRect = close.GetComponent<RectTransform>();
        closeRect.anchorMin = closeRect.anchorMax = closeRect.pivot = new Vector2(1, 1);
        closeRect.anchoredPosition = new Vector2(-12, -12);
        closeRect.sizeDelta = new Vector2(86, 32);
        close.GetComponentInChildren<TMP_Text>(true).fontSize = 11;

        var oldRows = inner.transform.Find("InventoryRows");
        if (oldRows != null) Undo.DestroyObjectImmediate(oldRows.gameObject);

        var scrollObject = FindOrCreate("InventoryScroll", inner.transform, typeof(RectTransform), typeof(ScrollRect));
        var scrollRect = scrollObject.GetComponent<RectTransform>();
        Stretch(scrollRect, new RectOffset(15, 15, 72, 208));
        var viewportObject = FindOrCreate("InventoryViewport", scrollObject.transform, typeof(RectTransform), typeof(Image), typeof(Mask));
        var viewportRect = viewportObject.GetComponent<RectTransform>();
        Stretch(viewportRect, 0);
        viewportObject.GetComponent<Image>().color = new Color(.89f, .83f, .68f, .75f);
        viewportObject.GetComponent<Image>().raycastTarget = true;
        viewportObject.GetComponent<Mask>().showMaskGraphic = false;

        var gridObject = FindOrCreate("InventoryGridContent", viewportObject.transform,
            typeof(RectTransform), typeof(GridLayoutGroup), typeof(ContentSizeFitter));
        var gridRect = gridObject.GetComponent<RectTransform>();
        gridRect.anchorMin = new Vector2(0, 1);
        gridRect.anchorMax = Vector2.one;
        gridRect.pivot = new Vector2(.5f, 1);
        gridRect.anchoredPosition = Vector2.zero;
        gridRect.sizeDelta = Vector2.zero;
        var grid = gridObject.GetComponent<GridLayoutGroup>();
        grid.cellSize = new Vector2(120, 78);
        grid.spacing = new Vector2(8, 8);
        grid.padding = new RectOffset(10, 10, 10, 10);
        grid.startCorner = GridLayoutGroup.Corner.UpperLeft;
        grid.startAxis = GridLayoutGroup.Axis.Horizontal;
        grid.childAlignment = TextAnchor.UpperLeft;
        grid.constraint = GridLayoutGroup.Constraint.FixedColumnCount;
        grid.constraintCount = 3;
        var fitter = gridObject.GetComponent<ContentSizeFitter>();
        fitter.horizontalFit = ContentSizeFitter.FitMode.Unconstrained;
        fitter.verticalFit = ContentSizeFitter.FitMode.PreferredSize;

        var scroll = scrollObject.GetComponent<ScrollRect>();
        scroll.viewport = viewportRect;
        scroll.content = gridRect;
        scroll.horizontal = false;
        scroll.vertical = true;
        scroll.movementType = ScrollRect.MovementType.Clamped;
        scroll.scrollSensitivity = 28;

        var details = FindOrCreate("InventoryDetails", inner.transform, typeof(RectTransform), typeof(Image));
        var detailsRect = details.GetComponent<RectTransform>();
        detailsRect.anchorMin = Vector2.zero;
        detailsRect.anchorMax = new Vector2(1, 0);
        detailsRect.pivot = new Vector2(.5f, 0);
        detailsRect.anchoredPosition = new Vector2(0, 52);
        detailsRect.sizeDelta = new Vector2(-30, 128);
        details.GetComponent<Image>().color = new Color(.88f, .81f, .65f, .92f);
        details.GetComponent<Image>().raycastTarget = false;

        var badge = Text("InventoryDetailBadge", details.transform, string.Empty, 13, Color.white);
        badge.alignment = TextAlignmentOptions.Center;
        badge.rectTransform.anchorMin = badge.rectTransform.anchorMax = badge.rectTransform.pivot = new Vector2(0, 1);
        badge.rectTransform.anchoredPosition = new Vector2(10, -10);
        badge.rectTransform.sizeDelta = new Vector2(30, 30);
        var itemName = Text("InventoryDetailName", details.transform, "Select an item", 16, new Color(.23f, .19f, .13f));
        itemName.alignment = TextAlignmentOptions.MidlineLeft;
        SetTop(itemName.rectTransform, 8, 32);
        itemName.rectTransform.anchoredPosition = new Vector2(20, -8);
        itemName.rectTransform.sizeDelta = new Vector2(-60, 32);
        var slot = Text("InventoryDetailSlot", details.transform, string.Empty, 12, new Color(.39f, .31f, .20f));
        slot.alignment = TextAlignmentOptions.MidlineLeft;
        SetTop(slot.rectTransform, 48, 22);
        slot.rectTransform.sizeDelta = new Vector2(-20, 22);
        var quantity = Text("InventoryDetailQuantity", details.transform, string.Empty, 12, new Color(.39f, .31f, .20f));
        quantity.alignment = TextAlignmentOptions.MidlineLeft;
        SetTop(quantity.rectTransform, 76, 22);
        quantity.rectTransform.sizeDelta = new Vector2(-20, 22);

        var status = Text("InventoryStatus", inner.transform, string.Empty, 12, new Color(.31f, .24f, .15f));
        status.alignment = TextAlignmentOptions.MidlineLeft;
        status.enableWordWrapping = true;
        status.rectTransform.anchorMin = status.rectTransform.anchorMax = status.rectTransform.pivot = Vector2.zero;
        status.rectTransform.anchoredPosition = new Vector2(15, 12);
        status.rectTransform.sizeDelta = new Vector2(260, 34);

        var refresh = Button("InventoryRefreshButton", inner.transform, "REFRESH", new Color(.40f, .29f, .17f, 1f));
        var refreshRect = refresh.GetComponent<RectTransform>();
        refreshRect.anchorMin = refreshRect.anchorMax = refreshRect.pivot = new Vector2(1, 0);
        refreshRect.anchoredPosition = new Vector2(-14, 12);
        refreshRect.sizeDelta = new Vector2(112, 34);
        refresh.GetComponentInChildren<TMP_Text>(true).fontSize = 11;

        var screen = canvas.GetComponent<C07InventoryScreen>() ?? Undo.AddComponent<C07InventoryScreen>(canvas.gameObject);
        var serialized = new SerializedObject(screen);
        serialized.FindProperty("loginScreen").objectReferenceValue = login;
        serialized.FindProperty("worldScreen").objectReferenceValue = world;
        serialized.FindProperty("bazaarScreen").objectReferenceValue = bazaar;
        serialized.FindProperty("inventoryPanel").objectReferenceValue = panel;
        serialized.FindProperty("bagButton").objectReferenceValue = bag;
        serialized.FindProperty("refreshButton").objectReferenceValue = refresh;
        serialized.FindProperty("closeButton").objectReferenceValue = close;
        serialized.FindProperty("inventoryScroll").objectReferenceValue = scroll;
        serialized.FindProperty("inventoryGridContent").objectReferenceValue = gridRect;
        serialized.FindProperty("detailBadge").objectReferenceValue = badge;
        serialized.FindProperty("detailName").objectReferenceValue = itemName;
        serialized.FindProperty("detailSlot").objectReferenceValue = slot;
        serialized.FindProperty("detailQuantity").objectReferenceValue = quantity;
        serialized.FindProperty("inventoryStatus").objectReferenceValue = status;
        serialized.ApplyModifiedPropertiesWithoutUndo();

        var worldSerialized = new SerializedObject(world);
        worldSerialized.FindProperty("inventoryScreen").objectReferenceValue = screen;
        worldSerialized.ApplyModifiedPropertiesWithoutUndo();
        var combatSerialized = new SerializedObject(combat);
        combatSerialized.FindProperty("inventoryScreen").objectReferenceValue = screen;
        combatSerialized.ApplyModifiedPropertiesWithoutUndo();
        panel.SetActive(false);
        bag.gameObject.SetActive(false);
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
        var go = FindOrCreate(name, parent, typeof(RectTransform), typeof(TextMeshProUGUI)).GetComponent<TextMeshProUGUI>();
        go.font = TMP_Settings.defaultFontAsset;
        go.text = value;
        go.fontSize = size;
        go.color = color;
        go.richText = false;
        go.raycastTarget = false;
        return go;
    }

    private static Button Button(string name, Transform parent, string label, Color color)
    {
        var button = FindOrCreate(name, parent, typeof(RectTransform), typeof(Image), typeof(Button)).GetComponent<Button>();
        var image = button.GetComponent<Image>();
        image.color = color;
        button.targetGraphic = image;
        var text = Text("Label", button.transform, label, 13, Color.white);
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
}
