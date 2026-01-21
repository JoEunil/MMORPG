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

    public struct MoveData
    {
        public byte dir;
        public float speed;
        public bool dirty;
    }
    public struct CharacterInfoView
    {
        public ulong CharacterID;
        public string Name;
        public ushort Level;
    }
    public struct MyCharData
    {
        public ulong zoneInternalID;
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

    public struct CharacterState 
    {
        ushort zoneInternalID; 
        int hp; // 0
        int mp; // 1
        ushort level; // 2
        ulong exp; // 3
        ushort dir; // 4
        float x, y;  // 5, 6
    };

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
        event Action<ulong, string> OnChatReceived;
        event Action<bool> OnInventoryReceived;
        event Action<ushort, ulong, float, float> OnZoneChageReceived;
        event Action<ushort, DeltaUpdateField[]> OnDeltaReceived;
        event Action<ushort, FullStateField[]> OnFullReceived;
        event Action OnZoneChageFailed;
        event Action<ulong> OnPingReceived;
        Task Login(string id, string pwd);
        void Enter(ulong charID);
        void Chat(string message);
        void Error(string msg);
        void Log(string msg);
        void ZoneChange(int op);
    }

    // 네트워크 → ViewModel 호출
    public interface IViewModelNet
    {
        void AuthReceived(byte status);
        void ChatacterListReceived(byte resStatus, ushort ount, PacketHelper.CharacterInfo[] characters);
        void EnterReceived(byte resStatus, byte[] name, ushort level, uint exp, short hp, short mp, byte dir, float startX, float startY, ushort CurrentZone);
        void ChatReceived(ulong sender, string message);
        void InventoryReceived(byte resStatus, PacketHelper.InventoryItem[] items);
        void ZoneChageReceived(byte resStatus, ushort zoneID, ulong zoneInternalID, float x, float y);
        void DeltaReceived(ushort count, PacketHelper.DeltaUpdateField[] updates);
        void FullReceived(ushort count, PacketHelper.FullStateField[] states);
        void PingReceived(ulong servertimeNs, ulong rtt);
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
        (bool, byte, float) GetMoveState();
        void SetCharList(ushort count, CharacterInfo[] chars);
        void SetInventory(InventoryItem[] items);
    }

    public interface IViewDataUI
    {
        long GetServerTimeMs();
        void UpdateMove(byte dir, float speed);
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
    }
}

namespace ClientCore.Services
{
    public interface INetworkService
    {
        Task Connect(string address, int port);
        void CharacterList();
        void Enter(ulong charID);

        void Chat(string message);
        void Move(byte dir, float speed);
        void Pong(ulong serverTimeNs);
        void ZoneChange(byte op);
        void Log(string msg);
    }
}
