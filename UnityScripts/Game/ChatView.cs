using ClientCore;
using ClientCore.PacketHelper;
using System;
using System.Collections.Generic;
using TMPro;
using UnityEngine;
using UnityEngine.UI;
public class ChatView : MonoBehaviour
{
    private IViewModelUI _viewModel;
    [SerializeField] private PlayerManager playerManager;
    [SerializeField] private TMP_InputField ChatInputField;
    [SerializeField] private ScrollRect ChatScrollView;
    [SerializeField] private GameObject ChatMessagePrefab;

    [SerializeField] private RectTransform ChatContentGlobal;
    [SerializeField] private RectTransform ChatContentZone;
    [SerializeField] private RectTransform ChatContentWhisper;

    [SerializeField] private Button GlobalChatBtn;
    [SerializeField] private Button ZoneChatBtn;
    [SerializeField] private Button WhisperChatBtn;

    [SerializeField] private TextMeshProUGUI ChatScopeText;

    private ChatScope _currentScope = ChatScope.Global;
    private ulong _targetChatID = 0;
    public enum ChatScope
    {
        Global,
        Zone,
        Whisper
    };
    private void Awake()
    {
        _viewModel = CoreManager.Instance.VM;
        _viewModel.OnChatReceived += UpdateChat;
        ChatInputField.onSubmit.AddListener(OnChatSubmit);
        GlobalChatBtn.GetComponentInChildren<TextMeshProUGUI>().text = "Global";
        ZoneChatBtn.GetComponentInChildren<TextMeshProUGUI>().text = "Zone";
        WhisperChatBtn.GetComponentInChildren<TextMeshProUGUI>().text = "Whisper";
        GlobalChatBtn.onClick.AddListener(() => OnChatScopeClick(ChatScope.Global));
        ZoneChatBtn.onClick.AddListener(() => OnChatScopeClick(ChatScope.Zone));
        WhisperChatBtn.onClick.AddListener(() => OnChatScopeClick(ChatScope.Whisper));
        _currentScope = ChatScope.Global;
        RefreshChat();
    }

    private void OnChatSubmit(string text)
    {
        if (!string.IsNullOrWhiteSpace(text))
        {
            OnChatButtonClick();
        }
    }

    public void OnChatScopeClick(ChatScope scope)
    {
        _currentScope = scope;
        RefreshChat();
    }
    public void OnChatMessageClick(ulong target)
    {
        Debug.Log($"Chat message clicked: {target}");
        if (_targetChatID == playerManager.GetChatID())
            return;
        _targetChatID = target;
        _currentScope = ChatScope.Whisper;
        RefreshChat();
    }

    private void RefreshChat()
    {

        switch (_currentScope) 
        {
            case ChatScope.Global:
                {
                    ChatScopeText.text = "Current scope: Global";
                    ChatContentGlobal.gameObject.SetActive(true);
                    ChatContentZone.gameObject.SetActive(false);
                    ChatContentWhisper.gameObject.SetActive(false);
                    break;
                }
            case ChatScope.Zone:
                {
                    ChatScopeText.text = "Current scope: Zone";
                    ChatContentGlobal.gameObject.SetActive(false);
                    ChatContentZone.gameObject.SetActive(true);
                    ChatContentWhisper.gameObject.SetActive(false);
                    break;
                }
            case ChatScope.Whisper:
                {
                    ChatScopeText.text = "Current scope: Whisper";
                    ChatContentGlobal.gameObject.SetActive(false);
                    ChatContentZone.gameObject.SetActive(false);
                    ChatContentWhisper.gameObject.SetActive(true);
                    break;
                }
        }
        Canvas.ForceUpdateCanvases();
        if (ChatScrollView != null)
            ChatScrollView.verticalNormalizedPosition = 0f;
    }
    public void OnChatButtonClick()
    {
        ulong target = 0;
        if (_currentScope == ChatScope.Whisper)
        {
            target = _targetChatID;
        }
        _viewModel.Chat(ChatInputField.text, (byte)_currentScope, target);
        ChatInputField.text = "";
    }
    private void UpdateChat(byte scope, ulong senderChatID, string senderName, string message)
    {
        GameObject newMessageObj = null;

        switch ((ChatScope)scope)
        {
            case ChatScope.Global:
                newMessageObj = Instantiate(ChatMessagePrefab, ChatContentGlobal);
                break;
            case ChatScope.Zone:
                newMessageObj = Instantiate(ChatMessagePrefab, ChatContentZone);
                break;
            case ChatScope.Whisper:
                newMessageObj = Instantiate(ChatMessagePrefab, ChatContentWhisper);
                break;
        }

        if (newMessageObj == null)
            return;

        var item = newMessageObj.GetComponent<ChatMessageItem>();
        if (item != null)
        {
            item.Init(OnChatMessageClick);
            item.SetData(
                playerManager.GetChatID(),
                senderChatID,
                (ChatScope)scope,
                senderName,
                message);
        }

        Canvas.ForceUpdateCanvases();
        if (ChatScrollView != null)
            ChatScrollView.verticalNormalizedPosition = 0f;
    }
}
