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
        [SerializeField] private TMP_Text sendStatus;
        [SerializeField] private TMP_InputField messageInput;
        [SerializeField] private Button sendButton;
        private readonly Queue<string> lines = new Queue<string>(MaxVisibleLines);
        private IViewModelUI model;
        private TMP_FontAsset localKoreanFont;

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
            }
            chatLog.richText = false;
            messageInput.lineType = TMP_InputField.LineType.SingleLine;
            sendButton.onClick.AddListener(SendCurrentMessage);
            messageInput.onSubmit.AddListener(SubmitMessage);
            loginScreen.ClientChanged += ChangeClient;
            worldScreen.WorldStateChanged += WorldChanged;
            ChangeClient(loginScreen.Model);
        }

        private void ChangeClient(IViewModelUI next)
        {
            if (model != null) model.OnChatReceived -= ChatReceived;
            model = next;
            lines.Clear();
            chatLog.text = string.Empty;
            messageInput.SetTextWithoutNotify(string.Empty);
            sendStatus.text = string.Empty;
            if (model != null) model.OnChatReceived += ChatReceived;
            WorldChanged();
        }

        private void WorldChanged()
        {
            bool available = WorldAvailable && model != null && loginScreen.Model == model;
            chatPanel.SetActive(available);
            sendButton.interactable = available;
            messageInput.interactable = available;
            if (!available) sendStatus.text = string.Empty;
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
                model.Chat(message, 0, 0);
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
            if (!WorldAvailable || model == null || loginScreen.Model != model || scope != 0 || senderChatID == 0 || message == null)
                return;

            string nameText = (senderName ?? string.Empty).TrimEnd('\0');
            string name = string.IsNullOrWhiteSpace(nameText)
                ? "플레이어 #" + senderChatID.ToString(System.Globalization.CultureInfo.InvariantCulture)
                : nameText;
            lines.Enqueue(name + ": " + message);
            while (lines.Count > MaxVisibleLines) lines.Dequeue();
            chatLog.text = string.Join("\n", lines);
            sendStatus.text = string.Empty;
        }

        private void OnDisable()
        {
            if (loginScreen != null) loginScreen.ClientChanged -= ChangeClient;
            if (worldScreen != null) worldScreen.WorldStateChanged -= WorldChanged;
            if (sendButton != null) sendButton.onClick.RemoveListener(SendCurrentMessage);
            if (messageInput != null) messageInput.onSubmit.RemoveListener(SubmitMessage);
            if (model != null) model.OnChatReceived -= ChatReceived;
            model = null;
            lines.Clear();
            if (chatLog != null) chatLog.text = string.Empty;
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
