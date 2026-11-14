#pragma once
#include <tuple>
#include <memory>
#include <array>
#include <vector>

#include <CoreLib/MessageTypes.h>
#include <CoreLib/Message.h>
#include <CoreLib/LoggerGlobal.h>
#include <mysqlconn/include/mysql/jdbc.h>

#include "CacheStorage.h"


namespace Cache {
    struct FlushCommand;
    struct CurrencyData {
        uint64_t gold;
    };

    struct CurrencyKey {
        uint64_t characterID;
        bool operator==(const CurrencyKey& other) const noexcept {
            return characterID == other.characterID;
        }
        bool operator<(const CurrencyKey& other) const noexcept {
            return characterID < other.characterID;
        }
    };
    struct CurrencyKeyHash {
        std::size_t operator()(const CurrencyKey& key) const noexcept {
            return std::hash<uint64_t>()(key.characterID);
        }
    };

    using CurrencyCacheItem = CacheItem<CurrencyData>;

    class CacheStorageCurrency : public CacheStorage<CurrencyKey, CurrencyCacheItem, CurrencyKeyHash> {
        using Key = CurrencyKey;
        using KeyHash = CurrencyKeyHash;
        using Result = CurrencyCacheItem;
        CACHE_STATUS LoadFromDB(uint16_t shardIndex, Key& key, bool claimed = false);
        struct PendingDeposit {
            uint64_t gold;
            std::function<void(CACHE_STATUS, uint64_t)> completed;
        };
        std::array<std::unordered_map<uint64_t, std::vector<PendingDeposit>>, SHARD_SIZE> pendingDeposits;
        std::tuple<CACHE_STATUS, uint64_t> DepositLocked(uint16_t index, Key key, uint64_t gold);
        void CompleteLoad(uint16_t index, Key key, const Result* loaded);
        friend class CurrencyRewardFixture;
    public:
        CurrencyCacheItem Getter(uint64_t characterID);
        void GrantCurrency(uint64_t characterID, uint64_t gold, std::function<void(CACHE_STATUS, uint64_t)> completed);
        std::tuple<CACHE_STATUS, uint64_t> DepositCurrency(uint64_t characterID, uint64_t deposit);
        std::tuple<CACHE_STATUS, uint64_t> TryWithdrawCurrency(uint64_t characterID, uint64_t withdraw);
        static std::unique_ptr<FlushCommand> GetFlushCommand(Key key, Result result);
    };
}
