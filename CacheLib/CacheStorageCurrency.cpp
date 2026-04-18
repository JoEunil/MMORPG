#include "CacheStorageCurrency.h"

#include <mysqlconn/include/mysql/jdbc.h>
#include <CoreLib/Message.h>
#include "DBWorker.h"
#include "CacheFlush.h"

namespace Cache {
    CACHE_STATUS CacheStorageCurrency::LoadFromDB(uint16_t shardIndex, Key& key) {
        // status 변경 -> 원자성 보장
        auto status = TrySetReading(shardIndex, key);
        if (status != CACHE_STATUS::EMPTY) // READING
            return status;

        dbWorker->Enqueue([=](DBConnectionGame* conn) {
            Result result;
            auto res = conn->ExecuteSelect(7, key.characterID);
            if (!res || !res->next()) {
                Core::gameLogger->LogInfo("cache storage currency", "currency not found in DB", "char_id", key.characterID);
                SetEmpty(shardIndex, key);  // READING → EMPTY
                return CACHE_STATUS::EMPTY;
            }
            result.data.gold = static_cast<uint64_t>(res->getUInt64("gold"));
            result.rollbackCnt = 0;
            Insert(shardIndex, key, result);  // READING → AVAILABLE
            });
        return CACHE_STATUS::DB_READING;
    }

    Result7 CacheStorageCurrency::Getter(uint64_t characterID) {
        Key key;
        Result result;
        key.characterID = characterID;
        auto shardIndex = characterID & SHARD_SIZE_MASK;

        switch (TryGet(key.characterID & SHARD_SIZE_MASK, key, result))
        {
        case CACHE_STATUS::AVAILABLE:
            break;
        case CACHE_STATUS::DB_READING:
            result.status = CACHE_STATUS::DB_READING;
            return result;
        case CACHE_STATUS::EVICTING:
            result.status = CACHE_STATUS::EVICTING;
            return result;
        case CACHE_STATUS::EMPTY: {
            if (LoadFromDB(shardIndex, key) != CACHE_STATUS::AVAILABLE) {
                result.status = CACHE_STATUS::EMPTY;
                return result;
            }
            break;
        }
        default: break;
        }
        return result;
    }

    // res, gold
    std::tuple<CACHE_STATUS, uint64_t> CacheStorageCurrency::DepositCurrency(uint64_t characterID, uint64_t deposit) {

        Key key;
        Result result;
        key.characterID = characterID;
        uint16_t shardIndex = key.characterID & SHARD_SIZE_MASK;


        switch (TryGet(key.characterID & SHARD_SIZE_MASK, key, result))
        {
        case CACHE_STATUS::AVAILABLE:
            break;
        case CACHE_STATUS::DB_READING:
            return { CACHE_STATUS::DB_READING, 0 };
        case CACHE_STATUS::EVICTING:
            return { CACHE_STATUS::EVICTING, 0};
        case CACHE_STATUS::EMPTY: {
            if (LoadFromDB(shardIndex, key) != CACHE_STATUS::AVAILABLE)
                return { CACHE_STATUS::EMPTY, 0 };
            break;
            }
        }
        auto& shard = m_shards[shardIndex];
        std::unique_lock<std::mutex> lock(shard.mutex);
        auto& res = shard.cache_data[key];

        if (res.status != CACHE_STATUS::AVAILABLE)
            return { res.status, 0};

        CACHE_STATUS resStatus = CACHE_STATUS::AVAILABLE;
        
        res.data.gold +=deposit;
        res.lastModified = CacheTimer::GetTimeMS();
        shard.dirty_list.insert(key);
        return { resStatus, res.data.gold};
    }

    // res, gold
    std::tuple<CACHE_STATUS, uint64_t> CacheStorageCurrency::TryWithdrawCurrency(uint64_t characterID, uint64_t withdraw) {
        Key key;
        Result result;
        key.characterID = characterID;
        uint16_t shardIndex = key.characterID & SHARD_SIZE_MASK;


        switch (TryGet(key.characterID & SHARD_SIZE_MASK, key, result))
        {
        case CACHE_STATUS::AVAILABLE:
            break;
        case CACHE_STATUS::DB_READING:
            return { CACHE_STATUS::DB_READING, 0 };
        case CACHE_STATUS::EVICTING:
            return { CACHE_STATUS::EVICTING, 0 };
        case CACHE_STATUS::EMPTY: {
            if (LoadFromDB(shardIndex, key) != CACHE_STATUS::AVAILABLE)
                return { CACHE_STATUS::EMPTY, 0 };
            break;
        }
        }
        auto& shard = m_shards[shardIndex];
        std::unique_lock<std::mutex> lock(shard.mutex);
        auto& res = shard.cache_data[key];

        if (res.status != CACHE_STATUS::AVAILABLE)
            return { res.status, 0 };

        CACHE_STATUS resStatus = CACHE_STATUS::AVAILABLE;
        if (res.data.gold < withdraw)
            return { CACHE_STATUS::BLOCKED, res.data.gold };
        
        res.data.gold -= withdraw;
        res.lastModified = CacheTimer::GetTimeMS();
        shard.dirty_list.insert(key);
        return { resStatus, res.data.gold };
    }

    std::unique_ptr<FlushCommand> CacheStorageCurrency::GetFlushCommand(Key key, Result result) {
        auto command = std::make_unique<FlushCommand>();
        command->stmtID = 8;

        command->params.push_back(result.data.gold);
        command->params.push_back(key.characterID);
        return command;
    }
}
