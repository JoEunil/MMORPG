#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "Config.h"
#include "CacheStorageInventory.h"
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
    class CurrencyHandler;
    class DiamondHandler;
    class CacheResponseSender;

    class Handler {
        using ScopedMessage = Core::ScopedMessage;
        CacheResponseSender* responses = nullptr;
        CacheStorageInventory* cache_inventory = nullptr;
        ProfileCache* cache_profile = nullptr;
        Core::ILogger* logger = nullptr;
        DBWorker<DBConnectionGame>* dbWorkerGame = nullptr;
        BazaarHandler* bazaar = nullptr;
        CurrencyHandler* currency = nullptr;
        DiamondHandler* diamond = nullptr;

        template <typename T>
        bool CheckMessageLength(const Core::IMessage* msg) {
            return msg->GetLength() == sizeof(Core::MsgStruct<T>);
        }

        template<typename Body>
        std::optional<Body> ReadRequest(ScopedMessage& msg) {
            if (!msg || !CheckMessageLength<Body>(msg.get()))
                return std::nullopt;
            const auto request = *Core::parseMsgBody<Body>(msg->GetBuffer());
            msg.reset();
            return request;
        }

        void Initialize(CacheResponseSender* sender, DBWorker<DBConnectionGame>* dg, CacheStorageInventory* ci, ProfileCache* cp, BazaarHandler* bazaarHandler, CurrencyHandler* currencyHandler, DiamondHandler* diamondHandler) {
            responses = sender;
            dbWorkerGame = dg;
            cache_inventory = ci;
            cache_profile = cp;
            bazaar = bazaarHandler;
            currency = currencyHandler;
            diamond = diamondHandler;
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
            if (diamond == nullptr) {
                Core::sysLogger->LogError("cache handler", "diamond handler not initialized");
                return false;
            }
            if (cache_inventory == nullptr) {
                Core::sysLogger->LogError("cache handler", "cache_inventory not initialized");
                return false;
            }
            if (currency == nullptr) {
                Core::sysLogger->LogError("cache handler", "currency handler not initialized");
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
        void ProfileRename(ScopedMessage& msg, uint64_t sessionID, Core::MsgProfileRenameBody* body);

        friend class Initializer;
        friend class BazaarSearchFixture;
        friend class BazaarMyListFixture;
    public:
        void Process(ScopedMessage msg);
    };
}
