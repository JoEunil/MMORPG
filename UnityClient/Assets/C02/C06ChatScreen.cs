using System;
using System.Collections.Generic;
using System.Text;
using C01;
using ClientCore;
using TMPro;
using UnityEngine;
using UnityEngine.UI;

namespace C02
{
    public sealed class C06ChatScreen : MonoBehaviour
    {
        private const int MaxMessageBytes = 128;
        private const int MaxVisibleLines = 12;
        [SerializeField] private C01LoginScreen loginScreen;
        [SerializeField] private C02WorldScreen worldScreen;
        [SerializeField] private GameObject chatPanel;
        [SerializeField] private TMP_Text chatLog;
        [SerializeField] private ScrollRect chatScroll;
        [SerializeField] private TMP_Text sendStatus;
        [SerializeField] private TMP_InputField messageInput;
        [SerializeField] private Button sendButton;
        [SerializeField] private Button[] scopeButtons;
        [SerializeField] private Button whisperTargetButton;
        private readonly Queue<string> lines = new Queue<string>(MaxVisibleLines);
        private readonly List<KeyValuePair<ulong, string>> whisperRecipients = new List<KeyValuePair<ulong, string>>(8);
        private IViewModelUI model;
        private Action<byte, ulong, string, string> chatHandler;
        private Action<ushort, ulong, uint, float, float> zoneHandler;
        private TMP_FontAsset localKoreanFont;
        private byte selectedScope;
        private ulong ownChatID;
        private ulong selectedWhisperID;

        private bool WorldAvailable => worldScreen != null && worldScreen.isActiveAndEnabled &&
            worldScreen.Stage == C02Stage.InWorld && worldScreen.HasOwnFull && worldScreen.IsWorldVisible;

        private void OnEnable()
        {
            if (loginScreen == null || worldScreen == null || chatPanel == null || chatLog == null ||
                sendStatus == null || messageInput == null || messageInput.textComponent == null || sendButton == null)
            {
                Debug.LogError("C06 chat references are missing.", this);
                enabled = false;
                return;
            }

            if (localKoreanFont == null)
            {
                try { localKoreanFont = TMP_FontAsset.CreateFontAsset("Malgun Gothic", "Regular"); }
                catch (Exception) { localKoreanFont = null; }
            }
            if (localKoreanFont != null)
            {
                chatLog.font = localKoreanFont;
                sendStatus.font = localKoreanFont;
                messageInput.textComponent.font = localKoreanFont;
                if (messageInput.placeholder is TMP_Text placeholder) placeholder.font = localKoreanFont;
                if (sendButton.GetComponentInChildren<TMP_Text>(true) is TMP_Text label) label.font = localKoreanFont;
                if (scopeButtons != null)
                    foreach (var scopeButton in scopeButtons)
                        if (scopeButton != null && scopeButton.GetComponentInChildren<TMP_Text>(true) is TMP_Text scopeLabel)
                            scopeLabel.font = localKoreanFont;
                if (whisperTargetButton != null && whisperTargetButton.GetComponentInChildren<TMP_Text>(true) is TMP_Text targetLabel)
                    targetLabel.font = localKoreanFont;
            }
            chatLog.richText = false;
            messageInput.lineType = TMP_InputField.LineType.SingleLine;
            sendButton.onClick.AddListener(SendCurrentMessage);
            messageInput.onSubmit.AddListener(SubmitMessage);
            if (scopeButtons != null && scopeButtons.Length == 3)
            {
                scopeButtons[0]?.onClick.AddListener(SelectGlobal);
                scopeButtons[1]?.onClick.AddListener(SelectZone);
                scopeButtons[2]?.onClick.AddListener(SelectWhisper);
            }
            if (whisperTargetButton != null) whisperTargetButton.onClick.AddListener(CycleWhisperRecipient);
            loginScreen.ClientChanged += ChangeClient;
            worldScreen.WorldStateChanged += WorldChanged;
            ChangeClient(loginScreen.Model);
        }

        private void ChangeClient(IViewModelUI next)
        {
            if (model != null)
            {
                if (chatHandler != null) model.OnChatReceived -= chatHandler;
                if (zoneHandler != null) model.OnZoneChageReceived -= zoneHandler;
            }
            chatHandler = null;
            zoneHandler = null;
            model = next;
            lines.Clear();
            whisperRecipients.Clear();
            ownChatID = 0;
            selectedWhisperID = 0;
            selectedScope = 0;
            chatLog.text = string.Empty;
            messageInput.SetTextWithoutNotify(string.Empty);
            sendStatus.text = string.Empty;
            if (model != null)
            {
                var source = model;
                chatHandler = (scope, sender, name, message) =>
                {
                    if (source != model || loginScreen.Model != source || !isActiveAndEnabled) return;
                    ChatReceived(scope, sender, name, message);
                };
                zoneHandler = (zone, chat, internalID, x, y) =>
                {
                    if (source != model || loginScreen.Model != source || !isActiveAndEnabled || chat == 0) return;
                    ownChatID = chat;
                    whisperRecipients.RemoveAll(recipient => recipient.Key == ownChatID);
                    if (selectedWhisperID == ownChatID || whisperRecipients.FindIndex(recipient => recipient.Key == selectedWhisperID) < 0)
                        selectedWhisperID = whisperRecipients.Count == 0 ? 0 : whisperRecipients[0].Key;
                    RefreshChatControls();
                };
                model.OnChatReceived += chatHandler;
                model.OnZoneChageReceived += zoneHandler;
            }
            WorldChanged();
        }

        private void WorldChanged()
        {
            bool available = WorldAvailable && model != null && loginScreen.Model == model;
            chatPanel.SetActive(available);
            messageInput.interactable = available;
            if (!available) sendStatus.text = string.Empty;
            RefreshChatControls();
        }

        private void SelectGlobal() => SelectScope(0);
        private void SelectZone() => SelectScope(1);
        private void SelectWhisper() => SelectScope(2);

        private void SelectScope(byte scope)
        {
            if (scope > 2) return;
            selectedScope = scope;
            sendStatus.text = scope == 2 && selectedWhisperID == 0
                ? "채팅을 받으면 실제 발신자를 답장 대상으로 선택할 수 있습니다."
                : string.Empty;
            RefreshChatControls();
        }

        private void CycleWhisperRecipient()
        {
            if (whisperRecipients.Count == 0)
            {
                selectedWhisperID = 0;
                if (selectedScope == 2) sendStatus.text = "아직 채팅을 받은 대상이 없습니다.";
                RefreshChatControls();
                return;
            }

            int current = whisperRecipients.FindIndex(recipient => recipient.Key == selectedWhisperID);
            selectedWhisperID = whisperRecipients[(current + 1) % whisperRecipients.Count].Key;
            if (selectedScope == 2) sendStatus.text = string.Empty;
            RefreshChatControls();
        }

        private void RefreshChatControls()
        {
            bool available = WorldAvailable && model != null && loginScreen.Model == model;
            if (scopeButtons != null && scopeButtons.Length == 3)
            {
                for (int i = 0; i < scopeButtons.Length; i++)
                {
                    var button = scopeButtons[i];
                    if (button == null) continue;
                    button.interactable = available;
                    var image = button.GetComponent<Image>();
                    if (image != null) image.color = selectedScope == i
                        ? new Color(.58f, .39f, .15f, 1f)
                        : new Color(.23f, .19f, .13f, 1f);
                }
            }

            string targetName = null;
            for (int i = 0; i < whisperRecipients.Count; i++)
                if (whisperRecipients[i].Key == selectedWhisperID) { targetName = whisperRecipients[i].Value; break; }
            if (whisperTargetButton != null)
            {
                whisperTargetButton.interactable = available && whisperRecipients.Count > 0;
                if (whisperTargetButton.GetComponentInChildren<TMP_Text>() is TMP_Text targetText)
                    targetText.text = targetName == null
                        ? "귓속말 대상 없음 · 버튼으로 실제 수신자를 선택"
                        : "귓속말 대상: " + targetName + "  (다음 대상)";
            }
            sendButton.interactable = available && (selectedScope != 2 || targetName != null);
        }

        private void SubmitMessage(string _) => SendCurrentMessage();

        private void SendCurrentMessage()
        {
            if (!WorldAvailable || model == null || loginScreen.Model != model || loginScreen.Data == null) return;
            string raw = messageInput.text ?? string.Empty;
            if (raw.IndexOf('\0') >= 0 || raw.IndexOf('\r') >= 0 || raw.IndexOf('\n') >= 0)
            {
                sendStatus.text = "줄바꿈 또는 NUL은 보낼 수 없습니다.";
                return;
            }

            string message = raw.Trim();
            int byteCount = Encoding.UTF8.GetByteCount(message);
            if (message.Length == 0)
            {
                sendStatus.text = "메시지를 입력하세요.";
                return;
            }
            if (byteCount > MaxMessageBytes)
            {
                sendStatus.text = "메시지는 UTF-8 기준 128바이트까지 보낼 수 있습니다.";
                return;
            }

            try
            {
                if (selectedScope == 2 && (selectedWhisperID == 0 || whisperRecipients.FindIndex(recipient => recipient.Key == selectedWhisperID) < 0))
                {
                    selectedWhisperID = 0;
                    sendStatus.text = "채팅을 받은 실제 대상을 먼저 선택하세요.";
                    RefreshChatControls();
                    return;
                }
                model.Chat(message, selectedScope, selectedScope == 2 ? selectedWhisperID : 0);
                messageInput.SetTextWithoutNotify(string.Empty);
                sendStatus.text = "전송 요청됨 · 서버 echo 대기";
            }
            catch (Exception)
            {
                sendStatus.text = "전송 요청에 실패했습니다. 입력을 확인해 주세요.";
            }
        }

        private void ChatReceived(byte scope, ulong senderChatID, string senderName, string message)
        {
            if (!WorldAvailable || model == null || loginScreen.Model != model || scope > 2 || senderChatID == 0 || message == null)
                return;

            string nameText = (senderName ?? string.Empty).Replace('\0', ' ').Replace('\r', ' ').Replace('\n', ' ').Trim();
            string name = string.IsNullOrWhiteSpace(nameText)
                ? "플레이어 #" + senderChatID.ToString(System.Globalization.CultureInfo.InvariantCulture)
                : nameText;
            if (senderChatID != ownChatID)
            {
                int existing = whisperRecipients.FindIndex(recipient => recipient.Key == senderChatID);
                if (existing >= 0) whisperRecipients.RemoveAt(existing);
                whisperRecipients.Insert(0, new KeyValuePair<ulong, string>(senderChatID, name));
                if (whisperRecipients.Count > 8)
                {
                    int oldest = whisperRecipients.Count - 1;
                    whisperRecipients.RemoveAt(whisperRecipients[oldest].Key == selectedWhisperID ? oldest - 1 : oldest);
                }
                // Incoming traffic must not silently redirect a reply the user is composing.
                if (selectedWhisperID == 0 || whisperRecipients.FindIndex(recipient => recipient.Key == selectedWhisperID) < 0)
                    selectedWhisperID = senderChatID;
            }
            string scopeLabel = scope == 0 ? "Global" : scope == 1 ? "Zone" : "Whisper";
            string safeMessage = message.Replace('\0', ' ').Replace('\r', ' ').Replace('\n', ' ');
            lines.Enqueue("[" + scopeLabel + "] " + name + ": " + safeMessage);
            while (lines.Count > MaxVisibleLines) lines.Dequeue();
            chatLog.text = string.Join("\n", lines);
            if (chatScroll != null)
            {
                LayoutRebuilder.ForceRebuildLayoutImmediate(chatLog.rectTransform);
                Canvas.ForceUpdateCanvases();
                chatScroll.verticalNormalizedPosition = 0;
            }
            sendStatus.text = string.Empty;
            RefreshChatControls();
        }

        private void OnDisable()
        {
            if (loginScreen != null) loginScreen.ClientChanged -= ChangeClient;
            if (worldScreen != null) worldScreen.WorldStateChanged -= WorldChanged;
            if (sendButton != null) sendButton.onClick.RemoveListener(SendCurrentMessage);
            if (messageInput != null) messageInput.onSubmit.RemoveListener(SubmitMessage);
            if (scopeButtons != null && scopeButtons.Length == 3)
            {
                scopeButtons[0]?.onClick.RemoveListener(SelectGlobal);
                scopeButtons[1]?.onClick.RemoveListener(SelectZone);
                scopeButtons[2]?.onClick.RemoveListener(SelectWhisper);
            }
            if (whisperTargetButton != null) whisperTargetButton.onClick.RemoveListener(CycleWhisperRecipient);
            if (model != null)
            {
                if (chatHandler != null) model.OnChatReceived -= chatHandler;
                if (zoneHandler != null) model.OnZoneChageReceived -= zoneHandler;
            }
            chatHandler = null;
            zoneHandler = null;
            model = null;
            lines.Clear();
            whisperRecipients.Clear();
            ownChatID = 0;
            selectedWhisperID = 0;
            selectedScope = 0;
            if (chatLog != null) chatLog.text = string.Empty;
            if (sendStatus != null) sendStatus.text = string.Empty;
            if (messageInput != null) messageInput.SetTextWithoutNotify(string.Empty);
            if (chatPanel != null) chatPanel.SetActive(false);
        }

        private void OnDestroy()
        {
            if (localKoreanFont != null) Destroy(localKoreanFont);
            localKoreanFont = null;
        }
    }
}
