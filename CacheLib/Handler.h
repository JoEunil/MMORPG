#pragma once

#include <cstdint>
#include <memory>
#include <utility>

#include <mysqlconn/include/mysql/jdbc.h>
#include "Config.h"
#include "CacheStorageInventory.h"
#include "CacheStorageCurrency.h"
#include "ProfileCache.h"
#include "DBWorker.h"
#include "DBConnectionGame.h"
#include "DBConnectionBazaar.h"
#include <CoreLib/MessagePool.h>

#include <CoreLib/LoggerGlobal.h>
namespace Core {
    class IMessageQueue;
    class ILogger;
    class Message;
}

namespace Cache {
    class Handler {
        using ScopedMessage = CacheMessagePool::ScopedMessage;
        Core::IMessageQueue* messageQ;  // response
        CacheMessagePool* messagePool = nullptr;
        CacheStorageInventory* cache_inventory = nullptr;
        CacheStorageCurrency* cache_currency = nullptr;
        ProfileCache* cache_profile = nullptr;
        Core::ILogger* logger = nullptr;
        DBWorker<DBConnectionGame>* dbWorkerGame = nullptr;
        DBWorker<DBConnectionBazaar>* dbWorkerBazaar = nullptr;
        template <typename Response, typename Work>
        void EnqueueBazaarQuery(ScopedMessage& msg, Core::MsgHeader header, const Response& failureBody,const char* queueError, Work work);

        template <typename Connection, typename Work>
        void EnqueueMessageWork(DBWorker<Connection>* worker, ScopedMessage& msg, Work&& work) {
            // DBWorker stores copyable callbacks, so their copies share one scoped handle.
            auto owned = std::make_shared<ScopedMessage>(std::move(msg));
            worker->Enqueue([owned, work = std::forward<Work>(work)](Connection* conn) {
                auto message = std::move(*owned);
                work(conn, message.get());
            });
        }

        template <typename T>
        bool CheckMessageLength(const Core::Message* msg) {
            return msg->GetLength() == sizeof(Core::MsgStruct<T>);
        }
        void Initialize(Core::IMessageQueue* mq,  CacheMessagePool* mp, DBWorker<DBConnectionGame>* dg, DBWorker<DBConnectionBazaar>* db, CacheStorageInventory* ci, CacheStorageCurrency* cc, ProfileCache* cp) {
            messageQ = mq;
            messagePool = mp;
            dbWorkerGame = dg;
            dbWorkerBazaar = db;
            cache_inventory = ci;
            cache_currency = cc;
            cache_profile = cp;
        }
        bool IsReady() {
            if (messageQ == nullptr) {
                Core::sysLogger->LogError("cache handler", "messageQ not initialized");
                return false;
            }
            if (messagePool == nullptr) {
                Core::sysLogger->LogError("cache handler", "messagePool not initialized");
                return false;
            }
            if (dbWorkerGame == nullptr) {
                Core::sysLogger->LogError("cache handler", "dbWorkerGame not initialized");
                return false;
            }
            if (dbWorkerBazaar == nullptr) {
                Core::sysLogger->LogError("cache handler", "dbWorkerBazaar not initialized");
                return false;
            }
            if (cache_inventory == nullptr) {
                Core::sysLogger->LogError("cache handler", "cache_inventory not initialized");
                return false;
            }
            if (cache_currency == nullptr) {
                Core::sysLogger->LogError("cache handler", "cache_currency not initialized");
                return false;
            }
            if (cache_profile == nullptr) {
                Core::sysLogger->LogError("cache handler", "cache_profile not initialized");
                return false;
            }
            return true;
        }
        void CharacterListRequest(ScopedMessage& msg, uint64_t sesionID, Core::MsgCharacterListReqBody* body);
        void CharacterStateRequest(ScopedMessage& msg, uint64_t sesionID, Core::MsgCharacterStateReqBody* body);
        void CharacterStateUpdate(ScopedMessage& msg, uint64_t sessionID, Core::MsgCharacterStateUpdateBody* body);
        void InventoryRequest(ScopedMessage& msg, uint64_t sesionID, Core::MsgInventoryReqBody* body);
        void InventoryUpdate(ScopedMessage& msg, uint64_t sesionID, Core::MsgInventoryUpdateBody* body);
        void CurrencyRequest(ScopedMessage& msg, uint64_t sesionID, Core::MsgCurrencyReqBody* body);
        void CurrencyDeposit(ScopedMessage& msg, uint64_t sesionID, Core::MsgCurrencyDepositBody* body);
        void DiamondRequest(ScopedMessage& msg, uint64_t sesionID, Core::MsgDiamondReqBody* body);
        void DiamondDeposit(ScopedMessage& msg, uint64_t sesionID, Core::MsgDiamondDepositBody* body);
        void BazaarMyList(ScopedMessage& msg, uint64_t sesionID, Core::MsgBazaarMyListBody* body);
        void BazaarSearch(ScopedMessage& msg, uint64_t sesionID, Core::MsgBazaarSearchBody* body);
        void BazaarRegister(ScopedMessage& msg, uint64_t sesionID, Core::MsgBazaarRegisterBody* body);
        void BazaarCancel(ScopedMessage& msg, uint64_t sesionID, Core::MsgBazaarCancelBody* body);
        void BazaarBuy(ScopedMessage& msg, uint64_t sesionID, Core::MsgBazaarBuyBody* body);
        void BazaarClaim(ScopedMessage& msg, uint64_t sesionID, Core::MsgBazaarClaimBody* body);
        void BazaarOutboxList(ScopedMessage& msg, uint64_t sessionID, Core::MsgBazaarOutboxListBody* body);
        void BazaarCheckOutbox(ScopedMessage& msg, uint64_t sessionID, Core::MsgBazaarCheckOutboxBody* body);
        void BazaarCancelListing(ScopedMessage& msg, uint64_t sessionID, Core::MsgBazaarCancelListingBody* body);
        void ProfileRename(ScopedMessage& msg, uint64_t sessionID, Core::MsgProfileRenameBody* body);

        friend class Initializer;
        friend class BazaarSearchFixture;
        friend class BazaarMyListFixture;
    public:
        void Process(ScopedMessage msg);
    };
}
