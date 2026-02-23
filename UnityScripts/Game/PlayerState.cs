using ClientCore;
using ClientCore.PacketHelper;
using System;
using System.Collections;
using System.Text;
using UnityEngine;
public class PlayerState : MonoBehaviour
{
    public ulong zoneInternalID;
    public int HP;
    public int MP;
    public int MAXHP;
    public int MAXMP;
    public uint attack;
    public ulong exp;
    public ushort level;
    public byte dir;
    public float x, y;
    public string charName;
    public void ZoneMove(ulong id, float X, float Y)
    {
        zoneInternalID = id;
        x = X;
        y = Y;
    }
    public void SetPlayer(FullStateField state)
    {
        charName = Encoding.UTF8.GetString(state.charName);
        HP = state.hp;
        MP = state.mp;
        MAXHP = state.maxHp;
        MAXMP = state.maxMp;
        exp = state.exp;
        level = state.level;
        dir = state.dir;
        x = state.x;
        y = state.y;
        zoneInternalID = state.zoneInternalID;
    }
    public void UpdateDelta(DeltaUpdateField delta)
    { 
        uint raw = delta.fieldVal;
        byte[] bytes = BitConverter.GetBytes((uint)raw); 
        switch (delta.fieldID)
        {
            case 0: HP = (int)raw; break;
            case 1: MP = (int)raw; break;
            case 2: MAXHP = (int)raw; break;
            case 3: MAXMP = (int)raw; break;
            case 4: exp = (uint)raw; break;
            case 5: level = (ushort)raw; break;
            case 6: dir = (byte)raw; break;
            case 7: x = BitConverter.ToSingle(bytes, 0); break;
            case 8: y = BitConverter.ToSingle(bytes, 0); break;
        }
    }
}