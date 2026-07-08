#pragma once

#include <cstdint>

#include <mysqlconn/include/mysql/jdbc.h>
#include "CacheStorageInventory.h"
#include "CacheStorageCurrency.h"
#include "DBWorker.h"
#include "DBConnectionGame.h"
#include "DBConnectionBazaar.h"

#include <CoreLib/LoggerGlobal.h>
namespace Core {
    class IMessageQueue;
    class ILogger;
    class Message;
}

namespace Cache {
    class MessagePool;

    class Handler {
        Core::IMessageQueue* messageQ;  // response
        MessagePool* messagePool;
        CacheStorageInventory* cache_inventory;
        CacheStorageCurrency* cache_currency;
        Core::ILogger* logger;
        DBWorker<DBConnectionGame>* dbWorkerGame;
        DBWorker<DBConnectionBazaar>* dbWorkerBazaar;
        void Initialize(Core::IMessageQueue* mq,  MessagePool* mp, DBWorker<DBConnectionGame>* dg, DBWorker<DBConnectionBazaar>* db, CacheStorageInventory* ci, CacheStorageCurrency* cc) {
            messageQ = mq;
            messagePool = mp;
            dbWorkerGame = dg;
            dbWorkerBazaar = db;
            cache_inventory = ci;
            cache_currency = cc;
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
            return true;
        }
        void CharacterListRequest(Core::Message*& msg, uint64_t sesionID, Core::MsgCharacterListReqBody* body);
        void CharacterStateRequest(Core::Message*& msg, uint64_t sesionID, Core::MsgCharacterStateReqBody* body);
        void CharacterStateUpdate(Core::Message*& msg, uint64_t sessionID, Core::MsgCharacterStateUpdateBody* body);
        void InventoryRequest(Core::Message*& msg, uint64_t sesionID, Core::MsgInventoryReqBody* body);
        void InventoryUpdate(Core::Message*& msg, uint64_t sesionID, Core::MsgInventoryUpdateBody* body);
        void CurrencyRequest(Core::Message*& msg, uint64_t sesionID, Core::MsgCurrencyReqBody* body);
        void CurrencyDeposit(Core::Message*& msg, uint64_t sesionID, Core::MsgCurrencyDepositBody* body);
        void DiamondRequest(Core::Message*& msg, uint64_t sesionID, Core::MsgDiamondReqBody* body);
        void DiamondDeposit(Core::Message*& msg, uint64_t sesionID, Core::MsgDiamondDepositBody* body);
        void BazaarMyList(Core::Message*& msg, uint64_t sesionID, Core::MsgBazaarMyListBody* body);
        void BazaarSearch(Core::Message*& msg, uint64_t sesionID, Core::MsgBazaarSearchBody* body);
        void BazaarRegister(Core::Message*& msg, uint64_t sesionID, Core::MsgBazaarRegisterBody* body);
        void BazaarCancel(Core::Message*& msg, uint64_t sesionID, Core::MsgBazaarCancelBody* body);
        void BazaarBuy(Core::Message*& msg, uint64_t sesionID, Core::MsgBazaarBuyBody* body);
        void BazaarClaim(Core::Message*& msg, uint64_t sesionID, Core::MsgBazaarClaimBody* body);
        friend class Initializer;
    public:
        void Process(Core::Message* msg);
    };
}
