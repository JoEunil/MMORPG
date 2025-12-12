using UnityEngine;
using TMPro;
using UnityEngine.UI;
using System.Collections;
using ClientCore; 

public class LoginView : MonoBehaviour
{
    [SerializeField] private TMP_InputField txtID;
    [SerializeField] private TMP_InputField txtPW;
    [SerializeField] private TMP_Text btnLoginText;

    private IViewModelUI _viewModel;

    private const string IdPlaceholder = "ID";
    private const string PwPlaceholder = "PW";

    void Awake()
    {    
        TMP_Text ID = txtID.placeholder as TMP_Text;
        TMP_Text PW = txtPW.placeholder as TMP_Text;
        ID.text = IdPlaceholder;
        PW.text = PwPlaceholder;
        btnLoginText.text = "Login";

    }
    private IEnumerator Start()
    {
        while (CoreManager.Instance == null || CoreManager.Instance.VM == null || CoreManager.Instance.VD == null)
        {
            yield return null; // 한 프레임 대기
        }
        _viewModel = CoreManager.Instance.VM;
        _viewModel.OnCharacterListReceived += OnLoginSuccess;
    }
    private void OnDestroy()
    {
        _viewModel.OnCharacterListReceived -= OnLoginSuccess;
    }

    public void OnLoginBtnClicked()
    {
        _viewModel.Login(txtID.text, txtPW.text);
    }
    private void OnLoginSuccess(bool sucesss)
    {
        if (sucesss)
            UnityEngine.SceneManagement.SceneManager.LoadScene("CharacterList");
    }

    public void OnPWSelect()
    {
        txtPW.contentType = TMP_InputField.ContentType.Password;
        txtPW.ForceLabelUpdate();
    }
}
