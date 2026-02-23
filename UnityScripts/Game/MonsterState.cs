using ClientCore;
using ClientCore.PacketHelper;
using System;
using System.Collections;
using System.Text;
using UnityEngine;
public class MonsterState : MonoBehaviour
{
    public ushort internalId;
    public int HP;
    public int MAXHP;
    public byte dir;
    public float x, y;
    public uint damage = 0;
    public void SetMonster(MonsterFullField field)
    {
        internalId = field.internalId;
        HP = field.hp;
        MAXHP = field.maxHp;
        dir = field.dir;
        x = field.x;
        y = field.y;
        damage = field.attacked;
    }
    public void UpdateDelta(MonsterDeltaField delta)
    {
        uint raw = delta.fieldVal;
        byte[] bytes = BitConverter.GetBytes((uint)raw);
        switch (delta.fieldId)
        {
            case 0: HP = (int)raw; break;
            case 1: x = BitConverter.ToSingle(bytes, 0); break;
            case 2: y = BitConverter.ToSingle(bytes, 0); break;
            case 3: dir = (byte)raw; break;
            case 4: damage = raw; break;
        }
    }
}