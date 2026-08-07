using ClientCore.PacketHelper;
using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using static ClientCore.ViewData;

namespace ClientCore
{
    public interface IMainThreadDispatcher
    {
        void Post(Action job);
    }

    public struct ActionData
    {
        public byte dir;
        public float x, y;   // 클라이언트가 로컬로 이동한 뒤의 최종 좌표
        public bool dirty;
        public byte waitSkillSlot;
    }
    public struct CharacterInfoView
    {
        public ulong CharacterID;
        public string Name;
        public ushort Level;
    }
    public struct MyCharData
    {
        public uint zoneInternalID;
        public string Name;
        public ushort Level;
        public uint Exp;
        public short HP;
        public short MP;
        public byte Dir;
        public float X;
        public float Y;
        public ushort zoneID;
    }


public struct InventoryItemView
    {
        public uint ItemID;
        public ushort Quantity;
        public byte Slot;
    }

    public interface ILogger
    {
        void Log(string message);
    }

    // UI → ViewModel 호출
    public interface IViewModelUI
    {
        event Action<bool> OnEnterSuccess;
        event Action<bool> OnCharacterListReceived;
        event Action<byte, ulong, string, string> OnChatReceived;
        event Action<bool> OnInventoryReceived;
        event Action<ushort, ulong, uint, float, float> OnZoneChageReceived;
        event Action<ushort, DeltaUpdateField[]> OnDeltaReceived;
        event Action<ushort, FullStateField[]> OnFullReceived;
        event Action<ushort, MonsterDeltaField[]> OnMonsterDeltaReceived;
        event Action<ushort, MonsterFullField[]> OnMonsterFullReceived;
        event Action<ushort, ActionResultField[]> OnActionResultReceived;
        event Action OnZoneChageFailed;
        event Action<ulong> OnPingReceived;
        Task Login(string id, string pwd);
        void Enter(ulong charID);
        void Chat(string message, byte scope, ulong targetID);
        void Error(string msg);
        void Log(string msg);
        void ZoneChange(int op);
    }

    // 네트워크 → ViewModel 호출
    public interface IViewModelNet
    {
        void AuthReceived(byte status);
        void ChatacterListReceived(byte resStatus, ushort ount, PacketHelper.CharacterInfo[] characters);
        void EnterReceived(byte resStatus, byte[] name, ushort attack, ushort level, uint exp, int hp, int mp, int maxHp, int maxMp, byte dir, float startX, float startY, ushort CurrentZone);
        void ChatReceived(Packet.Message message);
        void InventoryReceived(byte resStatus, PacketHelper.InventoryItem[] items);
        void ZoneChageReceived(byte resStatus, ushort zoneID, ulong chatID, uint zoneInternalID, float x, float y);
        void DeltaReceived(ushort count, PacketHelper.DeltaUpdateField[] updates);
        void FullReceived(ushort count, PacketHelper.FullStateField[] states);
        void MonsterDeltaReceived(ushort count, PacketHelper.MonsterDeltaField[] updates);
        void MonsterFullReceived(ushort count, PacketHelper.MonsterFullField[] states);
        void ActionResultReceived(ushort count, PacketHelper.ActionResultField[] states);
        void PingReceived(ulong servertimeMs, ulong rtt);
        void Error(string msg);
        void Log(string msg);
    }

    public interface IViewModelTick
    {
        void SendUpdate();
    }

    public interface IViewDataModel
    {
        long GetServerTimeMs();
        void UpdateServerTime(long serverTime);
        (bool, byte, float, float, byte) GetActionState();
        void SetPosition(float x, float y);
        void SetCharList(ushort count, CharacterInfo[] chars);
        void SetInventory(InventoryItem[] items);
    }

    public interface IViewDataUI
    {
        long GetServerTimeMs();
        void UpdateMove(byte dir, float x, float y);
        void UpdateSkill(byte skillSlot);
        (ushort, List<CharacterInfoView>) GetCharList();
        List<InventoryItemView> GetInventory();
    }
}

namespace ClientCore.Network
{
    public interface IReceiveTick
    {
        void Enqueue(byte[] buffer);
    }
    public interface ITCPSocket
    {
        Task Connect(string address, int port);
        void Send(byte[] binary);
    }
    public interface IHandlerSock
    {
        void HandleResponse(byte[] buffer);
    }
    public interface IHandlerTick
    {
        void HandleZoneChangeRes(PacketHelper.STPacket<PacketHelper.ZoneChangeResponseBody> packet);
        void HandleDeltaUpdate((ushort, DeltaUpdateField[]) res);
        void HandleFullState((ushort, FullStateField[]) res);
        void HandleMonsterDelta((ushort, MonsterDeltaField[]) res);
        void HandleMonsterFull((ushort, MonsterFullField[]) res);
        void HandleActionReult((ushort, ActionResultField[]) res);
    }
}

namespace ClientCore.Services
{
    public interface INetworkService
    {
        Task Connect(string address, int port);
        void CharacterList();
        void Enter(ulong charID);

        void Chat(string message, byte scope, ulong targetID);
        void Action(byte dir, float x, float y, byte skillSlot);
        void Pong(ulong serverTimeMs);
        void ZoneChange(byte op);
        void Log(string msg);
    }
}
