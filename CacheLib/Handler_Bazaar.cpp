#include "pch.h"
#include "Handler.h"

#include <exception>
#include <memory>

#include <CoreLib/Interface/IMessage.h>
#include <CoreLib/Cache/MessageTypes.h>
#include <CoreLib/Interface/IMessageQueue.h>
#include <CoreLib/LoggerGlobal.h>

#include "DB/DBWorker.h"
#include "BazaarResult.h"
#include "BazaarRegistration.h"

namespace Cache {
    template <typename Response, typename Work>
    void Handler::EnqueueBazaarQuery(Core::MsgHeader header, const Response& failureBody, const char* queueError, Work work) {
        const auto finish = [this, header, queueError](const Response& response) {
            auto message = messagePool->AcquireScoped();
            if (!message) {
                Core::errorLogger->LogError("cache handler bazaar", "response message pool exhausted",
                    {{"sessionID", header.sessionID}, {"messageType", header.messageType}, {"requestID", response.requestID}});
                return;
            }

            bool queued = false;
            try {
                queued = TryEnqueueResponse(*message, header, response);
            }
            catch (const std::exception& e) {
                // The queue may already have accepted the response before throwing.
                Core::errorLogger->LogError("cache handler bazaar", "response enqueue threw",
                    {{"sessionID", header.sessionID}, {"requestID", response.requestID}, {"msg", e.what()}});
                return;
            }
            catch (...) {
                Core::errorLogger->LogError("cache handler bazaar", "response enqueue threw",
                    {{"sessionID", header.sessionID}, {"requestID", response.requestID}});
                return;
            }
            if (!queued)
                Core::errorLogger->LogError("cache handler bazaar", queueError, {{"sessionID", header.sessionID}});
        };
        const auto failed = [finish, failureBody] { finish(failureBody); };
        const auto outcome = dbWorkerBazaar->Enqueue([finish, work = std::move(work)](DBConnectionBazaar* conn) {
            finish(work(*conn));
        }, failed);
        if (outcome != DBEnqueueResult::Queued)
            failed();
    }

    void Handler::BazaarMyList(ScopedMessage& msg, uint64_t sessionID, Core::MsgBazaarMyListBody* body) {
        if (!CheckMessageLength<Core::MsgBazaarMyListBody>(msg.get()))
            return;

        const auto request = *body;
        msg.reset();
        Core::MsgBazaarMyListResBody failureBody{};
        failureBody.requestID = request.requestID;
        failureBody.characterID = request.characterID;

        EnqueueBazaarQuery({ sessionID, Core::MSG_BAZAAR_MY_LIST_RES }, failureBody,
            "my list response queue rejected", [request](DBConnectionBazaar& conn) {
                auto result = conn.ExecuteSelect(StatementId::SelectBazaarMyList, request.characterID, Core::MAX_BAZAAR_MY_LIST);
                Core::MsgBazaarMyListResBody response{};
                ParseBazaarMyListResult(result.get(), request.requestID, request.characterID, response);
                return response;
            });
    }

    void Handler::BazaarSearch(ScopedMessage& msg, uint64_t sessionID, Core::MsgBazaarSearchBody* body) {
        if (!CheckMessageLength<Core::MsgBazaarSearchBody>(msg.get()))
            return;

        const auto request = *body;
        msg.reset();
        Core::MsgBazaarSearchResBody failureBody{};
        failureBody.requestID = request.requestID;

        EnqueueBazaarQuery({ sessionID, Core::MSG_BAZAAR_SEARCH_RES }, failureBody,
            "search response queue rejected", [request](DBConnectionBazaar& conn) {
                const uint32_t offset = static_cast<uint32_t>(request.page) * Core::MAX_BAZAAR_SEARCH_RESULT;
                auto result = conn.ExecuteSelect(StatementId::SearchBazaar, request.item_type, Core::MAX_BAZAAR_SEARCH_RESULT, offset);
                Core::MsgBazaarSearchResBody response{};
                ParseBazaarSearchResult(result.get(), request.requestID, response);
                return response;
            });
    }

    void Handler::BazaarRegister(ScopedMessage& msg, uint64_t sessionID, Core::MsgBazaarRegisterBody* body) {
        if (!CheckMessageLength<Core::MsgBazaarRegisterBody>(msg.get()))
            return;

        const auto request = *body;
        const auto pending = std::make_shared<ScopedMessage>(std::move(msg));
        const auto finish = [this, pending, sessionID, requestID = request.requestID, characterID = request.characterID](BazaarRegistrationStatus status) {
            auto owned = std::move(*pending);
            if (!owned) return;
            const Core::MsgBazaarRegisterResBody response{static_cast<uint8_t>(status), requestID, characterID};
            if (!TryEnqueueResponse(*owned, {sessionID, Core::MSG_BAZAAR_REGISTER_RES}, response))
                Core::errorLogger->LogError("cache handler bazaar", "register response queue rejected", {{"sessionID", sessionID}});
        };
        bazaarRegistration->Register({request.characterID, request.quantity, request.price, request.inventorySlot}, finish);
    }

    void Handler::BazaarCancel(ScopedMessage& msg, uint64_t sessionID, Core::MsgBazaarCancelBody* body) {
        if (!CheckMessageLength<Core::MsgBazaarCancelBody>(msg.get()))
            return;

        // Cancellation and its return event commit together in the Bazaar DB.
        // Inventory capacity/cache availability can delay delivery, but cannot lose the item.
        const auto request = *body;
        EnqueueMessageWork(dbWorkerBazaar, msg, [this, sessionID, request](DBConnectionBazaar* conn, Core::IMessage* msg) {
            uint64_t characterID = request.characterID;
            auto res = conn->ExecuteSelect(StatementId::CancelBazaarListing, request.listingID, characterID);
            uint8_t result = 0;
            if (res && res->next()) {
                result = static_cast<uint8_t>(res->getUInt("result"));
                if (result == 1 && res->getUInt("delivery_pending") != 0) {
                    auto status = cache_inventory->DeliverItem(characterID, res->getUInt64("event_id"),
                        res->getUInt("item_id"), res->getUInt("quantity"));
                    if (status != CACHE_STATUS::AVAILABLE && status != CACHE_STATUS::DUPLICATED)
                        Core::gameLogger->LogInfo("cache handler bazaar", "cancel return pending in outbox", {{"char_id", characterID}});
                }
            }
            const Core::MsgBazaarCancelResBody response{ static_cast<uint8_t>(result == 1) };
            TryEnqueueResponse(*msg, { sessionID, Core::MSG_BAZAAR_CANCEL_RES }, response);
        });
    }

    void Handler::BazaarBuy(ScopedMessage& msg, uint64_t sessionID, Core::MsgBazaarBuyBody* body) {
        if (!CheckMessageLength<Core::MsgBazaarBuyBody>(msg.get()))
            return;

        const auto request = *body;
        auto pending = std::make_shared<ScopedMessage>(std::move(msg));
        const auto finish = [this, pending, sessionID, request](Core::MsgBazaarBuyResBody response) {
            auto owned = std::move(*pending);
            if (!owned) return;
            response.requestID = request.requestID;
            response.listingID = request.listingID;
            response.characterID = request.characterID;
            if (!TryEnqueueResponse(*owned, { sessionID, Core::MSG_BAZAAR_BUY_RES }, response))
                Core::errorLogger->LogError("cache handler bazaar", "buy response queue rejected", {{"sessionID", sessionID}});
        };
        const auto failed = [finish] {
            Core::MsgBazaarBuyResBody response{};
            response.resStatus = 5;
            finish(response);
        };
        const auto outcome = dbWorkerBazaar->Enqueue([request, finish](DBConnectionBazaar* conn) {
            auto result = conn->ExecuteSelect(StatementId::BuyBazaarListing, request.listingID, request.characterID);
            Core::MsgBazaarBuyResBody response{};
            ParseBazaarBuyResult(result.get(), response);
            finish(response);
        }, failed);
        if (outcome != DBEnqueueResult::Queued) failed();
    }

    void Handler::BazaarCancelListing(ScopedMessage& msg, uint64_t sessionID, Core::MsgBazaarCancelListingBody* body) {
        if (!CheckMessageLength<Core::MsgBazaarCancelListingBody>(msg.get()))
            return;

        const auto request = *body;
        auto pending = std::make_shared<ScopedMessage>(std::move(msg));
        const auto finish = [this, pending, sessionID, request](Core::MsgBazaarCancelListingResBody response) {
            auto owned = std::move(*pending);
            if (!owned) return;
            response.requestID = request.requestID;
            response.listingID = request.listingID;
            response.characterID = request.characterID;
            if (!TryEnqueueResponse(*owned, { sessionID, Core::MSG_BAZAAR_CANCEL_LISTING_RES }, response))
                Core::errorLogger->LogError("cache handler bazaar", "cancel response queue rejected", {{"sessionID", sessionID}});
        };
        const auto failed = [finish] {
            Core::MsgBazaarCancelListingResBody response{};
            response.resStatus = 5;
            finish(response);
        };
        const auto outcome = dbWorkerBazaar->Enqueue([this, request, finish](DBConnectionBazaar* conn) {
            auto result = conn->ExecuteSelect(StatementId::CancelBazaarListing, request.listingID, request.characterID);
            Core::MsgBazaarCancelListingResBody response{};
            response.resStatus = 5;
            Cache::ParseBazaarCancelResult(result.get(), [this, characterID = request.characterID](uint64_t eventID, uint32_t itemID, uint32_t quantity) {
                return cache_inventory->DeliverItem(characterID, eventID, itemID, quantity);
            }, response);
            finish(response);
        }, failed);
        if (outcome != DBEnqueueResult::Queued) failed();
    }

    void Handler::BazaarClaim(ScopedMessage& msg, uint64_t sessionID, Core::MsgBazaarClaimBody* body) {
        if (!CheckMessageLength<Core::MsgBazaarClaimBody>(msg.get()))
            return;

        const auto request = *body;
        auto pending = std::make_shared<ScopedMessage>(std::move(msg));
        const auto finish = [this, pending, sessionID, request](Core::MsgBazaarClaimResBody response) {
            auto owned = std::move(*pending);
            if (!owned) return;
            response.requestID = request.requestID;
            response.listingID = request.listingID;
            response.characterID = request.characterID;
            if (!TryEnqueueResponse(*owned, { sessionID, Core::MSG_BAZAAR_CLAIM_RES }, response))
                Core::errorLogger->LogError("cache handler bazaar", "claim response queue rejected", {{"sessionID", sessionID}});
        };
        const auto failed = [finish] { finish(Core::MsgBazaarClaimResBody{}); };
        const auto outcome = dbWorkerBazaar->Enqueue([request, finish](DBConnectionBazaar* conn) {
            auto result = conn->ExecuteSelect(StatementId::ClaimBazaarProceeds, request.listingID, request.characterID);
            Core::MsgBazaarClaimResBody response{};
            ParseBazaarClaimResult(result.get(), response);
            finish(response);
        }, failed);
        if (outcome != DBEnqueueResult::Queued)
            failed();
    }

    void Handler::BazaarOutboxList(ScopedMessage& msg, uint64_t sessionID, Core::MsgBazaarOutboxListBody* body) {
        if (!CheckMessageLength<Core::MsgBazaarOutboxListBody>(msg.get()))
            return;

        const auto request = *body;
        auto pending = std::make_shared<ScopedMessage>(std::move(msg));
        const auto finish = [this, pending, sessionID, request](Core::MsgBazaarOutboxListResBody response) {
            auto owned = std::move(*pending);
            if (!owned) return;
            response.requestID = request.requestID;
            response.characterID = request.characterID;
            if (!TryEnqueueResponse(*owned, { sessionID, Core::MSG_BAZAAR_OUTBOX_LIST_RES }, response))
                Core::errorLogger->LogError("cache handler bazaar", "outbox list response queue rejected", {{"sessionID", sessionID}});
        };
        const auto failed = [finish] { finish(Core::MsgBazaarOutboxListResBody{}); };
        const auto outcome = dbWorkerBazaar->Enqueue([this, request, finish](DBConnectionBazaar* conn) {
            auto result = conn->ExecuteSelect(StatementId::SelectBuyerOutbox, request.characterID);
            const auto inventory = cache_inventory->Getter(request.characterID);
            Core::MsgBazaarOutboxListResBody response{};
            ParseBazaarOutboxListResult(result.get(), inventory, response);
            finish(response);
        }, failed);
        if (outcome != DBEnqueueResult::Queued) failed();
    }

    void Handler::BazaarCheckOutbox(ScopedMessage& msg, uint64_t sessionID, Core::MsgBazaarCheckOutboxBody* body) {
        if (!CheckMessageLength<Core::MsgBazaarCheckOutboxBody>(msg.get()))
            return;

        const auto request = *body;
        auto pending = std::make_shared<ScopedMessage>(std::move(msg));
        const auto finish = [this, pending, sessionID, request](Core::MsgBazaarCheckOutboxResBody response) {
            auto owned = std::move(*pending);
            if (!owned) return;
            response.requestID = request.requestID;
            response.characterID = request.characterID;
            if (!TryEnqueueResponse(*owned, { sessionID, Core::MSG_BAZAAR_CHECK_OUTBOX_RES }, response))
                Core::errorLogger->LogError("cache handler bazaar", "outbox response queue rejected", {{"sessionID", sessionID}});
        };
        const auto failed = [finish] { finish(Core::MsgBazaarCheckOutboxResBody{}); };
        const auto outcome = dbWorkerBazaar->Enqueue([this, request, finish](DBConnectionBazaar* conn) {
            auto result = conn->ExecuteSelect(StatementId::SelectBuyerOutbox, request.characterID);
            Core::MsgBazaarCheckOutboxResBody response{};
            DeliverBazaarOutboxResult(result.get(), [this, characterID = request.characterID](uint64_t eventID, uint32_t itemID, uint32_t quantity) {
                const auto status = cache_inventory->DeliverItem(characterID, eventID, itemID, quantity);
                if (status == CACHE_STATUS::AVAILABLE) CrashPoint("DELIVER");
                return status;
            }, response);
            finish(response); // Cache effects and event IDs flush together through the existing CacheFlush path.
        }, failed);
        if (outcome != DBEnqueueResult::Queued) failed();
    }


}
