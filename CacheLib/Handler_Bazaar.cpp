#include "Handler.h"

#include <algorithm>
#include <exception>
#include <memory>

#include <CoreLib/Message.h>
#include <CoreLib/MessageTypes.h>
#include <CoreLib/IMessageQueue.h>
#include <CoreLib/LoggerGlobal.h>
#include <CoreLib/ItemData.h>

#include "DBWorker.h"
#include "BazaarSearchResult.h"
#include "BazaarMyListResult.h"
#include "BazaarClaimResult.h"
#include "BazaarPurchaseResult.h"
#include "BazaarCancelResult.h"
#include "BazaarRegisterResult.h"

namespace Cache {
    template <typename Response, typename Work>
    void Handler::EnqueueBazaarQuery(Core::MsgHeader header, const Response& failureBody, const char* queueError, Work work) {
        const auto finish = [this, header, queueError](const Response& response) {
            auto message = messagePool->AcquireScoped();
            if (!message) {
                Core::errorLogger->LogError("cache handler bazaar", "response message pool exhausted",
                    "sessionID", header.sessionID, "messageType", header.messageType, "requestID", response.requestID);
                return;
            }

            bool queued = false;
            try {
                queued = TryEnqueueResponse(*message, header, response);
            }
            catch (const std::exception& e) {
                // The queue may already have accepted the response before throwing.
                Core::errorLogger->LogError("cache handler bazaar", "response enqueue threw",
                    "sessionID", header.sessionID, "requestID", response.requestID, "msg", e.what());
                return;
            }
            catch (...) {
                Core::errorLogger->LogError("cache handler bazaar", "response enqueue threw",
                    "sessionID", header.sessionID, "requestID", response.requestID);
                return;
            }
            if (!queued)
                Core::errorLogger->LogError("cache handler bazaar", queueError, "sessionID", header.sessionID);
        };
        const auto failed = [finish, failureBody] { finish(failureBody); };
        const auto outcome = dbWorkerBazaar->Enqueue([finish, work = std::move(work)](DBConnectionBazaar* conn) {
            finish(work(*conn));
        }, nullptr, failed);
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
                auto result = conn.ExecuteSelect(11, request.characterID, Core::MAX_BAZAAR_MY_LIST);
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
                auto result = conn.ExecuteSelect(12, request.item_type, Core::MAX_BAZAAR_SEARCH_RESULT, offset);
                Core::MsgBazaarSearchResBody response{};
                ParseBazaarSearchResult(result.get(), request.requestID, response);
                return response;
            });
    }

    void Handler::BazaarRegister(ScopedMessage& msg, uint64_t sessionID, Core::MsgBazaarRegisterBody* body) {
        if (!CheckMessageLength<Core::MsgBazaarRegisterBody>(msg.get()))
            return;

        auto request = *body; // The response overwrites this buffer and the worker outlives this call.
        const auto pending = std::make_shared<ScopedMessage>(std::move(msg));
        const auto finish = [this, pending, sessionID, request](uint8_t status) {
            auto owned = std::move(*pending);
            if (!owned) return;
            auto* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarRegisterResBody>*>(owned->GetBuffer());
            st->header = { sessionID, Core::MSG_BAZAAR_REGISTER_RES };
            st->body = { status, request.requestID, request.characterID };
            owned->SetLength(sizeof(*st));
            if (!messageQ->EnqueueMessage(owned.get()))
                Core::errorLogger->LogError("cache handler bazaar", "register response queue rejected", "sessionID", sessionID);
        };

        const auto inventory = cache_inventory->Getter(request.characterID);
        if (request.inventorySlot >= MAX_INVENTORY || inventory.status != CACHE_STATUS::AVAILABLE) {
            finish(0);
            return;
        }
        const auto& selected = inventory.data.items[request.inventorySlot];
        if (selected.itemID == 0 || selected.slot != request.inventorySlot || request.quantity == 0 || request.price == 0) {
            finish(4);
            return;
        }
        request.itemID = selected.itemID;
        if (selected.quantity < request.quantity) {
            finish(2);
            return;
        }
        // Inventory insertion stacks by item ID, so duplicate IDs would make slot-based removal ambiguous.
        for (uint16_t slot = 0; slot < MAX_INVENTORY; ++slot) {
            if (slot != request.inventorySlot && inventory.data.items[slot].itemID == request.itemID) {
                finish(4);
                return;
            }
        }
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

    void Handler::BazaarCancel(ScopedMessage& msg, uint64_t sessionID, Core::MsgBazaarCancelBody* body) {
        if (!CheckMessageLength<Core::MsgBazaarCancelBody>(msg.get()))
            return;

        // Cancellation and its return event commit together in the Bazaar DB.
        // Inventory capacity/cache availability can delay delivery, but cannot lose the item.
        const auto request = *body;
        EnqueueMessageWork(dbWorkerBazaar, msg, [this, sessionID, request](DBConnectionBazaar* conn, Core::Message* msg) {
            uint64_t characterID = request.characterID;
            auto res = conn->ExecuteSelect(14, request.listingID, characterID);
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
        });
    }

    void Handler::BazaarBuy(ScopedMessage& msg, uint64_t sessionID, Core::MsgBazaarBuyBody* body) {
        if (!CheckMessageLength<Core::MsgBazaarBuyBody>(msg.get()))
            return;

        const auto request = *body;
        auto pending = std::make_shared<ScopedMessage>(std::move(msg));
        const auto finish = [this, pending, sessionID, request](const Core::MsgBazaarBuyResBody& response) {
            auto owned = std::move(*pending);
            if (!owned) return;
            auto* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarBuyResBody>*>(owned->GetBuffer());
            st->header = { sessionID, Core::MSG_BAZAAR_BUY_RES };
            st->body = response;
            st->body.requestID = request.requestID;
            st->body.listingID = request.listingID;
            st->body.characterID = request.characterID;
            owned->SetLength(sizeof(*st));
            if (!messageQ->EnqueueMessage(owned.get()))
                Core::errorLogger->LogError("cache handler bazaar", "buy response queue rejected", "sessionID", sessionID);
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
    }

    void Handler::BazaarCancelListing(ScopedMessage& msg, uint64_t sessionID, Core::MsgBazaarCancelListingBody* body) {
        if (!CheckMessageLength<Core::MsgBazaarCancelListingBody>(msg.get()))
            return;

        const auto request = *body;
        auto pending = std::make_shared<ScopedMessage>(std::move(msg));
        const auto finish = [this, pending, sessionID, request](const Core::MsgBazaarCancelListingResBody& response) {
            auto owned = std::move(*pending);
            if (!owned) return;
            auto* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarCancelListingResBody>*>(owned->GetBuffer());
            st->header = { sessionID, Core::MSG_BAZAAR_CANCEL_LISTING_RES };
            st->body = response;
            st->body.requestID = request.requestID;
            st->body.listingID = request.listingID;
            st->body.characterID = request.characterID;
            owned->SetLength(sizeof(*st));
            if (!messageQ->EnqueueMessage(owned.get()))
                Core::errorLogger->LogError("cache handler bazaar", "cancel response queue rejected", "sessionID", sessionID);
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
    }

    void Handler::BazaarClaim(ScopedMessage& msg, uint64_t sessionID, Core::MsgBazaarClaimBody* body) {
        if (!CheckMessageLength<Core::MsgBazaarClaimBody>(msg.get()))
            return;

        const auto request = *body;
        auto pending = std::make_shared<ScopedMessage>(std::move(msg));
        const auto finish = [this, pending, sessionID, request](const Core::MsgBazaarClaimResBody& response) {
            auto owned = std::move(*pending);
            if (!owned) return;
            auto* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarClaimResBody>*>(owned->GetBuffer());
            st->header = { sessionID, Core::MSG_BAZAAR_CLAIM_RES };
            st->body = response;
            st->body.requestID = request.requestID;
            st->body.listingID = request.listingID;
            st->body.characterID = request.characterID;
            owned->SetLength(sizeof(*st));
            if (!messageQ->EnqueueMessage(owned.get()))
                Core::errorLogger->LogError("cache handler bazaar", "claim response queue rejected", "sessionID", sessionID);
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
    }

    void Handler::BazaarOutboxList(ScopedMessage& msg, uint64_t sessionID, Core::MsgBazaarOutboxListBody* body) {
        if (!CheckMessageLength<Core::MsgBazaarOutboxListBody>(msg.get()))
            return;

        const auto request = *body;
        auto pending = std::make_shared<ScopedMessage>(std::move(msg));
        const auto finish = [this, pending, sessionID, request](const Core::MsgBazaarOutboxListResBody& response) {
            auto owned = std::move(*pending);
            if (!owned) return;
            auto* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarOutboxListResBody>*>(owned->GetBuffer());
            st->header = { sessionID, Core::MSG_BAZAAR_OUTBOX_LIST_RES };
            st->body = response;
            st->body.requestID = request.requestID;
            st->body.characterID = request.characterID;
            owned->SetLength(sizeof(*st));
            if (!messageQ->EnqueueMessage(owned.get()))
                Core::errorLogger->LogError("cache handler bazaar", "outbox list response queue rejected", "sessionID", sessionID);
        };
        const auto failed = [finish] { finish(Core::MsgBazaarOutboxListResBody{}); };
        const auto outcome = dbWorkerBazaar->Enqueue([this, request, finish](DBConnectionBazaar* conn) {
            auto result = conn->ExecuteSelect(18, request.characterID);
            const auto inventory = cache_inventory->Getter(request.characterID);
            Core::MsgBazaarOutboxListResBody response{};
            ParseBazaarOutboxListResult(result.get(), inventory, response);
            finish(response);
        }, nullptr, failed);
        if (outcome != DBEnqueueResult::Queued) failed();
    }

    void Handler::BazaarCheckOutbox(ScopedMessage& msg, uint64_t sessionID, Core::MsgBazaarCheckOutboxBody* body) {
        if (!CheckMessageLength<Core::MsgBazaarCheckOutboxBody>(msg.get()))
            return;

        const auto request = *body;
        auto pending = std::make_shared<ScopedMessage>(std::move(msg));
        const auto finish = [this, pending, sessionID, request](const Core::MsgBazaarCheckOutboxResBody& response) {
            auto owned = std::move(*pending);
            if (!owned) return;
            auto* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarCheckOutboxResBody>*>(owned->GetBuffer());
            st->header = { sessionID, Core::MSG_BAZAAR_CHECK_OUTBOX_RES };
            st->body = response;
            st->body.requestID = request.requestID;
            st->body.characterID = request.characterID;
            owned->SetLength(sizeof(*st));
            if (!messageQ->EnqueueMessage(owned.get()))
                Core::errorLogger->LogError("cache handler bazaar", "outbox response queue rejected", "sessionID", sessionID);
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
    }


}
