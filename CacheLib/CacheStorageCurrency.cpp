#include "CacheStorageCurrency.h"

#include <mysqlconn/include/mysql/jdbc.h>
#include <CoreLib/Message.h>
#include "DBWorker.h"
#include "CacheFlush.h"

namespace Cache {
    CACHE_STATUS CacheStorageCurrency::LoadFromDB(uint16_t shardIndex, Key& key, bool claimed) {
        if (!claimed) {
            auto status = TrySetReading(shardIndex, key);
            if (status != CACHE_STATUS::EMPTY) return status;
        }

        const auto enqueueResult = dbWorker->Enqueue([=](DBConnectionGame* conn) {
            Result result;
            auto res = conn->ExecuteSelect(StatementId::SelectCurrency, key.characterID);
            if (!res) {
                CompleteLoad(shardIndex, key, nullptr);
                return;
            }
            // A successful empty SELECT is a new wallet. The normal UPSERT flush creates its row.
            result.data.gold = res->next() ? static_cast<uint64_t>(res->getUInt64("gold")) : 0;
            result.rollbackCnt = 0;
            CompleteLoad(shardIndex, key, &result);
            }, nullptr, [this, shardIndex, key] { CompleteLoad(shardIndex, key, nullptr); });
        if (enqueueResult != DBEnqueueResult::Queued) {
            CompleteLoad(shardIndex, key, nullptr);
            return CACHE_STATUS::EMPTY;
        }
        return CACHE_STATUS::DB_READING;
    }

    CurrencyCacheItem CacheStorageCurrency::Getter(uint64_t characterID) {
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

    std::tuple<CACHE_STATUS, uint64_t> CacheStorageCurrency::DepositLocked(uint16_t index, Key key, uint64_t gold) {
        auto& shard = m_shards[index];
        auto found = shard.cache_data.find(key);
        if (found == shard.cache_data.end()) return {CACHE_STATUS::EMPTY, 0};
        auto& entry = found->second;
        if (entry.status != CACHE_STATUS::AVAILABLE) return {entry.status, 0};
        if (gold > UINT64_MAX - entry.data.gold) return {CACHE_STATUS::BLOCKED, entry.data.gold};
        entry.data.gold += gold;
        entry.lastModified = clock->GetTimeMS();
        shard.dirty_list.insert(key);
        return {CACHE_STATUS::AVAILABLE, entry.data.gold};
    }

    void CacheStorageCurrency::GrantCurrency(uint64_t characterID, uint64_t gold,
        std::function<void(CACHE_STATUS, uint64_t)> completed) {
        const uint16_t index = characterID & SHARD_SIZE_MASK;
        auto& shard = m_shards[index];
        bool load = false;
        std::tuple<CACHE_STATUS, uint64_t> result{CACHE_STATUS::BLOCKED, 0};
        {
            std::lock_guard lock(shard.mutex);
            if (characterID != 0 && gold != 0) {
                auto found = shard.cache_data.find(Key{characterID});
                if (found == shard.cache_data.end() || found->second.status == CACHE_STATUS::DB_READING) {
                    pendingDeposits[index][characterID].push_back({gold, std::move(completed)});
                    load = found == shard.cache_data.end();
                    if (!load) return;
                    // Claim the read under the same lock as the pending deposit; no completion can miss it.
                    shard.cache_data[Key{characterID}].status = CACHE_STATUS::DB_READING;
                } else {
                    if (found->second.status == CACHE_STATUS::EVICTING) {
                        found->second.status = CACHE_STATUS::AVAILABLE;
                        shard.lru_list.push_front(Key{characterID});
                        shard.lru_pos[Key{characterID}] = shard.lru_list.begin();
                    }
                    result = DepositLocked(index, Key{characterID}, gold);
                }
            }
        }
        if (load) {
            Key key{characterID};
            LoadFromDB(index, key, true);
            return;
        }
        if (completed) completed(std::get<0>(result), std::get<1>(result));
    }

    void CacheStorageCurrency::CompleteLoad(uint16_t index, Key key, const Result* loaded) {
        std::vector<PendingDeposit> deposits;
        std::vector<std::tuple<CACHE_STATUS, uint64_t>> results;
        auto& shard = m_shards[index];
        {
            std::lock_guard lock(shard.mutex);
            if (loaded) InsertLocked(shard, key, *loaded);
            else {
                auto found = shard.cache_data.find(key);
                if (found != shard.cache_data.end() && found->second.status == CACHE_STATUS::DB_READING)
                    shard.cache_data.erase(found);
            }
            const auto pending = pendingDeposits[index].find(key.characterID);
            if (pending != pendingDeposits[index].end()) {
                deposits = std::move(pending->second);
                pendingDeposits[index].erase(pending);
                for (const auto& deposit : deposits)
                    results.push_back(loaded ? DepositLocked(index, key, deposit.gold)
                        : std::tuple<CACHE_STATUS, uint64_t>{CACHE_STATUS::EMPTY, 0});
            }
        }
        for (size_t i = 0; i < deposits.size(); ++i)
            if (deposits[i].completed) deposits[i].completed(std::get<0>(results[i]), std::get<1>(results[i]));
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
        return DepositLocked(shardIndex, key, deposit);
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
        auto it = shard.cache_data.find(key);
        if (it == shard.cache_data.end())
            return { CACHE_STATUS::EMPTY, 0 };
        auto& res = it->second;

        if (res.status != CACHE_STATUS::AVAILABLE)
            return { res.status, 0 };

        CACHE_STATUS resStatus = CACHE_STATUS::AVAILABLE;
        if (res.data.gold < withdraw)
            return { CACHE_STATUS::BLOCKED, res.data.gold };
        
        res.data.gold -= withdraw;
        res.lastModified = clock->GetTimeMS();
        shard.dirty_list.insert(key);
        return { resStatus, res.data.gold };
    }

    std::unique_ptr<FlushCommand> CacheStorageCurrency::GetFlushCommand(Key key, Result result) {
        auto command = std::make_unique<FlushCommand>();
        command->stmtID = StatementId::UpsertCurrency;
        command->generation = result.flushGeneration;

        command->params.emplace_back(result.data.gold);
        command->params.emplace_back(key.characterID);
        return command;
    }
}
