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
    class NoneZoneHandler{
        void Initialize(IIOCP* i, ILogger* l, ISessionAuth* s, PacketWriter* p, MessagePool* m, IMessageQueue* mq, StateManager* manager, LobbyZone* lobby);
        bool IsReady();
        void CheckSession(IPacketView* p);
        void GetCharacterList(IPacketView* p);
        void GetCharacterState(IPacketView* p);
        void GetInventory(IPacketView* p);
        void Chat(IPacketView* p);
        void ZoneChange(IPacketView* p);
        ISessionAuth* auth;
        IDBCache* cache;
        MessagePool* messagePool;
        IMessageQueue* messageQueue;
        LobbyZone* lobbyZone;
        friend class Initializer;
        
    public:
        void Process(IPacketView* p);
        void Disconnect(uint64_t sessionID);
        void Ping(uint64_t sessionID, uint64_t rtt, std::chrono::steady_clock::time_point now);
    };
}
