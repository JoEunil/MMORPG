using ClientCore;
using TMPro;
using UnityEngine;
using UnityEngine.UI;
public class CharacterSlotUI : MonoBehaviour
{
    public Image background;
    public TMP_Text nameText;
    private CharacterData _data;
    public void SetData(CharacterInfoView data)
    {
        _data = new CharacterData
        {
            level = data.Level,
            characterID = data.CharacterID,
            characterName = data.Name
        };

        background.color = new Color(0.8f, 0.8f, 0.8f, 1f);
        nameText.text = $"{data.Name}   Lv.{data.Level}";
    }
    public void OnClick()
    {
        CharacterListView.Instance.SelectCharacter(_data);
    }

}
