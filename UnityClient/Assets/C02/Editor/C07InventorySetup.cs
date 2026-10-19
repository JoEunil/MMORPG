using C01;
using C02;
using TMPro;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
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
        var hud = canvas != null ? canvas.transform.Find("WorldHud") : null;
        if (canvas == null || canvas.renderMode != RenderMode.ScreenSpaceOverlay || login == null || world == null || hud == null)
            throw new System.InvalidOperationException("Expected the existing C02 overlay Canvas, WorldHud, C01LoginScreen, and C02WorldScreen.");

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

        var scrollObject = FindOrCreate("InventoryScroll", inner.transform, typeof(RectTransform), typeof(ScrollRect));
        var scrollRect = scrollObject.GetComponent<RectTransform>();
        Stretch(scrollRect, new RectOffset(15, 15, 72, 102));
        var viewportObject = FindOrCreate("InventoryViewport", scrollObject.transform, typeof(RectTransform), typeof(Image), typeof(Mask));
        var viewportRect = viewportObject.GetComponent<RectTransform>();
        Stretch(viewportRect, 0);
        viewportObject.GetComponent<Image>().color = new Color(.89f, .83f, .68f, .75f);
        viewportObject.GetComponent<Image>().raycastTarget = true;
        viewportObject.GetComponent<Mask>().showMaskGraphic = false;

        var rowsObject = FindOrCreate("InventoryRows", viewportObject.transform, typeof(RectTransform), typeof(TextMeshProUGUI), typeof(ContentSizeFitter));
        var rowsRect = rowsObject.GetComponent<RectTransform>();
        rowsRect.anchorMin = new Vector2(0, 1);
        rowsRect.anchorMax = Vector2.one;
        rowsRect.pivot = new Vector2(.5f, 1);
        rowsRect.anchoredPosition = Vector2.zero;
        rowsRect.sizeDelta = Vector2.zero;
        var rows = rowsObject.GetComponent<TextMeshProUGUI>();
        rows.font = TMP_Settings.defaultFontAsset;
        rows.fontSize = 15;
        rows.color = new Color(.22f, .18f, .12f);
        rows.richText = false;
        rows.enableWordWrapping = false;
        rows.overflowMode = TextOverflowModes.Overflow;
        rows.lineSpacing = 8;
        rows.alignment = TextAlignmentOptions.TopLeft;
        rows.margin = new Vector4(12, 10, 8, 8);
        rows.raycastTarget = false;
        var fitter = rowsObject.GetComponent<ContentSizeFitter>();
        fitter.horizontalFit = ContentSizeFitter.FitMode.Unconstrained;
        fitter.verticalFit = ContentSizeFitter.FitMode.PreferredSize;

        var scroll = scrollObject.GetComponent<ScrollRect>();
        scroll.viewport = viewportRect;
        scroll.content = rowsRect;
        scroll.horizontal = false;
        scroll.vertical = true;
        scroll.movementType = ScrollRect.MovementType.Clamped;
        scroll.scrollSensitivity = 28;

        var status = Text("InventoryStatus", inner.transform, string.Empty, 12, new Color(.31f, .24f, .15f));
        status.alignment = TextAlignmentOptions.MidlineLeft;
        status.enableWordWrapping = true;
        status.rectTransform.anchorMin = Vector2.zero;
        status.rectTransform.anchorMax = new Vector2(1, 0);
        status.rectTransform.pivot = new Vector2(.5f, 0);
        status.rectTransform.anchoredPosition = new Vector2(0, 54);
        status.rectTransform.sizeDelta = new Vector2(-30, 40);

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
        serialized.FindProperty("inventoryPanel").objectReferenceValue = panel;
        serialized.FindProperty("bagButton").objectReferenceValue = bag;
        serialized.FindProperty("refreshButton").objectReferenceValue = refresh;
        serialized.FindProperty("closeButton").objectReferenceValue = close;
        serialized.FindProperty("inventoryRows").objectReferenceValue = rows;
        serialized.FindProperty("inventoryStatus").objectReferenceValue = status;
        serialized.ApplyModifiedPropertiesWithoutUndo();
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
