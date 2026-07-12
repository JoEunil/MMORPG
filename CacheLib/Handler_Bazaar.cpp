#include "Handler.h"

#include <algorithm>

#include <CoreLib/Message.h>
#include <CoreLib/MessageTypes.h>
#include <CoreLib/IMessageQueue.h>
#include <CoreLib/LoggerGlobal.h>
#include <CoreLib/ItemData.h>

#include "DBWorker.h"
#include "MessagePool.h"

namespace Cache {
    void Handler::BazaarMyList(Core::Message*& msg, uint64_t sessionID, Core::MsgBazaarMyListBody * body) {
        dbWorkerBazaar->Enqueue([=](DBConnectionBazaar* conn) {
            auto res = conn->ExecuteSelect(11, body->characterID, Core::MAX_BAZAAR_MY_LIST);
            Core::MsgStruct<Core::MsgBazaarMyListResBody>* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarMyListResBody>*>(msg->GetBuffer());

            st->header.sessionID = sessionID;
            st->header.messageType = Core::MSG_BAZAAR_MY_LIST_RES;

            if (!res || !res->next()) {
                st->body.resStatus = 0;
                msg->SetLength(sizeof(Core::MsgStruct<Core::MsgBazaarMyListResBody>));
                messageQ->EnqueueMessage(msg);
				messagePool->Return(msg);
                Core::gameLogger->LogInfo("cache handler bazaar", "my list read failed", "sessionID", sessionID, "char_id", body->characterID);
                return;
            }

            st->body.resStatus = 1;
            st->body.count = 0;

            do {
                if (st->body.count >= Core::MAX_BAZAAR_MY_LIST) 
                    break;
                auto& listing = st->body.listings[st->body.count];
                listing.itemID = res->getUInt64("item_id");
                listing.listingID = res->getUInt64("listing_id");
                listing.price = res->getUInt64("price");
                listing.quantity = res->getInt("quantity");
                listing.registeredAt = res->getUInt64("listed_at");
                listing.sellerCharacterID = res->getUInt64("seller_id");
                listing.status = res->getUInt("status");

                st->body.count++;
            } while (res->next());

            msg->SetLength(sizeof(Core::MsgStruct<Core::MsgBazaarMyListResBody>));
            messageQ->EnqueueMessage(msg);
            messagePool->Return(msg);
            });
        msg = nullptr;
    }

    void Handler::BazaarSearch(Core::Message*& msg, uint64_t sessionID, Core::MsgBazaarSearchBody* body) {
        dbWorkerBazaar->Enqueue([=](DBConnectionBazaar* conn) {
            uint32_t offset = body->page * Core::MAX_BAZAAR_SEARCH_RESULT;
            auto res = conn->ExecuteSelect(12, body->item_type, Core::MAX_BAZAAR_SEARCH_RESULT, offset);
            Core::MsgStruct<Core::MsgBazaarSearchResBody>* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarSearchResBody>*>(msg->GetBuffer());

            st->header.sessionID = sessionID;
            st->header.messageType = Core::MSG_BAZAAR_SEARCH_RES;

            st->body.resStatus = 1;
            st->body.count = 0;

            if (!res || !res->next()) {
                msg->SetLength(sizeof(Core::MsgStruct<Core::MsgBazaarSearchResBody>));
                messageQ->EnqueueMessage(msg);
                messagePool->Return(msg);
                return;
            }

            do {
                if (st->body.count >= Core::MAX_BAZAAR_SEARCH_RESULT)
                    break;
                auto& listing = st->body.listings[st->body.count];
                listing.itemID = res->getUInt64("item_id");
                listing.listingID = res->getUInt64("listing_id");
                listing.price = res->getUInt64("price");
                listing.quantity = res->getInt("quantity");
                listing.registeredAt = res->getUInt64("listed_at");
                listing.sellerCharacterID = res->getUInt64("seller_id");
                listing.status = 0;

                st->body.count++;
            } while (res->next());

            msg->SetLength(sizeof(Core::MsgStruct<Core::MsgBazaarSearchResBody>));
            messageQ->EnqueueMessage(msg);
            messagePool->Return(msg);
            });
        msg = nullptr;
    }

    void Handler::BazaarRegister(Core::Message*& msg, uint64_t sessionID, Core::MsgBazaarRegisterBody* body) {
        auto it = Data::itemMap.find(body->itemID);
        if (it == Data::itemMap.end()) {
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
        auto [inventoryStatus, itemID, slot, quantity] = cache_inventory->PartialUpdate(body->characterID, body->itemID, 2, -static_cast<int16_t>(body->quantity));
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
        dbWorkerBazaar->Enqueue([=](DBConnectionBazaar* conn) {
            int affected = conn->ExecuteUpdate(13, body->itemID, body->characterID, itemType, body->quantity, body->price);

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
            });
        msg = nullptr;
    }

    void Handler::BazaarCancel(Core::Message*& msg, uint64_t sessionID, Core::MsgBazaarCancelBody* body) {
		// 1. bazaar에서 listing status 변경 (CANCELLED) CAS 방식으로 처리
        dbWorkerBazaar->Enqueue([=](DBConnectionBazaar* conn) {
            int affected = conn->ExecuteUpdate(14, body->listingID, body->characterID);
            if (affected == 1) {
                // 2. inventory에 아이템 추가 (롤백)
                auto res = conn->ExecuteSelect(15, body->listingID);
                if (res && res->next()) {
                    uint64_t realItemID = res->getUInt64("item_id");
                    uint16_t realQty = res->getUInt("quantity");

                    auto [status, itemID, slot, quantity] = cache_inventory->PartialUpdate(body->characterID, realItemID, 2, (int16_t)realQty);

                    if (status != CACHE_STATUS::AVAILABLE) {
                        // 이 경로는 캐시 설정이 올바른 경우 도달 불가.
                        // 도달 시 Config의 LRU 크기 재검토 필요.
                        Core::gameLogger->LogInfo("cache handler bazaar", "rollback invenoty failed after cancel bazaar", "listing_id", body->listingID, "item_id", itemID, "quantity", quantity);
                    }
                }
                else {
                    Core::gameLogger->LogError("cache handler bazaar", "listing select failed after cancel", "listing_id", body->listingID);
                }
            }
            // 3. 응답. (1: 성공, 0: 실패(listing이 이미 거래된 경우)) 
            Core::MsgStruct<Core::MsgBazaarCancelResBody>* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarCancelResBody>*>(msg->GetBuffer());
            st->header.sessionID = sessionID;
            st->header.messageType = Core::MSG_BAZAAR_CANCEL_RES;
            st->body.resStatus = affected == 1 ? 1 : 0;
            msg->SetLength(sizeof(Core::MsgStruct<Core::MsgBazaarCancelResBody>));
            messageQ->EnqueueMessage(msg);
            messagePool->Return(msg);
            return;
            });
		msg = nullptr;
    }

    void Handler::BazaarBuy(Core::Message*& msg, uint64_t sessionID, Core::MsgBazaarBuyBody* body) {
        // 1. DB에서 itemID 조회 
        dbWorkerBazaar->Enqueue([=](DBConnectionBazaar* conn) {

			auto listingRes = conn->ExecuteSelect(15, body->listingID);

            if (!listingRes || !listingRes->next()) {
                Core::MsgStruct<Core::MsgBazaarBuyResBody>* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarBuyResBody>*>(msg->GetBuffer());
                st->header.sessionID = sessionID;
                st->header.messageType = Core::MSG_BAZAAR_BUY_RES;
                st->body.resStatus = 2;
                msg->SetLength(sizeof(Core::MsgStruct<Core::MsgBazaarBuyResBody>));
                messageQ->EnqueueMessage(msg);
                messagePool->Return(msg);
                return;
            }
            uint64_t realItemID = listingRes->getUInt64("item_id");
            uint16_t realQty = listingRes->getUInt("quantity");

            // 2. bazzar에서 listing status 변경 (SOLD)
            // bazaar_log에 거래 기록 추가 
            // buyer_outbox에 아이템 추가
            // 구매자 다이아 차감
            dbWorkerBazaar->Enqueue([=](DBConnectionBazaar* conn) {
				auto res = conn->ExecuteSelect(16, body->listingID, body->characterID);

                if (!res || !res->next()) {
                    Core::MsgStruct<Core::MsgBazaarBuyResBody>* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarBuyResBody>*>(msg->GetBuffer());
                    st->header.sessionID = sessionID;
                    st->header.messageType = Core::MSG_BAZAAR_BUY_RES;
                    st->body.resStatus = 3;
                    msg->SetLength(sizeof(Core::MsgStruct<Core::MsgBazaarBuyResBody>));
                    messageQ->EnqueueMessage(msg);
                    messagePool->Return(msg);
                    return;
                }
                uint8_t resultCode = res->getUInt("result");
                uint64_t listingID = body->listingID;
                uint32_t itemID = res->getUInt("item_id");
				uint16_t quantity = res->getUInt("quantity");
                uint32_t diamondSpent = res->getUInt("price");
                // 3. 응답
                Core::MsgStruct<Core::MsgBazaarBuyResBody>* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarBuyResBody>*>(msg->GetBuffer());
                st->header.sessionID = sessionID;
                st->header.messageType = Core::MSG_BAZAAR_BUY_RES;
                st->body.resStatus = resultCode;
                st->body.listingID = listingID;
                st->body.itemID = itemID;
                st->body.quantity = quantity;
                st->body.diamondSpent = diamondSpent;
                msg->SetLength(sizeof(Core::MsgStruct<Core::MsgBazaarBuyResBody>));
                messageQ->EnqueueMessage(msg);
                messagePool->Return(msg);
                });
            });

        msg = nullptr;
    }

    void Handler::BazaarClaim(Core::Message*& msg, uint64_t sessionID, Core::MsgBazaarClaimBody* body) {
        // listing status가 sold인 listingID에 대해서 판매자가 claim 요청
		// 판매자에게 diamond 지급, status 'CLAIMED'로 변경
        dbWorkerBazaar->Enqueue([=](DBConnectionBazaar* conn) {
            auto res = conn->ExecuteSelect(17, body->listingID, body->characterID);
            if (!res || !res->next()) {
                Core::MsgStruct<Core::MsgBazaarClaimResBody>* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarClaimResBody>*>(msg->GetBuffer());
                st->header.sessionID = sessionID;
                st->header.messageType = Core::MSG_BAZAAR_CLAIM_RES;
                st->body.resStatus = 0;
                msg->SetLength(sizeof(Core::MsgStruct<Core::MsgBazaarClaimResBody>));
                messageQ->EnqueueMessage(msg);
                messagePool->Return(msg);
                return;
            }

            uint8_t resultCode = res->getUInt("result");
            uint32_t claimed = res->getUInt("diamond");

            Core::MsgStruct<Core::MsgBazaarClaimResBody>* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarClaimResBody>*>(msg->GetBuffer());
            st->header.sessionID = sessionID;
            st->header.messageType = Core::MSG_BAZAAR_CLAIM_RES;
            st->body.resStatus = resultCode;
            st->body.diamondClaimed = claimed;
            msg->SetLength(sizeof(Core::MsgStruct<Core::MsgBazaarClaimResBody>));
            messageQ->EnqueueMessage(msg);
            messagePool->Return(msg);
            });
		msg = nullptr;
    }

    void Handler::BazaarCheckOutbox(Core::Message*& msg, uint64_t sessionID, Core::MsgBazaarCheckOutboxBody* body) {
        // buyer_outbox의 READY event를 인벤토리로 배송 (Outbox → Inbox).
        // CLAIMED 전환은 여기서 하지 않는다 — 인벤토리 blob이 flush로 durable해진 뒤 CacheFlush가 수행.
        dbWorkerBazaar->Enqueue([=](DBConnectionBazaar* conn) {
            // body는 msg 버퍼 내부를 가리키므로, 응답(st)이 같은 버퍼를 덮어쓰기 전에 먼저 읽어야 함
            uint64_t characterID = body->characterID;

            Core::MsgStruct<Core::MsgBazaarCheckOutboxResBody>* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarCheckOutboxResBody>*>(msg->GetBuffer());
            st->header.sessionID = sessionID;
            st->header.messageType = Core::MSG_BAZAAR_CHECK_OUTBOX_RES;
            st->body.resStatus = 1;
            st->body.deliveredCount = 0;
            st->body.duplicatedCount = 0;
            st->body.blockedCount = 0;

            auto res = conn->ExecuteSelect(18, characterID);
            if (!res) {
                st->body.resStatus = 0; // select 실패 — 재시도 필요
                Core::errorLogger->LogError("cache handler bazaar", "check outbox select failed", "char_id", characterID);
            }
            if (res) {
                bool stop = false;
                while (!stop && res->next()) {
                    uint64_t eventID = res->getUInt64("event_id");
                    uint32_t itemID = res->getUInt("item_id");
                    uint32_t quantity = res->getUInt("quantity");

                    switch (cache_inventory->DeliverItem(characterID, eventID, itemID, quantity)) {
                    case CACHE_STATUS::AVAILABLE:
                        st->body.deliveredCount++;
                        break;
                    case CACHE_STATUS::DUPLICATED:
                        st->body.duplicatedCount++; // 이미 배송됨 — flush가 outbox를 CLAIMED로 수렴시킴
                        break;
                    case CACHE_STATUS::BLOCKED:
                        // 인벤토리/ring 가득 — 배송 순서 유지를 위해 이후 event도 보류 (outbox READY 유지)
                        st->body.blockedCount++;
                        stop = true;
                        break;
                    default:
                        // DB_READING / EVICTING / EMPTY — 캐시 미적재, 재시도 필요
                        st->body.resStatus = 0;
                        stop = true;
                        break;
                    }
                }
            }

            msg->SetLength(sizeof(Core::MsgStruct<Core::MsgBazaarCheckOutboxResBody>));
            messageQ->EnqueueMessage(msg);
            messagePool->Return(msg);
            });
        msg = nullptr;
    }


}