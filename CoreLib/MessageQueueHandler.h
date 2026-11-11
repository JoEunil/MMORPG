#pragma once

#include <memory>
#include <cstdint>

#include "MessageTypes.h"
#include "LoggerGlobal.h"

namespace Core {
    class PacketWriter;
    class Message;
    class MessagePool;
    class INetworkSender;
    class LobbyZone;
    class StateManager;

    class MessageQueueHandler {
        // 캐시 or db 요청 결과 처리
        PacketWriter* writer = nullptr;
        INetworkSender* networkSender = nullptr;
        LobbyZone* lobbyZone = nullptr;
        MessagePool* messagePool = nullptr;
        StateManager* stateManager = nullptr;
        void Initialize(INetworkSender* sender, PacketWriter* w, LobbyZone* lo, MessagePool* msgPool) noexcept {
            networkSender = sender;
            writer = w;
            lobbyZone = lo;
            messagePool = msgPool;
        }
        void InitializeStateManager(StateManager* s) noexcept {
            stateManager = s;
        }
        bool IsReady() {
            if (networkSender == nullptr || writer == nullptr)
                return false;
            return true;
        }

        void CharacterListResponse(uint64_t sessionID, MsgCharacterListResBody* body);
        void CharacterStateResponse(uint64_t sessionID, MsgCharacterStateResBody* body);
        void BazaarBuyResponse(uint64_t sessionID, MsgBazaarBuyResBody* body);
        void BazaarOutboxListResponse(uint64_t sessionID, MsgBazaarOutboxListResBody* body);
        void BazaarCheckOutboxResponse(uint64_t sessionID, MsgBazaarCheckOutboxResBody* body);
        void BazaarCancelListingResponse(uint64_t sessionID, MsgBazaarCancelListingResBody* body);
        void BazaarClaimResponse(uint64_t sessionID, MsgBazaarClaimResBody* body);
        void BazaarMyListResponse(uint64_t sessionID, MsgBazaarMyListResBody* body);
        void BazaarSearchResponse(uint64_t sessionID, MsgBazaarSearchResBody* body);
        void BazaarRegisterResponse(uint64_t sessionID, MsgBazaarRegisterResBody* body);
        void WalletGoldResponse(uint64_t sessionID, MsgCurrencyResBody* body);
        void WalletDiamondResponse(uint64_t sessionID, MsgDiamondResBody* body);
        void InventoryResponse(uint64_t sessionID, MsgInventoryResBody* body);
        void InventoryUpdateResponse(uint64_t sessionID, MsgInventoryUpdateResBody* body);
        void ProfileRenameResponse(uint64_t sessionID, MsgProfileRenameResBody* body);


        friend class Initializer;
    public:
        void Process(Message* m);
    };
}
