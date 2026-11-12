#include "pch.h"
#include "NonZoneHandler.h"

#include "Config.h"
#include "IMessageQueue.h"
#include "INetworkSender.h"
#include "IPacketView.h"
#include "Message.h"
#include "PacketTypes.h"
#include "PacketWriter.h"
#include "StateManager.h"

#include <optional>

namespace Core {
    namespace {
        template<typename T>
        std::optional<T> ParseBazaarRequest(const IPacketView& packet, std::optional<OP> expectedOpcode = std::nullopt) {
            const auto length = packet.GetLength();
            if (length != sizeof(PacketStruct<T>))
                return std::nullopt;

            auto* buffer = packet.GetPtr();
            const auto* header = parseHeader(buffer);
            if (header->length != length || IsSimulationFlag(header->flags))
                return std::nullopt;
            if (expectedOpcode && (header->magic != MAGIC || header->opcode != *expectedOpcode))
                return std::nullopt;

            const auto request = *parseBody<T>(buffer);
            if (request.requestID == 0)
                return std::nullopt;
            return request;
        }
    }

    void NonZoneHandler::BazaarBuy(IPacketView* p) {
        const auto request = ParseBazaarRequest<BazaarBuyRequestBody>(*p, OP::BAZAAR_BUY);
        if (!request || request->listingID == 0)
            return;
        const uint64_t characterID = stateManager->GetFieldCharacterID(p->GetSessionID());
        if (characterID == 0)
            return;
        if (auto msg = messagePool->AcquireScoped()) {
            auto* st = InitializeMsg<MsgBazaarBuyBody>(*msg, { p->GetSessionID(), MSG_BAZAAR_BUY });
            st->body = { characterID, request->listingID, request->requestID };
            if (messageQueue->EnqueueMessage(msg.get()))
                return;
        }
        MsgBazaarBuyResBody failed{};
        failed.requestID = request->requestID;
        failed.listingID = request->listingID;
        failed.resStatus = 5;
        if (stateManager->GetFieldCharacterID(p->GetSessionID()) != characterID)
            return;
        auto response = writer->WriteBazaarBuyResponse(&failed);
        if (response)
            networkSender->SendDataUnique(p->GetSessionID(), std::move(response));
    }

    void NonZoneHandler::BazaarRegister(IPacketView* p) {
        const auto request = ParseBazaarRequest<BazaarRegisterRequestBody>(*p, OP::BAZAAR_REGISTER);
        if (!request || request->inventorySlot >= MAX_INVENTORY_ITEMS
            || request->quantity == 0 || request->totalDiamondPrice == 0)
            return;
        const uint64_t characterID = stateManager->GetFieldCharacterID(p->GetSessionID());
        if (characterID == 0) return;

        if (auto msg = messagePool->AcquireScoped()) {
            auto* st = InitializeMsg<MsgBazaarRegisterBody>(*msg, { p->GetSessionID(), MSG_BAZAAR_REGISTER });
            st->body = { characterID, 0, request->quantity, request->totalDiamondPrice,
                request->requestID, request->inventorySlot };
            if (messageQueue->EnqueueMessage(msg.get())) return;
        }

        MsgBazaarRegisterResBody failed{};
        failed.requestID = request->requestID;
        failed.characterID = characterID;
        failed.resStatus = 0;
        if (stateManager->GetFieldCharacterID(p->GetSessionID()) != characterID) return;
        auto response = writer->WriteBazaarRegisterResponse(&failed);
        if (response) networkSender->SendDataUnique(p->GetSessionID(), std::move(response));
    }

    void NonZoneHandler::BazaarOutboxList(IPacketView* p) {
        const auto request = ParseBazaarRequest<BazaarOutboxListRequestBody>(*p, OP::BAZAAR_OUTBOX_LIST);
        if (!request)
            return;
        const uint64_t characterID = stateManager->GetFieldCharacterID(p->GetSessionID());
        if (characterID == 0) return;
        if (auto msg = messagePool->AcquireScoped()) {
            auto* st = InitializeMsg<MsgBazaarOutboxListBody>(*msg, { p->GetSessionID(), MSG_BAZAAR_OUTBOX_LIST });
            st->body = { characterID, request->requestID };
            if (messageQueue->EnqueueMessage(msg.get())) return;
        }
        MsgBazaarOutboxListResBody failed{};
        failed.requestID = request->requestID;
        failed.characterID = characterID;
        if (stateManager->GetFieldCharacterID(p->GetSessionID()) != characterID) return;
        auto response = writer->WriteBazaarOutboxListResponse(&failed);
        if (response) networkSender->SendDataUnique(p->GetSessionID(), std::move(response));
    }

    void NonZoneHandler::BazaarCheckOutbox(IPacketView* p) {
        const auto request = ParseBazaarRequest<BazaarCheckOutboxRequestBody>(*p, OP::BAZAAR_CHECK_OUTBOX);
        if (!request)
            return;
        const uint64_t characterID = stateManager->GetFieldCharacterID(p->GetSessionID());
        if (characterID == 0)
            return;
        if (auto msg = messagePool->AcquireScoped()) {
            auto* st = InitializeMsg<MsgBazaarCheckOutboxBody>(*msg, { p->GetSessionID(), MSG_BAZAAR_CHECK_OUTBOX });
            st->body = { characterID, request->requestID };
            if (messageQueue->EnqueueMessage(msg.get()))
                return;
        }
        MsgBazaarCheckOutboxResBody failed{};
        failed.requestID = request->requestID;
        if (stateManager->GetFieldCharacterID(p->GetSessionID()) != characterID)
            return;
        auto response = writer->WriteBazaarCheckOutboxResponse(&failed);
        if (response)
            networkSender->SendDataUnique(p->GetSessionID(), std::move(response));
    }

    void NonZoneHandler::BazaarCancelListing(IPacketView* p) {
        const auto request = ParseBazaarRequest<BazaarCancelListingRequestBody>(*p, OP::BAZAAR_CANCEL_LISTING);
        if (!request || request->listingID == 0)
            return;
        const uint64_t characterID = stateManager->GetFieldCharacterID(p->GetSessionID());
        if (characterID == 0) return;
        if (auto msg = messagePool->AcquireScoped()) {
            auto* st = InitializeMsg<MsgBazaarCancelListingBody>(*msg, { p->GetSessionID(), MSG_BAZAAR_CANCEL_LISTING });
            st->body = { characterID, request->listingID, request->requestID };
            if (messageQueue->EnqueueMessage(msg.get())) return;
        }
        MsgBazaarCancelListingResBody failed{};
        failed.requestID = request->requestID;
        failed.listingID = request->listingID;
        failed.characterID = characterID;
        failed.resStatus = 5;
        if (stateManager->GetFieldCharacterID(p->GetSessionID()) != characterID) return;
        auto response = writer->WriteBazaarCancelListingResponse(&failed);
        if (response) networkSender->SendDataUnique(p->GetSessionID(), std::move(response));
    }

    void NonZoneHandler::BazaarClaim(IPacketView* p) {
        const auto request = ParseBazaarRequest<BazaarClaimRequestBody>(*p, OP::BAZAAR_CLAIM);
        if (!request || request->listingID == 0)
            return;
        const uint64_t characterID = stateManager->GetFieldCharacterID(p->GetSessionID());
        if (characterID == 0)
            return;
        if (auto msg = messagePool->AcquireScoped()) {
            auto* st = InitializeMsg<MsgBazaarClaimBody>(*msg, { p->GetSessionID(), MSG_BAZAAR_CLAIM });
            st->body = { characterID, request->listingID, request->requestID };
            if (messageQueue->EnqueueMessage(msg.get()))
                return;
        }
        MsgBazaarClaimResBody failed{};
        failed.requestID = request->requestID;
        failed.listingID = request->listingID;
        if (stateManager->GetFieldCharacterID(p->GetSessionID()) != characterID)
            return;
        auto response = writer->WriteBazaarClaimResponse(&failed);
        if (response)
            networkSender->SendDataUnique(p->GetSessionID(), std::move(response));
    }

    void NonZoneHandler::BazaarMyList(IPacketView* p) {
        const auto request = ParseBazaarRequest<BazaarMyListRequestBody>(*p);
        if (!request)
            return;
        const uint64_t characterID = stateManager->GetFieldCharacterID(p->GetSessionID());
        if (characterID == 0)
            return;
        if (auto msg = messagePool->AcquireScoped()) {
            auto* st = InitializeMsg<MsgBazaarMyListBody>(*msg, { p->GetSessionID(), MSG_BAZAAR_MY_LIST });
            st->body = { request->requestID, characterID };
            if (messageQueue->EnqueueMessage(msg.get()))
                return;
        }
        MsgBazaarMyListResBody failed{};
        failed.requestID = request->requestID;
        failed.characterID = characterID;
        if (stateManager->GetFieldCharacterID(p->GetSessionID()) != characterID)
            return;
        auto response = writer->WriteBazaarMyListResponse(&failed);
        if (response)
            networkSender->SendDataUnique(p->GetSessionID(), std::move(response));
    }

    void NonZoneHandler::BazaarSearch(IPacketView* p) {
        const auto request = ParseBazaarRequest<BazaarSearchRequestBody>(*p);
        if (!request || request->itemType < 1 || request->itemType > 3
            || !stateManager->IsFieldSession(p->GetSessionID()))
            return;

        if (auto msg = messagePool->AcquireScoped()) {
            auto* st = InitializeMsg<MsgBazaarSearchBody>(*msg, { p->GetSessionID(), MSG_BAZAAR_SEARCH });
            st->body = { request->requestID, request->itemType, request->page };
            if (messageQueue->EnqueueMessage(msg.get()))
                return;
        }
        MsgBazaarSearchResBody failed{};
        failed.requestID = request->requestID;
        auto response = writer->WriteBazaarSearchResponse(&failed);
        if (response)
            networkSender->SendDataUnique(p->GetSessionID(), std::move(response));
    }
}
