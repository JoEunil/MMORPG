#include "Handler.h"

#include <algorithm>

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
        auto it = Data::itemMap.find(body->itemID);
        if (it == Data::itemMap.end() || body->quantity == 0) {
            Core::MsgStruct<Core::MsgBazaarRegisterResBody>* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarRegisterResBody>*>(msg->GetBuffer());
            st->header.sessionID = sessionID;
            st->header.messageType = Core::MSG_BAZAAR_REGISTER_RES;
            st->body.resStatus = 4; // invalid item
            msg->SetLength(sizeof(Core::MsgStruct<Core::MsgBazaarRegisterResBody>));
            messageQ->EnqueueMessage(msg);
            return;
		}

		uint16_t itemType = it->second.itemType;

        // 1. 아이템 Iventory에서 제거
        auto [inventoryStatus, itemID, slot, quantity] = cache_inventory->PartialUpdate(body->characterID, body->itemID, 2, -static_cast<int32_t>(body->quantity));
        if (inventoryStatus != CACHE_STATUS::AVAILABLE) {
            Core::MsgStruct<Core::MsgBazaarRegisterResBody>* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarRegisterResBody>*>(msg->GetBuffer());
            st->header.sessionID = sessionID;
            st->header.messageType = Core::MSG_BAZAAR_REGISTER_RES;
            st->body.resStatus = 2; 
            msg->SetLength(sizeof(Core::MsgStruct<Core::MsgBazaarRegisterResBody>));
            messageQ->EnqueueMessage(msg);
            return;
		}

        // 2. 골드 차감

        constexpr uint64_t BAZAAR_FEE_MIN    = 10;
        constexpr uint64_t BAZAAR_FEE_RATE = 10;  // 10분의 1
        uint64_t fee = std::max(BAZAAR_FEE_MIN, body->price / BAZAAR_FEE_RATE);

        auto [feeStatus, gold] =  cache_currency->TryWithdrawCurrency(body->characterID, fee);

        if (feeStatus != CACHE_STATUS::AVAILABLE) {
            // inventory 롤백
            auto [inventoryStatus, itemID, slot, quantity] = cache_inventory->PartialUpdate(body->characterID, body->itemID, 2, body->quantity);
            if (inventoryStatus != CACHE_STATUS::AVAILABLE) {
				Core::gameLogger->LogError("cache handler bazaar", "rollback failed after fee withdraw failed", "sessionID", sessionID, "char_id", body->characterID, "item_id", body->itemID, "quantity", body->quantity);
                // 골드는 클라이언트에서 1차 체크 해야됨. 
            }
            Core::MsgStruct<Core::MsgBazaarRegisterResBody>* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarRegisterResBody>*>(msg->GetBuffer());
            st->header.sessionID = sessionID;
            st->header.messageType = Core::MSG_BAZAAR_REGISTER_RES;
            st->body.resStatus = 3;
            msg->SetLength(sizeof(Core::MsgStruct<Core::MsgBazaarRegisterResBody>));
            messageQ->EnqueueMessage(msg);
            return;
        }

        // 3. bazaar에 등록 
        if (!EnqueueMessageWork(dbWorkerBazaar, msg, [msg, sessionID, body, this, itemType, fee](DBConnectionBazaar* conn) {
            int affected = 0;
            try {
                affected = conn->ExecuteUpdate(13, body->itemID, body->characterID, itemType, body->quantity, body->price);
            }
            catch (const sql::SQLException& e) {
                Core::errorLogger->LogError("cache handler bazaar", "register query failed", "msg", e.what());
            }

            // 4. 실패 시 롤백
            if (affected != 1) {
                // inventory 롤백
                auto [inventoryStatusL, itemID, slot, quantity] = cache_inventory->PartialUpdate(body->characterID, body->itemID, 2, body->quantity);
                if (inventoryStatusL != CACHE_STATUS::AVAILABLE) {
                    Core::gameLogger->LogError("cache handler bazaar", "rollback inventory failed after register bazaar failed", "sessionID", sessionID, "char_id", body->characterID, "item_id", body->itemID, "quantity", body->quantity);
                }
                // 골드 롤백
                auto [feeStatusL, gold] = cache_currency->DepositCurrency(body->characterID, fee);
                if (feeStatusL != CACHE_STATUS::AVAILABLE) {
                    Core::gameLogger->LogError("cache handler bazaar", "rollback gold failed after register bazaar failed", "sessionID", sessionID, "char_id", body->characterID, "fee", fee);
				}
            }
            // 5. 성공, 실패 응답
            Core::MsgStruct<Core::MsgBazaarRegisterResBody>* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarRegisterResBody>*>(msg->GetBuffer());
            st->header.sessionID = sessionID;
            st->header.messageType = Core::MSG_BAZAAR_REGISTER_RES;
            st->body.resStatus = affected == 1 ? 1 : 0;
            msg->SetLength(sizeof(Core::MsgStruct<Core::MsgBazaarRegisterResBody>));
            messageQ->EnqueueMessage(msg);
            messagePool->Return(msg);
            return;
            })) {
            auto [inventoryStatusL, itemID, slot, quantity] = cache_inventory->PartialUpdate(body->characterID, body->itemID, 2, body->quantity);
            if (inventoryStatusL != CACHE_STATUS::AVAILABLE)
                Core::gameLogger->LogError("cache handler bazaar", "rollback inventory failed after register enqueue failed", "sessionID", sessionID, "char_id", body->characterID, "item_id", body->itemID, "quantity", body->quantity);

            auto [feeStatusL, gold] = cache_currency->DepositCurrency(body->characterID, fee);
            if (feeStatusL != CACHE_STATUS::AVAILABLE)
                Core::gameLogger->LogError("cache handler bazaar", "rollback gold failed after register enqueue failed", "sessionID", sessionID, "char_id", body->characterID, "fee", fee);

            Core::MsgStruct<Core::MsgBazaarRegisterResBody>* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarRegisterResBody>*>(msg->GetBuffer());
            st->header.sessionID = sessionID;
            st->header.messageType = Core::MSG_BAZAAR_REGISTER_RES;
            st->body.resStatus = 0;
            msg->SetLength(sizeof(Core::MsgStruct<Core::MsgBazaarRegisterResBody>));
            messageQ->EnqueueMessage(msg);
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
