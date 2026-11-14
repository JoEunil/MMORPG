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
    namespace {
        // A valid selection carries the server-owned item ID, which is always nonzero.
        struct BazaarRegistrationSelection {
            uint8_t failureStatus;
            uint32_t itemID = 0;
        };

        BazaarRegistrationSelection ValidateBazaarRegistration(
            const Core::MsgBazaarRegisterBody& request, const Result5& inventory) {
            if (request.inventorySlot >= MAX_INVENTORY || inventory.status != CACHE_STATUS::AVAILABLE)
                return { 0 };

            const auto& selected = inventory.data.items[request.inventorySlot];
            if (selected.itemID == 0 || selected.slot != request.inventorySlot || request.quantity == 0 || request.price == 0)
                return { 4 };
            if (selected.quantity < request.quantity)
                return { 2 };

            // Inventory stacks by item ID, so duplicate IDs make slot-based removal ambiguous.
            for (uint16_t slot = 0; slot < MAX_INVENTORY; ++slot) {
                if (slot != request.inventorySlot && inventory.data.items[slot].itemID == selected.itemID)
                    return { 4 };
            }
            return { 0, selected.itemID };
        }
    }

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

        auto request = *body; // The response overwrites this buffer and the worker outlives this call.
        const auto pending = std::make_shared<ScopedMessage>(std::move(msg));
        const auto finish = [this, pending, sessionID, request](uint8_t status) {
            auto owned = std::move(*pending);
            if (!owned) return;
            const Core::MsgBazaarRegisterResBody response{ status, request.requestID, request.characterID };
            if (!TryEnqueueResponse(*owned, { sessionID, Core::MSG_BAZAAR_REGISTER_RES }, response))
                Core::errorLogger->LogError("cache handler bazaar", "register response queue rejected", "sessionID", sessionID);
        };

        const auto selection = ValidateBazaarRegistration(request, cache_inventory->Getter(request.characterID));
        if (selection.itemID == 0) {
            finish(selection.failureStatus);
            return;
        }
        request.itemID = selection.itemID;
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
            const auto failure = item == Data::itemMap.end() ? std::optional<uint8_t>{ 4 } : DeductBazaarRegistrationCosts(request, fee);
            if (failure) {
                if (*failure == 5)
                    reportUnknown();
                finish(*failure);
                return;
            }
            const auto failed = [finish, reportUnknown] {
                // The worker may fail before or after SQL; neither permits automatic compensation.
                reportUnknown();
                finish(5);
            };
            const auto outcome = dbWorkerBazaar->Enqueue([request, itemType = item->second.itemType, finish, restore, reportUnknown](DBConnectionBazaar* conn) {
                const auto status = ResolveBazaarRegister([conn, request, itemType] {
                    return conn->ExecuteUpdate(StatementId::InsertBazaarListing, request.itemID, request.characterID, itemType, request.quantity, request.price);
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

    std::optional<uint8_t> Handler::DeductBazaarRegistrationCosts(const Core::MsgBazaarRegisterBody& request, uint64_t fee) {
        const auto inventoryStatus = std::get<0>(cache_inventory->PartialUpdate(
            request.characterID, request.itemID, 2, -static_cast<int32_t>(request.quantity)));
        if (inventoryStatus != CACHE_STATUS::AVAILABLE)
            return 2;

        const auto feeStatus = std::get<0>(cache_currency->TryWithdrawCurrency(request.characterID, fee));
        if (feeStatus == CACHE_STATUS::AVAILABLE)
            return std::nullopt;

        // The fee was never withdrawn, so restore only the item.
        const auto restored = std::get<0>(cache_inventory->PartialUpdate(
            request.characterID, request.itemID, 2, request.quantity));
        return restored == CACHE_STATUS::AVAILABLE ? 3 : 5;
    }

    void Handler::BazaarCancel(ScopedMessage& msg, uint64_t sessionID, Core::MsgBazaarCancelBody* body) {
        if (!CheckMessageLength<Core::MsgBazaarCancelBody>(msg.get()))
            return;

        // Cancellation and its return event commit together in the Bazaar DB.
        // Inventory capacity/cache availability can delay delivery, but cannot lose the item.
        const auto request = *body;
        EnqueueMessageWork(dbWorkerBazaar, msg, [this, sessionID, request](DBConnectionBazaar* conn, Core::Message* msg) {
            uint64_t characterID = request.characterID;
            auto res = conn->ExecuteSelect(StatementId::CancelBazaarListing, request.listingID, characterID);
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
                Core::errorLogger->LogError("cache handler bazaar", "buy response queue rejected", "sessionID", sessionID);
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
        }, nullptr, failed);
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
                Core::errorLogger->LogError("cache handler bazaar", "cancel response queue rejected", "sessionID", sessionID);
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
        }, nullptr, failed);
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
                Core::errorLogger->LogError("cache handler bazaar", "claim response queue rejected", "sessionID", sessionID);
        };
        const auto failed = [finish] { finish(Core::MsgBazaarClaimResBody{}); };
        const auto outcome = dbWorkerBazaar->Enqueue([request, finish](DBConnectionBazaar* conn) {
            auto result = conn->ExecuteSelect(StatementId::ClaimBazaarProceeds, request.listingID, request.characterID);
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
        const auto finish = [this, pending, sessionID, request](Core::MsgBazaarOutboxListResBody response) {
            auto owned = std::move(*pending);
            if (!owned) return;
            response.requestID = request.requestID;
            response.characterID = request.characterID;
            if (!TryEnqueueResponse(*owned, { sessionID, Core::MSG_BAZAAR_OUTBOX_LIST_RES }, response))
                Core::errorLogger->LogError("cache handler bazaar", "outbox list response queue rejected", "sessionID", sessionID);
        };
        const auto failed = [finish] { finish(Core::MsgBazaarOutboxListResBody{}); };
        const auto outcome = dbWorkerBazaar->Enqueue([this, request, finish](DBConnectionBazaar* conn) {
            auto result = conn->ExecuteSelect(StatementId::SelectBuyerOutbox, request.characterID);
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
        const auto finish = [this, pending, sessionID, request](Core::MsgBazaarCheckOutboxResBody response) {
            auto owned = std::move(*pending);
            if (!owned) return;
            response.requestID = request.requestID;
            response.characterID = request.characterID;
            if (!TryEnqueueResponse(*owned, { sessionID, Core::MSG_BAZAAR_CHECK_OUTBOX_RES }, response))
                Core::errorLogger->LogError("cache handler bazaar", "outbox response queue rejected", "sessionID", sessionID);
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
        }, nullptr, failed);
        if (outcome != DBEnqueueResult::Queued) failed();
    }


}
