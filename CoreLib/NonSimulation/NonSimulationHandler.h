#pragma once
#include <cstdint>
#include <memory>
#include "../Interface/IPacketView.h"
#include "NonSimulationEvent.h"

namespace Core {
    class CacheCommandSender;
    class IZonePacketQueue;
    class IZoneRequestQueue;
    class ISessionAuth;
    class IServerResponseQueue;
    class PacketWriter;
    class SessionManager;
    class LobbyZone;
    class ChatThreadPool;
    class IProfileCache;
    struct ZoneTransfer;
    class NonSimulationHandler{
        struct EventVisitor {
            NonSimulationHandler& handler;

            void operator()(NonSimulationWorkEvent& event) const;
            void operator()(NonSimulationDisconnectEvent& event) const;
            void operator()(NonSimulationLobbyReturnEvent& event) const;
        };

        void Initialize(IServerResponseQueue& responseQueue, PacketWriter& packetWriter, ISessionAuth* s, const CacheCommandSender* commands, SessionManager* manager, LobbyZone* lobby, ChatThreadPool* c, IProfileCache* pc, IZonePacketQueue& packets, IZoneRequestQueue& requests);
        bool IsReady() const;
        void CheckSession(IPacketView* p);
        void ResponseSession(uint64_t sessionID, uint8_t resStatus, uint64_t userID);
        void GetCharacterList(IPacketView* p);
        void GetCharacterState(IPacketView* p);
        void GetInventory(IPacketView* p);
        void ForwardZoneRequest(std::unique_ptr<IPacketView, PacketViewDeleter> packet);
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
        IZonePacketQueue* zonePackets = nullptr;
        IZoneRequestQueue* zoneRequests = nullptr;
        IServerResponseQueue* responses = nullptr;
        PacketWriter* writer = nullptr;
        SessionManager* sessionManager = nullptr;
        ISessionAuth* auth = nullptr;
        const CacheCommandSender* cacheCommands = nullptr;
        LobbyZone* lobbyZone = nullptr;
        ChatThreadPool* chat = nullptr;
        IProfileCache* profileCache = nullptr;
        friend class Initializer;
    public:
        void Process(NonSimulationEvent& event);

        void Process(std::unique_ptr<IPacketView, PacketViewDeleter> packet);
        void Disconnect(uint64_t sessionID);
        void ReturnToLobby(const std::shared_ptr<ZoneTransfer>& transfer);

        void RequestProfileRename(uint64_t sessionID, uint32_t profileId, const char* name, uint16_t nameLen);
    };
}
