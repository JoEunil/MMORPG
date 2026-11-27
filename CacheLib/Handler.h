#pragma once

#include <cstdint>
#include <memory>
#include <utility>

#include "Config.h"
#include "CacheStorageInventory.h"
#include "CacheStorageCurrency.h"
#include "ProfileCache.h"
#include "DB/DBWorker.h"
#include "DB/DBConfig.h"
#include <CoreLib/Interface/IMessagePool.h>

#include <CoreLib/LoggerGlobal.h>
namespace Core {
    class ILogger;
    class IMessage;
}

namespace Cache {
    class BazaarHandler;
    class CacheResponseSender;

    class Handler {
        using ScopedMessage = Core::ScopedMessage;
        CacheResponseSender* responses = nullptr;
        CacheStorageInventory* cache_inventory = nullptr;
        CacheStorageCurrency* cache_currency = nullptr;
        ProfileCache* cache_profile = nullptr;
        Core::ILogger* logger = nullptr;
        DBWorker<DBConnectionGame>* dbWorkerGame = nullptr;
        DBWorker<DBConnectionBazaar>* dbWorkerBazaar = nullptr;
        BazaarHandler* bazaar = nullptr;

        template <typename T>
        bool CheckMessageLength(const Core::IMessage* msg) {
            return msg->GetLength() == sizeof(Core::MsgStruct<T>);
        }

        void Initialize(CacheResponseSender* sender, DBWorker<DBConnectionGame>* dg, DBWorker<DBConnectionBazaar>* db, CacheStorageInventory* ci, CacheStorageCurrency* cc, ProfileCache* cp, BazaarHandler* bazaarHandler) {
            responses = sender;
            dbWorkerGame = dg;
            dbWorkerBazaar = db;
            cache_inventory = ci;
            cache_currency = cc;
            cache_profile = cp;
            bazaar = bazaarHandler;
        }
        bool IsReady() {
            if (bazaar == nullptr) {
                Core::sysLogger->LogError("cache handler", "bazaar handler not initialized");
                return false;
            }
            if (responses == nullptr) {
                Core::sysLogger->LogError("cache handler", "response sender not initialized");
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
