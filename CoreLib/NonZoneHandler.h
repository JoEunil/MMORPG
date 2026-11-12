#pragma once
#include <cstdint>

#include "MessageTypes.h"
#include "MessagePool.h"

namespace Core {
    class ILogger;
    class ISessionAuth;
    class IPacketView;
    class INetworkSender;
    class IDBCache;
    class PacketWriter;
    class IMessageQueue;
    class StateManager;
    class LobbyZone;
    class ChatThreadPool;
    class IProfileCache;
    class NonZoneHandler{
        template<typename T>
        static MsgStruct<T>* InitializeMsg(Message& msg, MsgHeader header) {
            static_assert(sizeof(MsgStruct<T>) <= MESSAGE_LEN);
            auto* request = reinterpret_cast<MsgStruct<T>*>(msg.GetBuffer());
            request->header = header;
            msg.SetLength(sizeof(*request));
            return request;
        }

        void Initialize(INetworkSender* sender, ISessionAuth* s, PacketWriter* p, MessagePool* m, IMessageQueue* mq, StateManager* manager, LobbyZone* lobby, ChatThreadPool* c, IProfileCache* pc);
        bool IsReady();
        void CheckSession(IPacketView* p);
        void ResponseSession(uint64_t sessionID, uint8_t resStatus, uint64_t userID);
        void GetCharacterList(IPacketView* p);
        void GetCharacterState(IPacketView* p);
        void GetInventory(IPacketView* p);
        void BazaarSearch(IPacketView* p);
        void BazaarRegister(IPacketView* p);
        void WalletRequest(IPacketView* p);
        void BazaarMyList(IPacketView* p);
        void BazaarClaim(IPacketView* p);
        void BazaarBuy(IPacketView* p);
        void BazaarOutboxList(IPacketView* p);
        void BazaarCheckOutbox(IPacketView* p);
        void BazaarCancelListing(IPacketView* p);
        void Chat(IPacketView* p);
        void ZoneChange(IPacketView* p);
        void RespawnToTown(IPacketView* p);
        void GetProfileBatch(IPacketView* p);
        INetworkSender* networkSender = nullptr;
        PacketWriter* writer = nullptr;
        StateManager* stateManager = nullptr;
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
