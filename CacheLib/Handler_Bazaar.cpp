#include "Handler.h"

#include <algorithm>
#include <memory>

#include <CoreLib/Message.h>
#include <CoreLib/MessageTypes.h>
#include <CoreLib/IMessageQueue.h>
#include <CoreLib/LoggerGlobal.h>
#include <CoreLib/ItemData.h>

#include "DBWorker.h"
#include "MessagePool.h"
#include "BazaarSearchResult.h"
#include "BazaarMyListResult.h"
#include "BazaarClaimResult.h"
#include "BazaarPurchaseResult.h"
#include "BazaarCancelResult.h"
#include "BazaarRegisterResult.h"

namespace Cache {
    void Handler::BazaarMyList(Core::Message*& msg, uint64_t sessionID, Core::MsgBazaarMyListBody* body) {
        const auto request = *body;
        auto* owned = msg;
        const auto finish = [this, owned, sessionID, request](const Core::MsgBazaarMyListResBody& response) {
            auto* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarMyListResBody>*>(owned->GetBuffer());
            st->header = { sessionID, Core::MSG_BAZAAR_MY_LIST_RES };
            st->body = response;
            st->body.requestID = request.requestID;
            st->body.characterID = request.characterID;
            owned->SetLength(sizeof(*st));
            if (!messageQ->EnqueueMessage(owned))
                Core::errorLogger->LogError("cache handler bazaar", "my list response queue rejected", "sessionID", sessionID);
            messagePool->Return(owned);
        };
        const auto failed = [finish] { finish(Core::MsgBazaarMyListResBody{}); };
        const auto outcome = dbWorkerBazaar->Enqueue([request, finish](DBConnectionBazaar* conn) {
            auto result = conn->ExecuteSelect(11, request.characterID, Core::MAX_BAZAAR_MY_LIST);
            Core::MsgBazaarMyListResBody response{};
            ParseBazaarMyListResult(result.get(), request.requestID, request.characterID, response);
            finish(response);
        }, nullptr, failed);
        if (outcome != DBEnqueueResult::Queued)
            failed();
        msg = nullptr; // Async work or synchronous rejection owns the single Return.
    }

    void Handler::BazaarSearch(Core::Message*& msg, uint64_t sessionID, Core::MsgBazaarSearchBody* body) {
        const auto request = *body;
        auto* owned = msg;
        const auto finish = [this, owned, sessionID, requestID = request.requestID](const Core::MsgBazaarSearchResBody& response) {
            auto* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarSearchResBody>*>(owned->GetBuffer());
            st->header = { sessionID, Core::MSG_BAZAAR_SEARCH_RES };
            st->body = response;
            st->body.requestID = requestID;
            owned->SetLength(sizeof(*st));
            if (!messageQ->EnqueueMessage(owned))
                Core::errorLogger->LogError("cache handler bazaar", "search response queue rejected", "sessionID", sessionID);
            messagePool->Return(owned);
        };
        const auto failed = [finish] { finish(Core::MsgBazaarSearchResBody{}); };
        const auto outcome = dbWorkerBazaar->Enqueue([request, finish](DBConnectionBazaar* conn) {
            const uint32_t offset = static_cast<uint32_t>(request.page) * Core::MAX_BAZAAR_SEARCH_RESULT;
            auto result = conn->ExecuteSelect(12, request.item_type, Core::MAX_BAZAAR_SEARCH_RESULT, offset);
            Core::MsgBazaarSearchResBody response{};
            ParseBazaarSearchResult(result.get(), request.requestID, response);
            finish(response);
        }, nullptr, failed);
        if (outcome != DBEnqueueResult::Queued)
            failed();
        msg = nullptr; // Queued work or the synchronous rejection path owns Return.
    }

    void Handler::BazaarRegister(Core::Message*& msg, uint64_t sessionID, Core::MsgBazaarRegisterBody* body) {
        const auto request = *body; // The response overwrites this buffer and the worker outlives this call.
        struct PendingResponse {
            Core::Message* message;
            MessagePool* pool;
            bool finished = false;
            PendingResponse(Core::Message* message, MessagePool* pool) : message(message), pool(pool) {}
            ~PendingResponse() { pool->Return(message); }
        };
        const auto pending = std::make_shared<PendingResponse>(msg, messagePool);
        msg = nullptr; // All paths now share one Return, including a throwing response queue.
        const auto finish = [this, pending, sessionID](uint8_t status) {
            if (pending->finished) return;
            pending->finished = true; // A worker failure callback cannot send a second response.
            auto* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarRegisterResBody>*>(pending->message->GetBuffer());
            st->header = { sessionID, Core::MSG_BAZAAR_REGISTER_RES };
            st->body = { status };
            pending->message->SetLength(sizeof(*st));
            if (!messageQ->EnqueueMessage(pending->message))
                Core::errorLogger->LogError("cache handler bazaar", "register response queue rejected", "sessionID", sessionID);
        };

        const auto item = Data::itemMap.find(request.itemID);
        constexpr uint64_t BAZAAR_FEE_MIN = 10;
        constexpr uint64_t BAZAAR_FEE_RATE = 10;
        const uint64_t fee = std::max(BAZAAR_FEE_MIN, request.price / BAZAAR_FEE_RATE);
        const auto reportUnknown = [request, fee] {
            Core::errorLogger->LogError("cache handler bazaar", "register result or compensation unknown; manual investigation required",
                "char_id", request.characterID, "item_id", request.itemID, "quantity", request.quantity, "fee", fee);
        };
        const auto restore = [this, request, fee] {
            const auto inventoryStatus = std::get<0>(cache_inventory->PartialUpdate(request.characterID, request.itemID, 2, request.quantity));
            const auto feeStatus = std::get<0>(cache_currency->DepositCurrency(request.characterID, fee));
            return inventoryStatus == CACHE_STATUS::AVAILABLE && feeStatus == CACHE_STATUS::AVAILABLE;
        };
        try {
            if (item == Data::itemMap.end() || request.quantity == 0) {
                finish(4);
                return;
            }
            const auto inventoryStatus = std::get<0>(cache_inventory->PartialUpdate(request.characterID, request.itemID, 2, -static_cast<int32_t>(request.quantity)));
            if (inventoryStatus != CACHE_STATUS::AVAILABLE) {
                finish(2);
                return;
            }
            const auto feeStatus = std::get<0>(cache_currency->TryWithdrawCurrency(request.characterID, fee));
            if (feeStatus != CACHE_STATUS::AVAILABLE) {
                // Gold was not withdrawn, so only restore the item. A failed/throwing restore is uncertain.
                const auto restored = std::get<0>(cache_inventory->PartialUpdate(request.characterID, request.itemID, 2, request.quantity));
                if (restored != CACHE_STATUS::AVAILABLE) reportUnknown();
                finish(restored == CACHE_STATUS::AVAILABLE ? 3 : 5);
                return;
            }
            const auto failed = [finish, reportUnknown] {
                // The worker may fail before or after SQL; neither permits automatic compensation.
                reportUnknown();
                finish(5);
            };
            const auto outcome = dbWorkerBazaar->Enqueue([request, itemType = item->second.itemType, finish, restore, reportUnknown](DBConnectionBazaar* conn) {
                const auto status = ResolveBazaarRegister([conn, request, itemType] {
                    return conn->ExecuteUpdate(13, request.itemID, request.characterID, itemType, request.quantity, request.price);
                }, restore, reportUnknown);
                finish(status);
            }, nullptr, failed);
            if (outcome != DBEnqueueResult::Queued) {
                // Rejection proves the worker never ran INSERT.
                finish(ResolveBazaarRegister([] { return 0; }, restore, reportUnknown));
            }
        }
        catch (...) {
            // A cache mutation/enqueue/response exception can follow a partial effect. Do not retry it.
            reportUnknown();
            finish(5);
        }
    }

    void Handler::BazaarCancel(Core::Message*& msg, uint64_t sessionID, Core::MsgBazaarCancelBody* body) {
        // Cancellation and its return event commit together in the Bazaar DB.
        // Inventory capacity/cache availability can delay delivery, but cannot lose the item.
        EnqueueMessageWork(dbWorkerBazaar, msg, [msg, sessionID, body, this](DBConnectionBazaar* conn) {
            uint64_t characterID = body->characterID;
            auto res = conn->ExecuteSelect(14, body->listingID, characterID);
            uint8_t result = 0;
            if (res && res->next()) {
                result = static_cast<uint8_t>(res->getUInt("result"));
                if (result == 1 && res->getUInt("delivery_pending") != 0) {
                    auto status = cache_inventory->DeliverItem(characterID, res->getUInt64("event_id"),
                        res->getUInt("item_id"), res->getUInt("quantity"));
                    if (status != CACHE_STATUS::AVAILABLE && status != CACHE_STATUS::DUPLICATED)
                        Core::gameLogger->LogInfo("cache handler bazaar", "cancel return pending in outbox", "char_id", characterID);
                }
            }
            auto* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarCancelResBody>*>(msg->GetBuffer());
            st->header.sessionID = sessionID;
            st->header.messageType = Core::MSG_BAZAAR_CANCEL_RES;
            st->body.resStatus = result == 1 ? 1 : 0;
            msg->SetLength(sizeof(Core::MsgStruct<Core::MsgBazaarCancelResBody>));
            messageQ->EnqueueMessage(msg);
            messagePool->Return(msg);
        });
    }

    void Handler::BazaarBuy(Core::Message*& msg, uint64_t sessionID, Core::MsgBazaarBuyBody* body) {
        const auto request = *body;
        auto* owned = msg;
        const auto finish = [this, owned, sessionID, request](const Core::MsgBazaarBuyResBody& response) {
            auto* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarBuyResBody>*>(owned->GetBuffer());
            st->header = { sessionID, Core::MSG_BAZAAR_BUY_RES };
            st->body = response;
            st->body.requestID = request.requestID;
            st->body.listingID = request.listingID;
            st->body.characterID = request.characterID;
            owned->SetLength(sizeof(*st));
            if (!messageQ->EnqueueMessage(owned))
                Core::errorLogger->LogError("cache handler bazaar", "buy response queue rejected", "sessionID", sessionID);
            messagePool->Return(owned);
        };
        const auto failed = [finish] {
            Core::MsgBazaarBuyResBody response{};
            response.resStatus = 5;
            finish(response);
        };
        const auto outcome = dbWorkerBazaar->Enqueue([request, finish](DBConnectionBazaar* conn) {
            auto result = conn->ExecuteSelect(16, request.listingID, request.characterID);
            Core::MsgBazaarBuyResBody response{};
            ParseBazaarBuyResult(result.get(), response);
            finish(response);
        }, nullptr, failed);
        if (outcome != DBEnqueueResult::Queued) failed();
        msg = nullptr;
    }

    void Handler::BazaarCancelListing(Core::Message*& msg, uint64_t sessionID, Core::MsgBazaarCancelListingBody* body) {
        const auto request = *body;
        auto* owned = msg;
        const auto finish = [this, owned, sessionID, request](const Core::MsgBazaarCancelListingResBody& response) {
            auto* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarCancelListingResBody>*>(owned->GetBuffer());
            st->header = { sessionID, Core::MSG_BAZAAR_CANCEL_LISTING_RES };
            st->body = response;
            st->body.requestID = request.requestID;
            st->body.listingID = request.listingID;
            st->body.characterID = request.characterID;
            owned->SetLength(sizeof(*st));
            if (!messageQ->EnqueueMessage(owned))
                Core::errorLogger->LogError("cache handler bazaar", "cancel response queue rejected", "sessionID", sessionID);
            messagePool->Return(owned);
        };
        const auto failed = [finish] {
            Core::MsgBazaarCancelListingResBody response{};
            response.resStatus = 5;
            finish(response);
        };
        const auto outcome = dbWorkerBazaar->Enqueue([this, request, finish](DBConnectionBazaar* conn) {
            auto result = conn->ExecuteSelect(14, request.listingID, request.characterID);
            Core::MsgBazaarCancelListingResBody response{};
            response.resStatus = 5;
            Cache::ParseBazaarCancelResult(result.get(), [this, characterID = request.characterID](uint64_t eventID, uint32_t itemID, uint32_t quantity) {
                return cache_inventory->DeliverItem(characterID, eventID, itemID, quantity);
            }, response);
            finish(response);
        }, nullptr, failed);
        if (outcome != DBEnqueueResult::Queued) failed();
        msg = nullptr;
    }

    void Handler::BazaarClaim(Core::Message*& msg, uint64_t sessionID, Core::MsgBazaarClaimBody* body) {
        const auto request = *body;
        auto* owned = msg;
        const auto finish = [this, owned, sessionID, request](const Core::MsgBazaarClaimResBody& response) {
            auto* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarClaimResBody>*>(owned->GetBuffer());
            st->header = { sessionID, Core::MSG_BAZAAR_CLAIM_RES };
            st->body = response;
            st->body.requestID = request.requestID;
            st->body.listingID = request.listingID;
            st->body.characterID = request.characterID;
            owned->SetLength(sizeof(*st));
            if (!messageQ->EnqueueMessage(owned))
                Core::errorLogger->LogError("cache handler bazaar", "claim response queue rejected", "sessionID", sessionID);
            messagePool->Return(owned);
        };
        const auto failed = [finish] { finish(Core::MsgBazaarClaimResBody{}); };
        const auto outcome = dbWorkerBazaar->Enqueue([request, finish](DBConnectionBazaar* conn) {
            auto result = conn->ExecuteSelect(17, request.listingID, request.characterID);
            Core::MsgBazaarClaimResBody response{};
            ParseBazaarClaimResult(result.get(), response);
            finish(response);
        }, nullptr, failed);
        if (outcome != DBEnqueueResult::Queued)
            failed();
        msg = nullptr; // Async work or synchronous rejection owns the single Return.
    }

    void Handler::BazaarCheckOutbox(Core::Message*& msg, uint64_t sessionID, Core::MsgBazaarCheckOutboxBody* body) {
        const auto request = *body;
        auto* owned = msg;
        const auto finish = [this, owned, sessionID, request](const Core::MsgBazaarCheckOutboxResBody& response) {
            auto* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarCheckOutboxResBody>*>(owned->GetBuffer());
            st->header = { sessionID, Core::MSG_BAZAAR_CHECK_OUTBOX_RES };
            st->body = response;
            st->body.requestID = request.requestID;
            st->body.characterID = request.characterID;
            owned->SetLength(sizeof(*st));
            if (!messageQ->EnqueueMessage(owned))
                Core::errorLogger->LogError("cache handler bazaar", "outbox response queue rejected", "sessionID", sessionID);
            messagePool->Return(owned);
        };
        const auto failed = [finish] { finish(Core::MsgBazaarCheckOutboxResBody{}); };
        const auto outcome = dbWorkerBazaar->Enqueue([this, request, finish](DBConnectionBazaar* conn) {
            auto result = conn->ExecuteSelect(18, request.characterID);
            Core::MsgBazaarCheckOutboxResBody response{};
            DeliverBazaarOutboxResult(result.get(), [this, characterID = request.characterID](uint64_t eventID, uint32_t itemID, uint32_t quantity) {
                const auto status = cache_inventory->DeliverItem(characterID, eventID, itemID, quantity);
                if (status == CACHE_STATUS::AVAILABLE) CrashPoint("DELIVER");
                return status;
            }, response);
            finish(response); // Cache effects and event IDs flush together through the existing CacheFlush path.
        }, nullptr, failed);
        if (outcome != DBEnqueueResult::Queued) failed();
        msg = nullptr;
    }


}
