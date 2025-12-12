using ClientCore;
using ClientCore.PacketHelper;
using System;
using System.Collections;
using System.Text;
using UnityEngine;
class PlayerState : MonoBehaviour
{
    public ulong zoneInternalID;
    public int HP;
    public int MP;
    public ushort level;
    public ulong exp;
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
        level = state.level;
        exp = state.exp;
        dir = state.dir;
        x = state.x;
        y = state.y;
        zoneInternalID = state.zoneInternalID;
    }
    public void UpdateDelta(DeltaUpdateField delta)
    {
        ulong packed = delta.fieldVal;
        byte[] bytes = BitConverter.GetBytes((uint)packed); // 하위 4바이트
        switch (delta.fieldID)
        {
            case 0: HP = (int)delta.fieldVal; break;
            case 1: MP = (int)delta.fieldVal; break;
            case 2: level = (ushort)delta.fieldVal; break;
            case 3: exp = (ulong)delta.fieldVal; break;
            case 4: dir = (byte)delta.fieldVal; Debug.Log(dir); break;
            case 5: x = BitConverter.ToSingle(bytes, 0); break;
            case 6: y = BitConverter.ToSingle(bytes, 0); break;
        }
    }
}