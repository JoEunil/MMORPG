using System.Collections.Generic;
using UnityEngine;
[System.Serializable]
public class SkillData
{
    public uint skillID;

    // 애니메이션
    public string castAnimName;
    public string fireAnimName;

    public string[] phases;
    public bool rotate;
    // 클라에서만 사용
    public float coolDownTime; 
}


public static class SkillDB
{
    public static readonly Dictionary<uint, SkillData> Data;
    static SkillDB()
    {
        Data = new Dictionary<uint, SkillData>();

        // Basic Attack 0
        Data[0] = new SkillData
        {
            skillID = 0,
            coolDownTime = 1.0f,
            rotate = false,
            phases = new string[] { "Prefabs/Skill0_ready", "Prefabs/Skill0_hit" },
        };

        // AoE 1
        Data[1] = new SkillData
        {
            skillID = 1,
            coolDownTime = 6.0f,
            rotate = false,
            phases = new string[] { "Prefabs/Skill1_ready", "Prefabs/Skill1_hit" },
        };

        // Rect skill 2
        Data[2] = new SkillData
        {
            skillID = 2,
            coolDownTime = 4.0f,
            rotate = true,
            phases = new string[] { "Prefabs/Skill2_ready", "Prefabs/Skill2_hit" },
        };

        // Boss 1
        Data[3] = new SkillData
        {
            skillID = 3,
            coolDownTime = 0.0f,
            rotate = false,
            phases = new string[] { "Prefabs/Skill3_0", "Prefabs/Skill3_1", "Prefabs/Skill3_2", "Prefabs/Skill3_3" },
        };

        // Boss 2
        Data[4] = new SkillData
        {
            skillID = 4,
            coolDownTime = 0.0f,
            rotate = false,
            phases = new string[] { "Prefabs/Skill4_0", "Prefabs/Skill4_1", "Prefabs/Skill4_2", "Prefabs/Skill4_3", "Prefabs/Skill4_4", "Prefabs/Skill4_5", "Prefabs/Skill4_6", "Prefabs/Skill4_7", "Prefabs/Skill4_8", "Prefabs/Skill4_9" },
        };
    }
}
