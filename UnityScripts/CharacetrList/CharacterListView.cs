using ClientCore;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;

public class CharacterListView : MonoBehaviour
{
    public static CharacterListView Instance;

    public Transform content; // ScrollView Content
    public GameObject slotPrefab;

    public Button enterButton;

    private CharacterData selectedCharacter;
    private IViewDataUI _viewData;
    private IViewModelUI _viewModel;
    private void Awake()
    {
        Instance = this;
        _viewData = CoreManager.Instance.VD;
        _viewModel = CoreManager.Instance.VM;
    }

    void Start()
    {
        enterButton.onClick.AddListener(OnEnterBtnClicked);
        enterButton.interactable = false;

        _viewModel.OnEnterSuccess += OnEnterSuccess;

        _viewData = CoreManager.Instance.VD;


        var (count, charList) = _viewData.GetCharList();

        if (charList == null || count <= 0)
            return;

        int createCount = Mathf.Min(count, charList.Count);

        for (int i = 0; i < createCount; i++)
        {
            var character = charList[i];
            GameObject go = Instantiate(slotPrefab, content);
            var slot = go.GetComponent<CharacterSlotUI>();
            slot.SetData(character);
        }
    }
    private void OnDestroy()
    {
        _viewModel.OnEnterSuccess -= OnEnterSuccess;
    }
    public void SelectCharacter(CharacterData data)
    {
        selectedCharacter = data;
        enterButton.interactable = true;
    }

    public void OnEnterBtnClicked()
    {
        _viewModel.Enter(selectedCharacter.characterID);
    }
    private void OnEnterSuccess(bool sucesss)
    {
        UnityEngine.SceneManagement.SceneManager.LoadScene("GameScene");
    }
}

