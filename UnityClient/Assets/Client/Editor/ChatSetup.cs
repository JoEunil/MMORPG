using Mmorpg.Client;
using TMPro;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;
using UnityEngine.UI;

namespace Mmorpg.Client.Editor
{
    public static class ChatSetup
    {
        public static void Apply()
        {
            if (EditorApplication.isPlaying) throw new System.InvalidOperationException("Stop Play before applying C06 chat UI.");
            if (EditorSceneManager.GetActiveScene().path != "Assets/Client/Scenes/World.unity")
                throw new System.InvalidOperationException("Open the C02 scene first.");

            var canvas = Object.FindFirstObjectByType<Canvas>();
            var login = Object.FindFirstObjectByType<LoginScreen>();
            var world = Object.FindFirstObjectByType<WorldScreen>();
            var hud = canvas != null ? canvas.transform.Find("WorldHud") : null;
            if (canvas == null || canvas.renderMode != RenderMode.ScreenSpaceOverlay || login == null || world == null || hud == null)
                throw new System.InvalidOperationException("Expected C02 Canvas, WorldHud, C01LoginScreen, and C02WorldScreen were not found.");

            var panel = FindOrCreate("C06ChatPanel", canvas.transform, typeof(RectTransform), typeof(Image));
            var panelRect = panel.GetComponent<RectTransform>();
            panelRect.anchorMin = Vector2.zero;
            panelRect.anchorMax = Vector2.zero;
            panelRect.pivot = Vector2.zero;
            panelRect.anchoredPosition = new Vector2(24, 22);
            panelRect.sizeDelta = new Vector2(490, 260);
            var frame = panel.GetComponent<Image>();
            frame.color = new Color(.53f, .39f, .18f, .96f);
            frame.raycastTarget = false;

            var inner = FindOrCreate("ChatInset", panel.transform, typeof(RectTransform), typeof(Image));
            Stretch(inner.GetComponent<RectTransform>(), 3);
            var innerImage = inner.GetComponent<Image>();
            innerImage.color = new Color(.10f, .085f, .065f, .96f);
            innerImage.raycastTarget = false;

            var title = Text("ChatTitle", inner.transform, "REALM CHAT", 15, new Color(.92f, .78f, .48f));
            SetTop(title.rectTransform, 8, 22);

            var scopes = new[]
            {
                Button("ChatGlobalButton", inner.transform, "GLOBAL", new Color(.23f, .19f, .13f, 1f)),
                Button("ChatZoneButton", inner.transform, "ZONE", new Color(.23f, .19f, .13f, 1f)),
                Button("ChatWhisperButton", inner.transform, "WHISPER", new Color(.23f, .19f, .13f, 1f))
            };
            string[] scopeNames = { "GLOBAL", "ZONE", "WHISPER" };
            for (int i = 0; i < scopes.Length; i++)
            {
                var scopeRect = scopes[i].GetComponent<RectTransform>();
                SetTopLeft(scopeRect, 12 + i * 156, 36, 148, 28);
                var scopeLabel = scopes[i].GetComponentInChildren<TMP_Text>(true);
                scopeLabel.text = scopeNames[i];
                scopeLabel.fontSize = 13;
                scopeLabel.color = new Color(.94f, .85f, .67f);
            }

            var whisperTarget = Button("ChatWhisperTargetButton", inner.transform,
                "귓속말 대상 없음 · 받은 플레이어를 버튼으로 선택", new Color(.19f, .16f, .12f, 1f));
            SetTopLeft(whisperTarget.GetComponent<RectTransform>(), 12, 68, 460, 24);
            var whisperTargetLabel = whisperTarget.GetComponentInChildren<TMP_Text>(true);
            whisperTargetLabel.alignment = TextAlignmentOptions.MidlineLeft;
            whisperTargetLabel.fontSize = 12;
            whisperTargetLabel.color = new Color(.88f, .81f, .68f);

            var history = FindOrCreate("ChatHistoryViewport", inner.transform,
                typeof(RectTransform), typeof(Image), typeof(Mask), typeof(ScrollRect));
            var historyViewport = history.GetComponent<RectTransform>();
            // RectOffset order is left, right, top, bottom; keep below the recipient row.
            Stretch(historyViewport, new RectOffset(14, 14, 96, 76));
            var historyImage = history.GetComponent<Image>();
            historyImage.color = Color.white;
            var oldRectMask = history.GetComponent<RectMask2D>();
            if (oldRectMask != null) Undo.DestroyObjectImmediate(oldRectMask);
            var historyMask = history.GetComponent<Mask>() ?? Undo.AddComponent<Mask>(history);
            historyMask.showMaskGraphic = false;
            historyImage.raycastTarget = true;
            var previousLog = inner.transform.Find("ChatHistoryText");
            if (previousLog != null) Undo.SetTransformParent(previousLog, history.transform, "Move chat into scroll view");
            var log = Text("ChatHistoryText", history.transform, string.Empty, 13, new Color(.89f, .86f, .76f));
            log.alignment = TextAlignmentOptions.TopLeft;
            log.enableWordWrapping = true;
            log.overflowMode = TextOverflowModes.Overflow;
            log.rectTransform.anchorMin = new Vector2(0, 1);
            log.rectTransform.anchorMax = Vector2.one;
            log.rectTransform.pivot = new Vector2(.5f, 1);
            log.rectTransform.anchoredPosition = Vector2.zero;
            log.rectTransform.sizeDelta = Vector2.zero;
            var fitter = log.GetComponent<ContentSizeFitter>() ?? Undo.AddComponent<ContentSizeFitter>(log.gameObject);
            fitter.horizontalFit = ContentSizeFitter.FitMode.Unconstrained;
            fitter.verticalFit = ContentSizeFitter.FitMode.PreferredSize;
            var historyScroll = history.GetComponent<ScrollRect>();
            historyScroll.viewport = historyViewport;
            historyScroll.content = log.rectTransform;
            historyScroll.horizontal = false;
            historyScroll.vertical = true;
            historyScroll.movementType = ScrollRect.MovementType.Clamped;
            historyScroll.inertia = false;
            historyScroll.scrollSensitivity = 24;

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

            var component = canvas.GetComponent<ChatScreen>() ?? Undo.AddComponent<ChatScreen>(canvas.gameObject);
            var serialized = new SerializedObject(component);
            serialized.FindProperty("loginScreen").objectReferenceValue = login;
            serialized.FindProperty("worldScreen").objectReferenceValue = world;
            serialized.FindProperty("chatPanel").objectReferenceValue = panel;
            serialized.FindProperty("chatLog").objectReferenceValue = log;
            serialized.FindProperty("chatScroll").objectReferenceValue = historyScroll;
            serialized.FindProperty("sendStatus").objectReferenceValue = status;
            serialized.FindProperty("messageInput").objectReferenceValue = input;
            serialized.FindProperty("sendButton").objectReferenceValue = send;
            var scopeProperty = serialized.FindProperty("scopeButtons");
            scopeProperty.arraySize = scopes.Length;
            for (int i = 0; i < scopes.Length; i++) scopeProperty.GetArrayElementAtIndex(i).objectReferenceValue = scopes[i];
            serialized.FindProperty("whisperTargetButton").objectReferenceValue = whisperTarget;
            serialized.ApplyModifiedPropertiesWithoutUndo();
            ApplyCleanStyle();
            panel.SetActive(false);
            EditorSceneManager.MarkSceneDirty(SceneManager.GetActiveScene());
            EditorSceneManager.SaveScene(SceneManager.GetActiveScene());
        }

        public static void ApplyCleanStyle()
        {
            var canvas=Object.FindFirstObjectByType<Canvas>();
            var panel=canvas!=null?canvas.transform.Find("C06ChatPanel") as RectTransform:null;
            if(panel==null)return;
            panel.sizeDelta=new Vector2(360,170);panel.anchoredPosition=new Vector2(24,24);
            foreach(var image in panel.GetComponentsInChildren<Image>(true))
            {
                if(image.name=="ForgedFrame"||image.name=="FrameOrnaments"){image.gameObject.SetActive(false);continue;}
                image.sprite=null;image.type=Image.Type.Simple;
                if(image.name=="ChatHistoryViewport")continue;
                image.color=image.GetComponent<Button>()!=null?new Color(.10f,.13f,.17f,.72f):new Color(.035f,.05f,.065f,.64f);
            }
            foreach(var button in panel.GetComponentsInChildren<Button>(true))button.colors=ColorBlock.defaultColorBlock;
            var inner=panel.Find("ChatInset");
            panel.GetComponent<Image>().color=new Color(.055f,.07f,.075f,.22f);
            inner.GetComponent<Image>().color=new Color(.035f,.05f,.065f,.38f);
            var title=inner.Find("ChatTitle");if(title!=null)title.gameObject.SetActive(false);
            string[] names={"ChatGlobalButton","ChatZoneButton","ChatWhisperButton"};
            for(int i=0;i<names.Length;i++)SetTopLeft(inner.Find(names[i]) as RectTransform,10+i*112,8,108,24);
            SetTopLeft(inner.Find("ChatWhisperTargetButton") as RectTransform,10,36,334,20);
            Stretch(inner.Find("ChatHistoryViewport") as RectTransform,new RectOffset(12,12,60,62));
            var input=inner.Find("ChatInput") as RectTransform;input.sizeDelta=new Vector2(250,30);input.anchoredPosition=new Vector2(10,8);
            var send=inner.Find("ChatSendButton") as RectTransform;send.sizeDelta=new Vector2(78,30);send.anchoredPosition=new Vector2(-10,8);
            foreach(var text in panel.GetComponentsInChildren<TMP_Text>(true))
            {text.fontSize=Mathf.Max(14,text.fontSize);text.color=new Color(.86f,.90f,.95f);}
            inner.Find("ChatHistoryViewport/ChatHistoryText").GetComponent<TMP_Text>().fontSize=14;
            var target=inner.Find("ChatWhisperTargetButton").GetComponentInChildren<TMP_Text>(true);
            target.enableWordWrapping=false;target.overflowMode=TextOverflowModes.Ellipsis;
            var status=inner.Find("ChatStatusText").GetComponent<TMP_Text>();
            status.fontSize=14;status.enableWordWrapping=false;status.overflowMode=TextOverflowModes.Ellipsis;
            status.rectTransform.anchoredPosition=new Vector2(0,42);
            status.rectTransform.sizeDelta=new Vector2(-24,18);
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

        private static void SetTopLeft(RectTransform rect, float left, float top, float width, float height)
        {
            rect.anchorMin = rect.anchorMax = new Vector2(0, 1);
            rect.pivot = new Vector2(0, 1);
            rect.anchoredPosition = new Vector2(left, -top);
            rect.sizeDelta = new Vector2(width, height);
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
}
