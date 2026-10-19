using C01;
using TMPro;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.InputSystem.UI;
using UnityEngine.UI;

public static class C01SceneBuilder
{
    public static void Build()
    {
        BuildAt("Assets/C01/C01.unity");
    }

    public static void BuildAt(string path)
    {
        if (EditorApplication.isPlaying) throw new System.InvalidOperationException("Stop Play before building C01.");
        if (TMP_Settings.defaultFontAsset == null) throw new System.InvalidOperationException("Import TMP essentials first.");
        var scene = EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
        var camera = new GameObject("Main Camera", typeof(Camera)).GetComponent<Camera>();
        camera.tag = "MainCamera";
        camera.transform.position = new Vector3(0, 0, -10);
        camera.clearFlags = CameraClearFlags.SolidColor;
        camera.backgroundColor = new Color(0.035f, 0.055f, 0.09f);
        var canvasObject = new GameObject("Canvas", typeof(Canvas), typeof(CanvasScaler), typeof(GraphicRaycaster));
        canvasObject.GetComponent<Canvas>().renderMode = RenderMode.ScreenSpaceOverlay;
        var scaler = canvasObject.GetComponent<CanvasScaler>();
        scaler.uiScaleMode = CanvasScaler.ScaleMode.ScaleWithScreenSize;
        scaler.referenceResolution = new Vector2(1280, 720);
        scaler.matchWidthOrHeight = 0.5f;
        var background = Box("Background", canvasObject.transform, new Color(0.035f, 0.055f, 0.09f));
        Stretch(background, 0);
        var panel = Box("C01Panel", canvasObject.transform, new Color(0.075f, 0.1f, 0.15f));
        panel.sizeDelta = new Vector2(1080, 560);
        panel.anchorMin = panel.anchorMax = new Vector2(0.5f, 0.5f);
        var left = Column("SignIn", panel, new Vector2(36, -32), new Vector2(420, 496));
        Text("StageLabel", left, "C01  /  SERVER CONNECTION", 17, 26, new Color(0.35f, 0.8f, 0.85f));
        Text("Title", left, "Welcome back", 38, 54, Color.white);
        Text("Subtitle", left, "Sign in to load your character list.", 18, 36, new Color(0.65f, 0.73f, 0.83f));
        Text("LoginIdLabel", left, "LOGIN ID", 15, 20, Color.white);
        var id = Input("LoginIdInput", left, "Login ID", false);
        Text("PasswordLabel", left, "PASSWORD", 15, 20, Color.white);
        var password = Input("PasswordInput", left, "Password", true);
        var buttonRect = Box("LoginButton", left, new Color(0.13f, 0.6f, 0.68f));
        Height(buttonRect.gameObject, 52);
        buttonRect.GetComponent<Image>().raycastTarget = true;
        var button = buttonRect.gameObject.AddComponent<Button>();
        button.targetGraphic = buttonRect.GetComponent<Image>();
        var buttonText = Text("Label", buttonRect, "Sign in", 20, 52, Color.white);
        Stretch(buttonText.rectTransform, 0);
        buttonText.alignment = TextAlignmentOptions.Center;
        Text("EndpointNote", left, "Local test environment", 15, 22, new Color(0.55f, 0.63f, 0.73f));
        var right = Column("Results", panel, new Vector2(510, -32), new Vector2(534, 496));
        Text("ListTitle", right, "Your characters", 28, 48, Color.white);
        var status = Text("StatusText", right, "Ready to sign in.", 19, 110, new Color(0.5f, 0.85f, 0.89f));
        var list = Text("CharacterListText", right, "Sign in to request your characters.", 18, 300, new Color(0.78f, 0.83f, 0.9f));
        new GameObject("EventSystem", typeof(EventSystem), typeof(InputSystemUIInputModule));
        var screen = new GameObject("C01Client").AddComponent<C01LoginScreen>();
        canvasObject.transform.SetParent(screen.transform, false);
        var serialized = new SerializedObject(screen);
        serialized.FindProperty("loginIdInput").objectReferenceValue = id;
        serialized.FindProperty("passwordInput").objectReferenceValue = password;
        serialized.FindProperty("loginButton").objectReferenceValue = button;
        serialized.FindProperty("statusText").objectReferenceValue = status;
        serialized.FindProperty("characterListText").objectReferenceValue = list;
        serialized.ApplyModifiedPropertiesWithoutUndo();
        EditorSceneManager.SaveScene(scene, path);
        AssetDatabase.SaveAssets();
    }

    private static RectTransform Column(string name, Transform parent, Vector2 position, Vector2 size)
    {
        var rect = new GameObject(name, typeof(RectTransform), typeof(VerticalLayoutGroup)).GetComponent<RectTransform>();
        rect.SetParent(parent, false);
        rect.anchorMin = rect.anchorMax = rect.pivot = new Vector2(0, 1);
        rect.anchoredPosition = position;
        rect.sizeDelta = size;
        var layout = rect.GetComponent<VerticalLayoutGroup>();
        layout.spacing = 8;
        layout.childControlWidth = true;
        layout.childControlHeight = true;
        layout.childForceExpandWidth = true;
        layout.childForceExpandHeight = false;
        return rect;
    }
    private static RectTransform Box(string name, Transform parent, Color color)
    {
        var rect = new GameObject(name, typeof(RectTransform), typeof(Image)).GetComponent<RectTransform>();
        rect.SetParent(parent, false);
        rect.GetComponent<Image>().color = color;
        rect.GetComponent<Image>().raycastTarget = false;
        return rect;
    }
    private static TextMeshProUGUI Text(string name, Transform parent, string value, int fontSize, int height, Color color)
    {
        var text = new GameObject(name, typeof(RectTransform), typeof(TextMeshProUGUI)).GetComponent<TextMeshProUGUI>();
        text.transform.SetParent(parent, false);
        text.font = TMP_Settings.defaultFontAsset;
        text.text = value;
        text.fontSize = fontSize;
        text.enableAutoSizing = false;
        text.richText = false;
        text.color = color;
        text.raycastTarget = false;
        Height(text.gameObject, height);
        return text;
    }
    private static TMP_InputField Input(string name, Transform parent, string placeholder, bool password)
    {
        var rect = Box(name, parent, new Color(0.12f, 0.16f, 0.22f));
        rect.GetComponent<Image>().raycastTarget = true;
        Height(rect.gameObject, 50);
        var input = rect.gameObject.AddComponent<TMP_InputField>();
        var viewport = new GameObject("Viewport", typeof(RectTransform), typeof(RectMask2D)).GetComponent<RectTransform>();
        viewport.SetParent(rect, false);
        Stretch(viewport, 12);
        var text = Text("Text", viewport, "", 20, 50, Color.white);
        Stretch(text.rectTransform, 0);
        text.alignment = TextAlignmentOptions.MidlineLeft;
        var hint = Text("Placeholder", viewport, placeholder, 20, 50, new Color(0.5f, 0.57f, 0.67f));
        Stretch(hint.rectTransform, 0);
        hint.alignment = TextAlignmentOptions.MidlineLeft;
        input.textViewport = viewport;
        input.textComponent = text;
        input.placeholder = hint;
        input.targetGraphic = rect.GetComponent<Image>();
        input.lineType = TMP_InputField.LineType.SingleLine;
        input.contentType = password ? TMP_InputField.ContentType.Password : TMP_InputField.ContentType.Standard;
        input.asteriskChar = '*';
        input.characterLimit = password ? 128 : 64;
        return input;
    }
    private static void Height(GameObject gameObject, int height)
    {
        var element = gameObject.AddComponent<LayoutElement>();
        element.minHeight = element.preferredHeight = height;
    }
    private static void Stretch(RectTransform rect, int margin)
    {
        rect.anchorMin = Vector2.zero;
        rect.anchorMax = Vector2.one;
        rect.offsetMin = new Vector2(margin, margin);
        rect.offsetMax = new Vector2(-margin, -margin);
    }
}
