using System.Data.SqlTypes;
using TMPro;
using UnityEngine;
using UnityEngine.UI;

public class PlayerStatsUI : MonoBehaviour
{
    [SerializeField] private Image hpFill;
    [SerializeField] private Image mpFill;
    [SerializeField] private Image expFill;


    [SerializeField] private TextMeshProUGUI hpText;
    [SerializeField] private TextMeshProUGUI mpText;
    [SerializeField] private TextMeshProUGUI expText;

    void Start()
    {
    }

    public void SetHP(int current, int max)
    {
        if (hpFill != null)
            hpFill.fillAmount = Mathf.Clamp01((float)current / max);

        if (hpText != null)
            hpText.text = $"{current} / {max}";
    }

    public void SetMP(int current, int max)
    {
        if (mpFill != null)
            mpFill.fillAmount = Mathf.Clamp01((float)current / max);

        if (mpText != null)
            mpText.text = $"{current}  /  {max}";
    }

    public void SetEXP(ulong current, ulong max)
    {
        if (expFill != null)
            expFill.fillAmount = Mathf.Clamp01((float)current / max);

        if (expText != null)
            expText.text = $"{current}  /  {max}";
    }

    public void UpdateAll(PlayerState state)
    {
        SetHP(state.HP, state.MAXHP);
        SetMP(state.MP, state.MAXMP);
        SetEXP(state.exp, 1000);
    }
}
