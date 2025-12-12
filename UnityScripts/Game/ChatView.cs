using ClientCore;
using UnityEngine;
using UnityEngine.UI;
using TMPro;
public class ChatView : MonoBehaviour
{
    private IViewModelUI _viewModel;
    [SerializeField] private PlayerManager playerManager;
    [SerializeField] private TMP_InputField ChatInputField;
    [SerializeField] private ScrollRect ChatScrollView;
    [SerializeField] private RectTransform ChatContent;
    [SerializeField] private GameObject ChatMessagePrefab;

    private void Awake()
    {
        _viewModel = CoreManager.Instance.VM;
        _viewModel.OnChatReceived += UpdateChat;
        ChatInputField.onSubmit.AddListener(OnChatSubmit);
    }

    private void OnChatSubmit(string text)
    {
        if (!string.IsNullOrWhiteSpace(text))
        {
            OnChatButtonClick();
        }
    }
    public void OnChatButtonClick()
    {
        _viewModel.Chat(ChatInputField.text);
        ChatInputField.text = "";
    }
    private void UpdateChat(ulong zoneInternalID, string message)
    {
        var newMessageObj = Instantiate(ChatMessagePrefab, ChatContent);
        var textComp = newMessageObj.GetComponent<TMP_Text>();
        if (zoneInternalID == playerManager.GetMyPlayerID())
            textComp.text = message;
        else
        {
            string name = playerManager.GetPlayerName(zoneInternalID).TrimEnd('\0');
            if (name != null) 
                textComp.text =  name + ": "+ message;
        }

        Canvas.ForceUpdateCanvases();
        ChatScrollView.verticalNormalizedPosition = 0f;
    }
}
