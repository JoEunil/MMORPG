#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <CoreLib/LoggerGlobal.h>
#include "../DB/DBConfig.h"

namespace Cache {
    class CacheStorageInventory;
    class CacheStorageCurrency;
    template<typename T> class DBWorker;

    struct BazaarRegistrationRequest {
        uint64_t characterID;
        uint16_t quantity;
        uint64_t price;
        uint8_t inventorySlot;
    };

    enum class BazaarRegistrationStatus : uint8_t {
        Failed = 0,
        Success = 1,
        InventoryRejected = 2,
        CurrencyRejected = 3,
        InvalidRequest = 4,
        Unknown = 5
    };

    class BazaarRegistration {
        CacheStorageInventory* inventory = nullptr;
        CacheStorageCurrency* currency = nullptr;
        DBWorker<DBConnectionBazaar>* worker = nullptr;

        void Initialize(CacheStorageInventory* items, CacheStorageCurrency* gold,
            DBWorker<DBConnectionBazaar>* dbWorker) {
            inventory = items;
            currency = gold;
            worker = dbWorker;
        }

        bool IsReady() const {
            if (inventory == nullptr) {
                Core::sysLogger->LogError("bazaar registration", "inventory not initialized");
                return false;
            }
            if (currency == nullptr) {
                Core::sysLogger->LogError("bazaar registration", "currency not initialized");
                return false;
            }
            if (worker == nullptr) {
                Core::sysLogger->LogError("bazaar registration", "DB worker not initialized");
                return false;
            }
            return true;
        }

        friend class Initializer;

        std::optional<BazaarRegistrationStatus> DeductCosts(
            const BazaarRegistrationRequest& request, uint32_t itemID, uint64_t fee);

    public:
        using Completion = std::function<void(BazaarRegistrationStatus)>;

        void Register(BazaarRegistrationRequest request, Completion completed);
    };
}
