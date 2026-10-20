using C01;
using TMPro;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

public static class C17EntrySetup
{
    private static readonly Color PanelColor = new Color(.115f, .105f, .087f, 1f);
    private static readonly Color ButtonColor = new Color(.16f, .32f, .32f, .93f);
    private static readonly Color TextColor = new Color(.89f, .84f, .70f);

    [MenuItem("C02/C17/Setup Entry Screens")]
    public static void Setup()
    {
        if (EditorApplication.isPlaying)
            throw new System.InvalidOperationException("Stop Play before setting up C17 entry screens.");
        if (EditorSceneManager.GetActiveScene().path != "Assets/C02/C02.unity")
            throw new System.InvalidOperationException("Open the C02 scene before setting up C17 entry screens.");

        GameObject panelObject = GameObject.Find("C01Panel");
        GameObject loginObject = GameObject.Find("C01Client");
        GameObject worldObject = GameObject.Find("C02Client");
        if (panelObject == null || loginObject == null || worldObject == null)
            throw new System.InvalidOperationException("Expected the existing C01Panel, C01Client, and C02Client.");

        Transform panel = panelObject.transform;
        Transform signIn = panel.Find("SignIn");
        Transform results = panel.Find("Results");
        C01LoginScreen login = loginObject.GetComponent<C01LoginScreen>();
        C02.C02WorldScreen world = worldObject.GetComponent<C02.C02WorldScreen>();
        TMP_Text status = FindText(panel, "StatusText");
        if (signIn == null || results == null || login == null || world == null || status == null)
            throw new System.InvalidOperationException("Expected existing SignIn, Results, StatusText, login, and world objects.");

        if (status.transform.parent != signIn)
        {
            Undo.SetTransformParent(status.transform, signIn, "Move login status into SignIn");
            status.transform.SetAsLastSibling();
        }
        SetPanel(signIn.GetComponent<RectTransform>());
        SetPanel(results.GetComponent<RectTransform>());
        signIn.GetComponent<RectTransform>().sizeDelta = new Vector2(420, 370);
        TMP_Text note = FindText(signIn, "EndpointNote");
        if (note != null)
        {
            Undo.RecordObject(note, "Update separated sign-in guidance");
            note.text = "Sign in to continue.";
        }

        Transform loadingTransform = panel.Find("LoadingPanel");
        GameObject loadingPanel = loadingTransform != null ? loadingTransform.gameObject : CreateLoadingPanel(panel);
        TMP_Text loadingText = loadingPanel.transform.Find("LoadingText").GetComponent<TMP_Text>();
        UnityEngine.UI.Button returnButton = FindReturnButton(panel);
        if (returnButton == null) returnButton = CreateReturnButton(loadingPanel.transform);
        else if (returnButton.transform.parent != loadingPanel.transform)
            Undo.SetTransformParent(returnButton.transform, loadingPanel.transform, "Place return button in LoadingPanel");
        SetLoadingPanel(loadingPanel.GetComponent<RectTransform>());
        loadingPanel.SetActive(false);
        signIn.gameObject.SetActive(true);
        results.gameObject.SetActive(false);

        Undo.RecordObject(login, "Bind C17 login status");
        var loginSerialized = new SerializedObject(login);
        loginSerialized.FindProperty("statusText").objectReferenceValue = status;
        loginSerialized.ApplyModifiedProperties();

        C02.C17EntryPresentation presentation = loginObject.GetComponent<C02.C17EntryPresentation>();
        if (presentation == null) presentation = Undo.AddComponent<C02.C17EntryPresentation>(loginObject);
        Undo.RecordObject(presentation, "Bind C17 entry presentation");
        var refs = new SerializedObject(presentation);
        refs.FindProperty("login").objectReferenceValue = login;
        refs.FindProperty("world").objectReferenceValue = world;
        refs.FindProperty("signIn").objectReferenceValue = signIn.gameObject;
        refs.FindProperty("results").objectReferenceValue = results.gameObject;
        refs.FindProperty("loadingPanel").objectReferenceValue = loadingPanel;
        refs.FindProperty("loadingText").objectReferenceValue = loadingText;
        refs.FindProperty("returnButton").objectReferenceValue = returnButton;
        refs.ApplyModifiedProperties();

        EditorSceneManager.MarkSceneDirty(EditorSceneManager.GetActiveScene());
        Debug.Log("C17 entry screens are configured in the active C02 scene. Review and save the scene separately.");
    }

    private static TMP_Text FindText(Transform root, string name)
    {
        foreach (TMP_Text text in root.GetComponentsInChildren<TMP_Text>(true))
            if (text.name == name) return text;
        return null;
    }

    private static void SetPanel(RectTransform rect)
    {
        if (rect == null) throw new System.InvalidOperationException("Expected a RectTransform for an entry panel.");
        Undo.RecordObject(rect, "Place entry stage panel");
        rect.anchorMin = rect.anchorMax = new Vector2(.8f, .5f);
        rect.pivot = new Vector2(.5f, .5f);
        rect.anchoredPosition = Vector2.zero;
        rect.sizeDelta = new Vector2(420, 500);
        UnityEngine.UI.Image image = rect.GetComponent<UnityEngine.UI.Image>();
        if (image != null)
        {
            Undo.RecordObject(image, "Style entry stage panel");
            image.color = PanelColor;
        }
    }

    private static GameObject CreateLoadingPanel(Transform parent)
    {
        var rect = new GameObject("LoadingPanel", typeof(RectTransform), typeof(UnityEngine.UI.Image), typeof(UnityEngine.UI.VerticalLayoutGroup)).GetComponent<RectTransform>();
        Undo.RegisterCreatedObjectUndo(rect.gameObject, "Create C17 LoadingPanel");
        rect.SetParent(parent, false);
        var image = rect.GetComponent<UnityEngine.UI.Image>();
        image.color = PanelColor;
        image.raycastTarget = true;

        var layout = rect.GetComponent<UnityEngine.UI.VerticalLayoutGroup>();
        layout.padding = new RectOffset(24, 24, 24, 24);
        layout.spacing = 18;
        layout.childControlWidth = layout.childControlHeight = true;
        layout.childForceExpandWidth = true;
        layout.childForceExpandHeight = false;

        var labelObject = new GameObject("LoadingText", typeof(RectTransform), typeof(CanvasRenderer), typeof(TextMeshProUGUI), typeof(UnityEngine.UI.LayoutElement));
        Undo.RegisterCreatedObjectUndo(labelObject, "Create C17 loading text");
        labelObject.transform.SetParent(rect, false);
        var label = labelObject.GetComponent<TextMeshProUGUI>();
        label.font = TMP_Settings.defaultFontAsset;
        label.fontSize = 17;
        label.color = TextColor;
        label.alignment = TextAlignmentOptions.Center;
        label.enableWordWrapping = true;
        label.raycastTarget = false;
        label.text = "Waiting for the server…";
        var labelLayout = labelObject.GetComponent<UnityEngine.UI.LayoutElement>();
        labelLayout.minHeight = 78;
        labelLayout.preferredHeight = 110;
        SetLoadingPanel(rect);
        rect.gameObject.SetActive(false);
        return rect.gameObject;
    }

    private static UnityEngine.UI.Button FindReturnButton(Transform root)
    {
        foreach (UnityEngine.UI.Button button in root.GetComponentsInChildren<UnityEngine.UI.Button>(true))
            if (button.name == "EntryReturnButton") return button;
        return null;
    }

    private static UnityEngine.UI.Button CreateReturnButton(Transform parent)
    {
        var buttonObject = new GameObject("EntryReturnButton", typeof(RectTransform), typeof(UnityEngine.UI.Image), typeof(UnityEngine.UI.Button), typeof(UnityEngine.UI.LayoutElement));
        Undo.RegisterCreatedObjectUndo(buttonObject, "Create C17 return button");
        buttonObject.transform.SetParent(parent, false);
        var image = buttonObject.GetComponent<UnityEngine.UI.Image>();
        image.color = ButtonColor;
        image.raycastTarget = true;
        var button = buttonObject.GetComponent<UnityEngine.UI.Button>();
        button.targetGraphic = image;
        var colors = button.colors;
        colors.normalColor = ButtonColor;
        colors.highlightedColor = new Color(.22f, .40f, .39f, 1f);
        colors.pressedColor = new Color(.11f, .24f, .24f, 1f);
        colors.selectedColor = colors.normalColor;
        button.colors = colors;
        var layout = buttonObject.GetComponent<UnityEngine.UI.LayoutElement>();
        layout.minHeight = layout.preferredHeight = 46;

        var labelObject = new GameObject("Label", typeof(RectTransform), typeof(CanvasRenderer), typeof(TextMeshProUGUI));
        Undo.RegisterCreatedObjectUndo(labelObject, "Create C17 return button label");
        labelObject.transform.SetParent(buttonObject.transform, false);
        var label = labelObject.GetComponent<TextMeshProUGUI>();
        label.font = TMP_Settings.defaultFontAsset;
        label.fontSize = 15;
        label.color = TextColor;
        label.alignment = TextAlignmentOptions.Center;
        label.text = "Change account";
        label.raycastTarget = false;
        var labelRect = label.rectTransform;
        labelRect.anchorMin = Vector2.zero;
        labelRect.anchorMax = Vector2.one;
        labelRect.offsetMin = new Vector2(8, 2);
        labelRect.offsetMax = new Vector2(-8, -2);
        return button;
    }

    private static void SetLoadingPanel(RectTransform rect)
    {
        Undo.RecordObject(rect, "Place C17 LoadingPanel");
        rect.anchorMin = rect.anchorMax = new Vector2(.8f, .5f);
        rect.pivot = new Vector2(.5f, .5f);
        rect.anchoredPosition = Vector2.zero;
        rect.sizeDelta = new Vector2(420, 240);
    }
}
