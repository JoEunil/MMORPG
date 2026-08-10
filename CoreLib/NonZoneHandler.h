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
    class IProfileCache;
    class NonZoneHandler{
        void Initialize(IIOCP* i, ISessionAuth* s, PacketWriter* p, MessagePool* m, IMessageQueue* mq, StateManager* manager, LobbyZone* lobby, ChatThreadPool* c);
        void InitializeProfileCache(IProfileCache* pc);
        bool IsReady();
        void CheckSession(IPacketView* p);
        void GetCharacterList(IPacketView* p);
        void GetCharacterState(IPacketView* p);
        void GetInventory(IPacketView* p);
        void Chat(IPacketView* p);
        void ZoneChange(IPacketView* p);
        void GetProfileBatch(IPacketView* p);
        ISessionAuth* auth = nullptr;
        IDBCache* cache = nullptr;
        MessagePool* messagePool = nullptr;
        IMessageQueue* messageQueue = nullptr;
        LobbyZone* lobbyZone = nullptr;
        ChatThreadPool* chat = nullptr;
        IProfileCache* profileCache = nullptr;
        friend class Initializer;
        
    public:
        void Process(IPacketView* p);
        void Disconnect(uint64_t sessionID);

        void RequestProfileRename(uint64_t sessionID, uint32_t profileId, const char* name, uint16_t nameLen);
    };
}
