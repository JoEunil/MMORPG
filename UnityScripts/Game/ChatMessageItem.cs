using System;
using TMPro;
using UnityEngine;
using UnityEngine.EventSystems;

public class ChatMessageItem : MonoBehaviour, IPointerDownHandler
{
    public ulong ChatID { get; private set; }

    [SerializeField] private TMP_Text textComp;

    private Action<ulong> _onClick;
    public void Init(Action<ulong> onClick)
    {
        _onClick = onClick;
    }

    public void SetData(ulong myChatID, ulong senderChatID, ChatView.ChatScope scope, string senderName, string message)
    {
        ChatID = senderChatID;

        if (senderChatID == myChatID)
        {
            textComp.text = "나 : " + message;
            textComp.color = Color.gray;
        }
        else
        {

            textComp.text = senderName + ": " + message;
        }

    }
    public void OnPointerDown(PointerEventData eventData)
    {
        _onClick?.Invoke(ChatID);
    }

}
