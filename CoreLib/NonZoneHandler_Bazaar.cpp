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

namespace Core {
    void NonZoneHandler::BazaarBuy(IPacketView* p) {
        if (p->GetLength() != sizeof(PacketStruct<BazaarBuyRequestBody>))
            return;
        const auto* header = parseHeader(p->GetPtr());
        const auto request = *parseBody<BazaarBuyRequestBody>(p->GetPtr());
        if (header->magic != MAGIC || header->opcode != OP::BAZAAR_BUY
            || header->length != p->GetLength() || IsSimulationFlag(header->flags)
            || request.requestID == 0 || request.listingID == 0)
            return;
        const uint64_t characterID = stateManager->GetFieldCharacterID(p->GetSessionID());
        if (characterID == 0)
            return;
        if (auto msg = messagePool->AcquireScoped()) {
            auto* st = reinterpret_cast<MsgStruct<MsgBazaarBuyBody>*>(msg->GetBuffer());
            st->header = { p->GetSessionID(), MSG_BAZAAR_BUY };
            st->body = { characterID, request.listingID, request.requestID };
            msg->SetLength(sizeof(*st));
            if (messageQueue->EnqueueMessage(msg.get()))
                return;
        }
        MsgBazaarBuyResBody failed{};
        failed.requestID = request.requestID;
        failed.listingID = request.listingID;
        failed.resStatus = 5;
        if (stateManager->GetFieldCharacterID(p->GetSessionID()) != characterID)
            return;
        auto response = writer->WriteBazaarBuyResponse(&failed);
        if (response)
            networkSender->SendDataUnique(p->GetSessionID(), std::move(response));
    }

    void NonZoneHandler::BazaarRegister(IPacketView* p) {
        if (p->GetLength() != sizeof(PacketStruct<BazaarRegisterRequestBody>)) return;
        const auto* header = parseHeader(p->GetPtr());
        const auto request = *parseBody<BazaarRegisterRequestBody>(p->GetPtr());
        if (header->magic != MAGIC || header->opcode != OP::BAZAAR_REGISTER || header->length != p->GetLength()
            || IsSimulationFlag(header->flags) || request.requestID == 0 || request.inventorySlot >= MAX_INVENTORY_ITEMS
            || request.quantity == 0 || request.totalDiamondPrice == 0) return;
        const uint64_t characterID = stateManager->GetFieldCharacterID(p->GetSessionID());
        if (characterID == 0) return;

        if (auto msg = messagePool->AcquireScoped()) {
            auto* st = reinterpret_cast<MsgStruct<MsgBazaarRegisterBody>*>(msg->GetBuffer());
            st->header = { p->GetSessionID(), MSG_BAZAAR_REGISTER };
            st->body = { characterID, 0, request.quantity, request.totalDiamondPrice,
                request.requestID, request.inventorySlot };
            msg->SetLength(sizeof(*st));
            if (messageQueue->EnqueueMessage(msg.get())) return;
        }

        MsgBazaarRegisterResBody failed{};
        failed.requestID = request.requestID;
        failed.characterID = characterID;
        failed.resStatus = 0;
        if (stateManager->GetFieldCharacterID(p->GetSessionID()) != characterID) return;
        auto response = writer->WriteBazaarRegisterResponse(&failed);
        if (response) networkSender->SendDataUnique(p->GetSessionID(), std::move(response));
    }

    void NonZoneHandler::BazaarOutboxList(IPacketView* p) {
        if (p->GetLength() != sizeof(PacketStruct<BazaarOutboxListRequestBody>)) return;
        const auto* header = parseHeader(p->GetPtr());
        const auto request = *parseBody<BazaarOutboxListRequestBody>(p->GetPtr());
        if (header->magic != MAGIC || header->opcode != OP::BAZAAR_OUTBOX_LIST
            || header->length != p->GetLength() || IsSimulationFlag(header->flags) || request.requestID == 0) return;
        const uint64_t characterID = stateManager->GetFieldCharacterID(p->GetSessionID());
        if (characterID == 0) return;
        if (auto msg = messagePool->AcquireScoped()) {
            auto* st = reinterpret_cast<MsgStruct<MsgBazaarOutboxListBody>*>(msg->GetBuffer());
            st->header = { p->GetSessionID(), MSG_BAZAAR_OUTBOX_LIST };
            st->body = { characterID, request.requestID };
            msg->SetLength(sizeof(*st));
            if (messageQueue->EnqueueMessage(msg.get())) return;
        }
        MsgBazaarOutboxListResBody failed{};
        failed.requestID = request.requestID;
        failed.characterID = characterID;
        if (stateManager->GetFieldCharacterID(p->GetSessionID()) != characterID) return;
        auto response = writer->WriteBazaarOutboxListResponse(&failed);
        if (response) networkSender->SendDataUnique(p->GetSessionID(), std::move(response));
    }

    void NonZoneHandler::BazaarCheckOutbox(IPacketView* p) {
        if (p->GetLength() != sizeof(PacketStruct<BazaarCheckOutboxRequestBody>))
            return;
        const auto* header = parseHeader(p->GetPtr());
        const auto request = *parseBody<BazaarCheckOutboxRequestBody>(p->GetPtr());
        if (header->magic != MAGIC || header->opcode != OP::BAZAAR_CHECK_OUTBOX
            || header->length != p->GetLength() || IsSimulationFlag(header->flags) || request.requestID == 0)
            return;
        const uint64_t characterID = stateManager->GetFieldCharacterID(p->GetSessionID());
        if (characterID == 0)
            return;
        if (auto msg = messagePool->AcquireScoped()) {
            auto* st = reinterpret_cast<MsgStruct<MsgBazaarCheckOutboxBody>*>(msg->GetBuffer());
            st->header = { p->GetSessionID(), MSG_BAZAAR_CHECK_OUTBOX };
            st->body = { characterID, request.requestID };
            msg->SetLength(sizeof(*st));
            if (messageQueue->EnqueueMessage(msg.get()))
                return;
        }
        MsgBazaarCheckOutboxResBody failed{};
        failed.requestID = request.requestID;
        if (stateManager->GetFieldCharacterID(p->GetSessionID()) != characterID)
            return;
        auto response = writer->WriteBazaarCheckOutboxResponse(&failed);
        if (response)
            networkSender->SendDataUnique(p->GetSessionID(), std::move(response));
    }

    void NonZoneHandler::BazaarCancelListing(IPacketView* p) {
        if (p->GetLength() != sizeof(PacketStruct<BazaarCancelListingRequestBody>)) return;
        const auto* header = parseHeader(p->GetPtr());
        const auto request = *parseBody<BazaarCancelListingRequestBody>(p->GetPtr());
        if (header->magic != MAGIC || header->opcode != OP::BAZAAR_CANCEL_LISTING
            || header->length != p->GetLength() || IsSimulationFlag(header->flags)
            || request.requestID == 0 || request.listingID == 0) return;
        const uint64_t characterID = stateManager->GetFieldCharacterID(p->GetSessionID());
        if (characterID == 0) return;
        if (auto msg = messagePool->AcquireScoped()) {
            auto* st = reinterpret_cast<MsgStruct<MsgBazaarCancelListingBody>*>(msg->GetBuffer());
            st->header = { p->GetSessionID(), MSG_BAZAAR_CANCEL_LISTING };
            st->body = { characterID, request.listingID, request.requestID };
            msg->SetLength(sizeof(*st));
            if (messageQueue->EnqueueMessage(msg.get())) return;
        }
        MsgBazaarCancelListingResBody failed{};
        failed.requestID = request.requestID;
        failed.listingID = request.listingID;
        failed.characterID = characterID;
        failed.resStatus = 5;
        if (stateManager->GetFieldCharacterID(p->GetSessionID()) != characterID) return;
        auto response = writer->WriteBazaarCancelListingResponse(&failed);
        if (response) networkSender->SendDataUnique(p->GetSessionID(), std::move(response));
    }

    void NonZoneHandler::BazaarClaim(IPacketView* p) {
        if (p->GetLength() != sizeof(PacketStruct<BazaarClaimRequestBody>))
            return;
        const auto* header = parseHeader(p->GetPtr());
        const auto request = *parseBody<BazaarClaimRequestBody>(p->GetPtr());
        if (header->magic != MAGIC || header->opcode != OP::BAZAAR_CLAIM
            || header->length != p->GetLength() || IsSimulationFlag(header->flags)
            || request.requestID == 0 || request.listingID == 0)
            return;
        const uint64_t characterID = stateManager->GetFieldCharacterID(p->GetSessionID());
        if (characterID == 0)
            return;
        if (auto msg = messagePool->AcquireScoped()) {
            auto* st = reinterpret_cast<MsgStruct<MsgBazaarClaimBody>*>(msg->GetBuffer());
            st->header = { p->GetSessionID(), MSG_BAZAAR_CLAIM };
            st->body = { characterID, request.listingID, request.requestID };
            msg->SetLength(sizeof(*st));
            if (messageQueue->EnqueueMessage(msg.get()))
                return;
        }
        MsgBazaarClaimResBody failed{};
        failed.requestID = request.requestID;
        failed.listingID = request.listingID;
        if (stateManager->GetFieldCharacterID(p->GetSessionID()) != characterID)
            return;
        auto response = writer->WriteBazaarClaimResponse(&failed);
        if (response)
            networkSender->SendDataUnique(p->GetSessionID(), std::move(response));
    }

    void NonZoneHandler::BazaarMyList(IPacketView* p) {
        if (p->GetLength() != sizeof(PacketStruct<BazaarMyListRequestBody>))
            return;
        const auto* header = parseHeader(p->GetPtr());
        const auto request = *parseBody<BazaarMyListRequestBody>(p->GetPtr());
        if (header->length != p->GetLength() || IsSimulationFlag(header->flags) || request.requestID == 0)
            return;
        const uint64_t characterID = stateManager->GetFieldCharacterID(p->GetSessionID());
        if (characterID == 0)
            return;
        if (auto msg = messagePool->AcquireScoped()) {
            auto* st = reinterpret_cast<MsgStruct<MsgBazaarMyListBody>*>(msg->GetBuffer());
            st->header = { p->GetSessionID(), MSG_BAZAAR_MY_LIST };
            st->body = { request.requestID, characterID };
            msg->SetLength(sizeof(*st));
            if (messageQueue->EnqueueMessage(msg.get()))
                return;
        }
        MsgBazaarMyListResBody failed{};
        failed.requestID = request.requestID;
        failed.characterID = characterID;
        if (stateManager->GetFieldCharacterID(p->GetSessionID()) != characterID)
            return;
        auto response = writer->WriteBazaarMyListResponse(&failed);
        if (response)
            networkSender->SendDataUnique(p->GetSessionID(), std::move(response));
    }

    void NonZoneHandler::BazaarSearch(IPacketView* p) {
        if (p->GetLength() != sizeof(PacketStruct<BazaarSearchRequestBody>))
            return;
        const auto* header = parseHeader(p->GetPtr());
        const auto request = *parseBody<BazaarSearchRequestBody>(p->GetPtr());
        if (header->length != p->GetLength() || IsSimulationFlag(header->flags)
            || request.requestID == 0 || request.itemType < 1 || request.itemType > 3
            || !stateManager->IsFieldSession(p->GetSessionID()))
            return;

        if (auto msg = messagePool->AcquireScoped()) {
            auto* st = reinterpret_cast<MsgStruct<MsgBazaarSearchBody>*>(msg->GetBuffer());
            st->header = { p->GetSessionID(), MSG_BAZAAR_SEARCH };
            st->body = { request.requestID, request.itemType, request.page };
            msg->SetLength(sizeof(*st));
            if (messageQueue->EnqueueMessage(msg.get()))
                return;
        }
        MsgBazaarSearchResBody failed{};
        failed.requestID = request.requestID;
        auto response = writer->WriteBazaarSearchResponse(&failed);
        if (response)
            networkSender->SendDataUnique(p->GetSessionID(), std::move(response));
    }
}
