using C01;
using C02;
using TMPro;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;
using UnityEngine.UI;

public static class C06ChatSetup
{
    public static void Apply()
    {
        if (EditorApplication.isPlaying) throw new System.InvalidOperationException("Stop Play before applying C06 chat UI.");
        if (EditorSceneManager.GetActiveScene().path != "Assets/C02/C02.unity")
            throw new System.InvalidOperationException("Open the C02 scene first.");

        var canvas = Object.FindFirstObjectByType<Canvas>();
        var login = Object.FindFirstObjectByType<C01LoginScreen>();
        var world = Object.FindFirstObjectByType<C02WorldScreen>();
        var hud = canvas != null ? canvas.transform.Find("WorldHud") : null;
        if (canvas == null || canvas.renderMode != RenderMode.ScreenSpaceOverlay || login == null || world == null || hud == null)
            throw new System.InvalidOperationException("Expected C02 Canvas, WorldHud, C01LoginScreen, and C02WorldScreen were not found.");

        var panel = FindOrCreate("C06ChatPanel", canvas.transform, typeof(RectTransform), typeof(Image));
        var panelRect = panel.GetComponent<RectTransform>();
        panelRect.anchorMin = Vector2.zero;
        panelRect.anchorMax = Vector2.zero;
        panelRect.pivot = Vector2.zero;
        panelRect.anchoredPosition = new Vector2(24, 22);
        panelRect.sizeDelta = new Vector2(490, 225);
        var frame = panel.GetComponent<Image>();
        frame.color = new Color(.53f, .39f, .18f, .96f);
        frame.raycastTarget = false;

        var inner = FindOrCreate("ChatInset", panel.transform, typeof(RectTransform), typeof(Image));
        Stretch(inner.GetComponent<RectTransform>(), 3);
        var innerImage = inner.GetComponent<Image>();
        innerImage.color = new Color(.10f, .085f, .065f, .96f);
        innerImage.raycastTarget = false;

        var title = Text("ChatTitle", inner.transform, "REALM CHAT  ·  GLOBAL", 15, new Color(.92f, .78f, .48f));
        SetTop(title.rectTransform, 15, 27);
        var log = Text("ChatHistoryText", inner.transform, string.Empty, 15, new Color(.89f, .86f, .76f));
        log.alignment = TextAlignmentOptions.BottomLeft;
        log.enableWordWrapping = true;
        log.overflowMode = TextOverflowModes.Truncate;
        Stretch(log.rectTransform, new RectOffset(14, 14, 44, 82));

        var inputObject = FindOrCreate("ChatInput", inner.transform, typeof(RectTransform), typeof(Image), typeof(TMP_InputField));
        var inputRect = inputObject.GetComponent<RectTransform>();
        inputRect.anchorMin = Vector2.zero;
        inputRect.anchorMax = Vector2.zero;
        inputRect.pivot = Vector2.zero;
        inputRect.anchoredPosition = new Vector2(12, 10);
        inputRect.sizeDelta = new Vector2(350, 34);
        inputObject.GetComponent<Image>().color = new Color(.28f, .23f, .16f, 1f);
        var input = inputObject.GetComponent<TMP_InputField>();
        input.targetGraphic = inputObject.GetComponent<Image>();
        var viewportObject = FindOrCreate("Viewport", inputObject.transform, typeof(RectTransform), typeof(RectMask2D));
        var viewport = viewportObject.GetComponent<RectTransform>();
        Stretch(viewport, 8);
        input.textViewport = viewport;
        var inputText = EnsureText("Text", viewport, input.textComponent);
        var placeholder = EnsureText("Placeholder", viewport, input.placeholder as TMP_Text);
        input.textComponent = inputText;
        input.placeholder = placeholder;
        inputText.fontSize = placeholder.fontSize = 15;
        inputText.alignment = placeholder.alignment = TextAlignmentOptions.MidlineLeft;
        inputText.color = new Color(.95f, .92f, .84f);
        placeholder.text = "Write a message…";
        placeholder.color = new Color(.63f, .57f, .43f);
        input.contentType = TMP_InputField.ContentType.Standard;
        input.characterLimit = 0;
        input.lineType = TMP_InputField.LineType.SingleLine;
        input.richText = false;
        input.onValidateInput = RejectLineBreakAndNul;

        var send = Button("ChatSendButton", inner.transform, "SEND", new Color(.37f, .26f, .12f, 1f));
        var sendRect = send.GetComponent<RectTransform>();
        sendRect.anchorMin = sendRect.anchorMax = sendRect.pivot = new Vector2(1, 0);
        sendRect.anchoredPosition = new Vector2(-12, 10);
        sendRect.sizeDelta = new Vector2(92, 34);
        var sendText = send.GetComponentInChildren<TMP_Text>(true);
        sendText.fontSize = 13;
        sendText.color = new Color(.94f, .84f, .63f);
        sendText.richText = false;

        var status = Text("ChatStatusText", inner.transform, string.Empty, 11, new Color(.85f, .73f, .52f));
        status.alignment = TextAlignmentOptions.MidlineLeft;
        status.enableWordWrapping = false;
        SetStatusRect(status.rectTransform);

        var component = canvas.GetComponent<C06ChatScreen>() ?? Undo.AddComponent<C06ChatScreen>(canvas.gameObject);
        var serialized = new SerializedObject(component);
        serialized.FindProperty("loginScreen").objectReferenceValue = login;
        serialized.FindProperty("worldScreen").objectReferenceValue = world;
        serialized.FindProperty("chatPanel").objectReferenceValue = panel;
        serialized.FindProperty("chatLog").objectReferenceValue = log;
        serialized.FindProperty("sendStatus").objectReferenceValue = status;
        serialized.FindProperty("messageInput").objectReferenceValue = input;
        serialized.FindProperty("sendButton").objectReferenceValue = send;
        serialized.ApplyModifiedPropertiesWithoutUndo();
        panel.SetActive(false);
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
        var text = Text("Label", button.transform, label, 14, Color.white);
        text.alignment = TextAlignmentOptions.Center;
        Stretch(text.rectTransform, 2);
        return button;
    }

    private static TMP_Text EnsureText(string name, Transform parent, TMP_Text current)
    {
        if (current != null && current.transform.parent == parent) return current;
        var text = Text(name, parent, string.Empty, 15, Color.white);
        Stretch(text.rectTransform, 0);
        return text;
    }

    private static char RejectLineBreakAndNul(string text, int index, char added)
    {
        return added == '\0' || added == '\r' || added == '\n' ? '\0' : added;
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

    private static void SetStatusRect(RectTransform rect)
    {
        rect.anchorMin = Vector2.zero;
        rect.anchorMax = new Vector2(1, 0);
        rect.pivot = new Vector2(.5f, 0);
        rect.anchoredPosition = new Vector2(0, 47);
        rect.sizeDelta = new Vector2(-26, 20);
    }
}
