using System.Collections;
using UnityEngine;
using UnityEngine.Rendering;

public class SkillEffectController : MonoBehaviour
{
    public static SkillEffectController Instance;

    private void Awake()
    {
        Instance = this;
    }
    private float DirToAngle(byte dir)
    {
        switch (dir)
        {
            case 0: return 90f;
            case 1: return -90f;   
            case 2: return -180f;
            case 3: return 0f;
            default: return 0f;
        }
    }
    public void PlaySkill(uint skillID, ushort phase, byte dir, float x, float y)
    {
        var address = SkillDB.Data[skillID].phases[phase];
        var prefab = Resources.Load<GameObject>(address);
        if (prefab == null)
        {
            Debug.Log($"Cannot load prefab: {address}");
            return;
        }
        Vector3 pos = new Vector3(x, y, 0);
        // 방향 적용
        float angle = 0;
        if (SkillDB.Data[skillID].rotate)
        {
            angle = DirToAngle(dir);
        }
        Debug.Log(angle);
        GameObject obj = Instantiate(prefab, pos, Quaternion.Euler(0, 0, angle));

    }
}
