#pragma once
#include <tuple>
#include <memory>

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

    struct Key7 {
        uint64_t characterID;
        bool operator==(const Key7& other) const noexcept {
            return characterID == other.characterID;
        }
        bool operator<(const Key7& other) const noexcept {
            return characterID < other.characterID;
        }
    };
    struct KeyHash7 {
        std::size_t operator()(const Key7& key) const noexcept {
            return std::hash<uint64_t>()(key.characterID);
        }
    };

    using Result7 = CacheItem<CurrencyData>;

    class CacheStorageCurrency : public CacheStorage<Key7, Result7, KeyHash7> {
        using Key = Key7;
        using KeyHash = KeyHash7;
        using Result = Result7;
        CACHE_STATUS LoadFromDB(uint16_t shardIndex, Key& key);
    public:
        Result7 Getter(uint64_t characterID);
        std::tuple<CACHE_STATUS, uint64_t> DepositCurrency(uint64_t characterID, uint64_t deposit);
        std::tuple<CACHE_STATUS, uint64_t> TryWithdrawCurrency(uint64_t characterID, uint64_t withdraw);
        static std::unique_ptr<FlushCommand> GetFlushCommand(Key key, Result result);
    };
}
