using ClientCore.PacketHelper;
using UnityEngine;

public class ActionManager : MonoBehaviour
{ 
    [SerializeField] private PlayerManager playerManager;
    [SerializeField] private MonsterManager monsterManager;
    [SerializeField] private SkillEffectController skillEffact;
    public void ProcessAction(ushort count, ActionResultField[] data)
    {
        for (int i = 0; i < count && i < data.Length; i++)
        {
            var action = data[i];
            if (action.skillPhase == 0)
            {
                if (data[i].casterType == 0)
                {
                    playerManager.SkillAnimation(action.casterId, action.skillSlot, action.skillId, action.dir);
                }
                else
                {
                    //monsterManager.SkillCharging(((ushort)action.casterId, action.skillId,  action.dir);
                }
                
            }
            StartSkillEffact(action.skillId, action.skillPhase, action.dir, action.x, action.y);
        }
    }
    private void StartSkillEffact(uint skillID, ushort phase, byte dir, float x, float y)
    {
        skillEffact.PlaySkill(skillID, phase, dir, x, y); 
    }
}
