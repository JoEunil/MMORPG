using UnityEngine;
using UnityEngine.UI;

public class SkillUI : MonoBehaviour
{
    [SerializeField] private Image[] skillFill;

    private float[] cooldownTimes;    // 총 쿨다운 시간
    private float[] cooldownEndTime;  // 쿨다운 종료 시간(Time.time 기준)

    private void Start()
    {
        int count = skillFill.Length;
        cooldownTimes = new float[count];
        cooldownEndTime = new float[count];

        for (int i = 0; i < count; i++)
        {
            cooldownTimes[i] = SkillDB.Data[(uint)i].coolDownTime; // DB에서 읽어오기
            cooldownEndTime[i] = 0f; // 시작 시 쿨다운 없음
        }
    }

    private void Update()
    {
        float now = Time.time;

        for (int i = 0; i < skillFill.Length; i++)
        {
            float remaining = Mathf.Max(0f, cooldownEndTime[i] - now);
            skillFill[i].fillAmount = remaining / cooldownTimes[i];
        }
    }

    public void UseSkill(int index)
    {
        if (index < 0 || index >= skillFill.Length) return;

        cooldownEndTime[index] = Time.time + cooldownTimes[index];
    }
}
