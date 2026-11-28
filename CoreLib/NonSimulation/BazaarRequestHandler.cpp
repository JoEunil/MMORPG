#include "pch.h"
#include "BazaarRequestHandler.h"
#include "../Cache/CacheCommandSender.h"
#include "../Packet/PacketTypes.h"
#include "../Response/Interface/IServerResponseQueue.h"
#include "../Packet/PacketWriter.h"
#include "../World/SessionManager.h"
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

    void BazaarRequestHandler::BazaarBuy(IPacketView* p) {
        const auto request = ParseBazaarRequest<BazaarBuyRequestBody>(*p, OP::BAZAAR_BUY);
        if (!request || request->listingID == 0)
            return;
        const uint64_t characterID = sessionManager->GetFieldCharacterID(p->GetSessionID());
        if (characterID == 0)
            return;
        if (cacheCommands->EnqueueBazaarBuy({p->GetSessionID(), characterID, request->requestID}, request->listingID) != Base::EnqueueResult::Rejected)
            return;
        MsgBazaarBuyResBody failed{};
        failed.requestID = request->requestID;
        failed.listingID = request->listingID;
        failed.resStatus = 5;
        if (sessionManager->GetFieldCharacterID(p->GetSessionID()) != characterID)
            return;
        responses->Enqueue(p->GetSessionID(), writer->WriteBazaarBuyResponse(&failed));
    }

    void BazaarRequestHandler::BazaarRegister(IPacketView* p) {
        const auto request = ParseBazaarRequest<BazaarRegisterRequestBody>(*p, OP::BAZAAR_REGISTER);
        if (!request || request->inventorySlot >= MAX_INVENTORY_ITEMS
            || request->quantity == 0 || request->totalDiamondPrice == 0)
            return;
        const uint64_t characterID = sessionManager->GetFieldCharacterID(p->GetSessionID());
        if (characterID == 0) return;

        if (cacheCommands->EnqueueBazaarRegister({p->GetSessionID(), characterID, request->requestID}, request->quantity,
                request->totalDiamondPrice, request->inventorySlot) != Base::EnqueueResult::Rejected)
            return;

        MsgBazaarRegisterResBody failed{};
        failed.requestID = request->requestID;
        failed.characterID = characterID;
        failed.resStatus = 0;
        if (sessionManager->GetFieldCharacterID(p->GetSessionID()) != characterID) return;
        responses->Enqueue(p->GetSessionID(), writer->WriteBazaarRegisterResponse(&failed));
    }

    void BazaarRequestHandler::BazaarOutboxList(IPacketView* p) {
        const auto request = ParseBazaarRequest<BazaarOutboxListRequestBody>(*p, OP::BAZAAR_OUTBOX_LIST);
        if (!request)
            return;
        const uint64_t characterID = sessionManager->GetFieldCharacterID(p->GetSessionID());
        if (characterID == 0) return;
        if (cacheCommands->EnqueueBazaarOutboxList({p->GetSessionID(), characterID, request->requestID}) != Base::EnqueueResult::Rejected)
            return;
        MsgBazaarOutboxListResBody failed{};
        failed.requestID = request->requestID;
        failed.characterID = characterID;
        if (sessionManager->GetFieldCharacterID(p->GetSessionID()) != characterID) return;
        responses->Enqueue(p->GetSessionID(), writer->WriteBazaarOutboxListResponse(&failed));
    }

    void BazaarRequestHandler::BazaarCheckOutbox(IPacketView* p) {
        const auto request = ParseBazaarRequest<BazaarCheckOutboxRequestBody>(*p, OP::BAZAAR_CHECK_OUTBOX);
        if (!request)
            return;
        const uint64_t characterID = sessionManager->GetFieldCharacterID(p->GetSessionID());
        if (characterID == 0)
            return;
        if (cacheCommands->EnqueueBazaarCheckOutbox({p->GetSessionID(), characterID, request->requestID}) != Base::EnqueueResult::Rejected)
            return;
        MsgBazaarCheckOutboxResBody failed{};
        failed.requestID = request->requestID;
        if (sessionManager->GetFieldCharacterID(p->GetSessionID()) != characterID)
            return;
        responses->Enqueue(p->GetSessionID(), writer->WriteBazaarCheckOutboxResponse(&failed));
    }

    void BazaarRequestHandler::BazaarCancelListing(IPacketView* p) {
        const auto request = ParseBazaarRequest<BazaarCancelListingRequestBody>(*p, OP::BAZAAR_CANCEL_LISTING);
        if (!request || request->listingID == 0)
            return;
        const uint64_t characterID = sessionManager->GetFieldCharacterID(p->GetSessionID());
        if (characterID == 0) return;
        if (cacheCommands->EnqueueBazaarCancelListing({p->GetSessionID(), characterID, request->requestID}, request->listingID) != Base::EnqueueResult::Rejected)
            return;
        MsgBazaarCancelListingResBody failed{};
        failed.requestID = request->requestID;
        failed.listingID = request->listingID;
        failed.characterID = characterID;
        failed.resStatus = 5;
        if (sessionManager->GetFieldCharacterID(p->GetSessionID()) != characterID) return;
        responses->Enqueue(p->GetSessionID(), writer->WriteBazaarCancelListingResponse(&failed));
    }

    void BazaarRequestHandler::BazaarClaim(IPacketView* p) {
        const auto request = ParseBazaarRequest<BazaarClaimRequestBody>(*p, OP::BAZAAR_CLAIM);
        if (!request || request->listingID == 0)
            return;
        const uint64_t characterID = sessionManager->GetFieldCharacterID(p->GetSessionID());
        if (characterID == 0)
            return;
        if (cacheCommands->EnqueueBazaarClaim({p->GetSessionID(), characterID, request->requestID}, request->listingID) != Base::EnqueueResult::Rejected)
            return;
        MsgBazaarClaimResBody failed{};
        failed.requestID = request->requestID;
        failed.listingID = request->listingID;
        if (sessionManager->GetFieldCharacterID(p->GetSessionID()) != characterID)
            return;
        responses->Enqueue(p->GetSessionID(), writer->WriteBazaarClaimResponse(&failed));
    }

    void BazaarRequestHandler::BazaarMyList(IPacketView* p) {
        const auto request = ParseBazaarRequest<BazaarMyListRequestBody>(*p);
        if (!request)
            return;
        const uint64_t characterID = sessionManager->GetFieldCharacterID(p->GetSessionID());
        if (characterID == 0)
            return;
        if (cacheCommands->EnqueueBazaarMyList({p->GetSessionID(), characterID, request->requestID}) != Base::EnqueueResult::Rejected)
            return;
        MsgBazaarMyListResBody failed{};
        failed.requestID = request->requestID;
        failed.characterID = characterID;
        if (sessionManager->GetFieldCharacterID(p->GetSessionID()) != characterID)
            return;
        responses->Enqueue(p->GetSessionID(), writer->WriteBazaarMyListResponse(&failed));
    }

    void BazaarRequestHandler::BazaarSearch(IPacketView* p) {
        const auto request = ParseBazaarRequest<BazaarSearchRequestBody>(*p);
        if (!request || request->itemType < 1 || request->itemType > 3
            || !sessionManager->IsFieldSession(p->GetSessionID()))
            return;

        if (cacheCommands->EnqueueBazaarSearch(p->GetSessionID(), request->requestID, request->itemType, request->page) != Base::EnqueueResult::Rejected)
            return;
        MsgBazaarSearchResBody failed{};
        failed.requestID = request->requestID;
        responses->Enqueue(p->GetSessionID(), writer->WriteBazaarSearchResponse(&failed));
    }
}
