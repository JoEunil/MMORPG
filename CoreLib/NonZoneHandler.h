#pragma once
#include <cstdint>

#include "MessageTypes.h"
#include "MessagePool.h"

namespace Core {
    class ILogger;
    class ISessionAuth;
    class IPacketView;
    class IIOCP;
    class IDBCache;
    class PacketWriter;
    class IMessageQueue;
    class StateManager;
    class LobbyZone;
    class ChatThreadPool;
    class NonZoneHandler{
        void Initialize(IIOCP* i, ISessionAuth* s, PacketWriter* p, MessagePool* m, IMessageQueue* mq, StateManager* manager, LobbyZone* lobby, ChatThreadPool* c);
        bool IsReady();
        void CheckSession(IPacketView* p);
        void GetCharacterList(IPacketView* p);
        void GetCharacterState(IPacketView* p);
        void GetInventory(IPacketView* p);
        void Chat(IPacketView* p);
        void ZoneChange(IPacketView* p);
        ISessionAuth* auth = nullptr;
        IDBCache* cache = nullptr;
        MessagePool* messagePool = nullptr;
        IMessageQueue* messageQueue = nullptr;
        LobbyZone* lobbyZone = nullptr;
        ChatThreadPool* chat = nullptr;
        friend class Initializer;
        
    public:
        void Process(IPacketView* p);
        void Disconnect(uint64_t sessionID);
    };
}
