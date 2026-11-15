
namespace Cache {
    // 템플릿은 컴파일 타임에 타입이 결정되기 때문에. 템플릿 클래스는 헤더에 정의가 필요함.
    // 다른 tu에서 참조할 경우, 타입에 맞는 소스는 생성되지 않기 때문


    template<typename Key, typename Result, typename KeyHash>
    void CacheStorage<Key, Result, KeyHash>::Initialize(DBWorker<DBConnectionGame>* c, Core::ICachedClock* clock) {
        m_shards.resize(SHARD_SIZE);
        dbWorker = c;
        this->clock = clock;
    }

    template<typename Key, typename Result, typename KeyHash>
    CACHE_STATUS CacheStorage<Key, Result, KeyHash>::TryGet(uint16_t shardIndex, const Key& key, Result& outResult) {
        auto& shard = m_shards[shardIndex];
        std::lock_guard<std::mutex> lock(shard.mutex);
        auto it = shard.cache_data.find(key);
        if (it == shard.cache_data.end()) {
            return CACHE_STATUS::EMPTY;
        }
        if (it->second.status != CACHE_STATUS::AVAILABLE) {
            return it->second.status;
        }
        outResult = it->second;
        auto posIt = shard.lru_pos.find(key);
        if (posIt != shard.lru_pos.end()) {
            shard.lru_list.splice(shard.lru_list.begin(), shard.lru_list, posIt->second);
        }
        else {
            Core::errorLogger->LogError("cache storage", "AVAILABLE entry missing from lru_pos, re-registering");
            shard.lru_list.push_front(key);
            shard.lru_pos[key] = shard.lru_list.begin();
        }
        return CACHE_STATUS::AVAILABLE;
    }

    template<typename Key, typename Result, typename KeyHash>
    void CacheStorage<Key, Result, KeyHash>::Insert(uint16_t shardIndex, const Key& key, const Result& result) {
        auto& shard = m_shards[shardIndex];
        std::lock_guard<std::mutex> lock(shard.mutex);

        InsertLocked(shard, key, result);
    }

    template<typename Key, typename Result, typename KeyHash>
    void CacheStorage<Key, Result, KeyHash>::InsertLocked(CacheShard<Key, Result, KeyHash>& shard, const Key& key, const Result& result) {
        auto [it, inserted] = shard.cache_data.try_emplace(key);
        if (!inserted && it->second.status != CACHE_STATUS::DB_READING) {
            return;
        }
        shard.lru_list.push_front(key);
        shard.lru_pos[key] = shard.lru_list.begin();

        auto& item = it->second;
        item.data = result.data;
        item.lastModified = clock->GetTimeMS();
        item.status = CACHE_STATUS::AVAILABLE;

        TryEvictAfterInsertLocked(shard, key);
    }

    template<typename Key, typename Result, typename KeyHash>
    bool CacheStorage<Key, Result, KeyHash>::TryEvictAfterInsertLocked(CacheShard<Key, Result, KeyHash>& shard, const Key& insertedKey) {
        if (shard.lru_list.size() < MAX_CACHE_SIZE)
            return true;

        const auto oldKey = shard.lru_list.back();
        const auto oldIt = shard.cache_data.find(oldKey);
        if (oldIt == shard.cache_data.end()) {
            Core::errorLogger->LogError("cache storage", "LRU eviction target missing from cache_data");
            shard.lru_pos.erase(oldKey);
            shard.lru_list.pop_back();
            return true;
        }

        auto& oldItem = oldIt->second;
        // An accepted snapshot owns the entry until its completion callback.
        if (oldItem.flushInFlight)
            return true;

        shard.lru_pos.erase(oldKey);
        shard.lru_list.pop_back();
        if (!shard.dirty_list.contains(oldKey)) {
            shard.cache_data.erase(oldIt);
            return true;
        }

        oldItem.status = CACHE_STATUS::EVICTING;
        if (QueueFlushLocked(shard, oldKey, oldItem) == FlushEnqueueResult::Queued) {
            // WriteDone can erase the evicting entry once its snapshot is written.
            shard.dirty_list.erase(oldKey);
            return true;
        }

        oldItem.status = CACHE_STATUS::AVAILABLE;
        shard.lru_list.push_back(oldKey);
        shard.lru_pos[oldKey] = std::prev(shard.lru_list.end());
        if (!(oldKey == insertedKey)) {
            const auto newPos = shard.lru_pos.find(insertedKey);
            if (newPos != shard.lru_pos.end()) {
                shard.lru_list.erase(newPos->second);
                shard.lru_pos.erase(newPos);
            }
            shard.dirty_list.erase(insertedKey);
            shard.cache_data.erase(insertedKey);
        }
        return false;
    }

    template<typename Key, typename Result, typename KeyHash>
    CACHE_STATUS CacheStorage<Key, Result, KeyHash>::TrySetReading(uint16_t shardIndex, const Key& key) {
        auto& shard = m_shards[shardIndex];
        std::lock_guard<std::mutex> lock(shard.mutex);
        auto [it, inserted] = shard.cache_data.try_emplace(key);
        if (!inserted) {
            return it->second.status;
        }
        it->second.status = CACHE_STATUS::DB_READING;
        return CACHE_STATUS::EMPTY; // 기존 READING 상태가 있어, EMPTY를 성공 상태로. 
    }

    template<typename Key, typename Result, typename KeyHash>
    void CacheStorage<Key, Result, KeyHash>::ApplyLoadResultLocked(CacheShard<Key, Result, KeyHash>& shard, const Key& key, const Result* loaded) {
        if (loaded) {
            InsertLocked(shard, key, *loaded);
            return;
        }

        auto it = shard.cache_data.find(key);
        if (it != shard.cache_data.end() && it->second.status == CACHE_STATUS::DB_READING)
            shard.cache_data.erase(it);
    }

    template<typename Key, typename Result, typename KeyHash>
    void CacheStorage<Key, Result, KeyHash>::MarkDirtyLocked(CacheShard<Key, Result, KeyHash>& shard, const Key& key, Result& result) {
        result.lastModified = clock->GetTimeMS();
        shard.dirty_list.insert(key);
    }

    template<typename Key, typename Result, typename KeyHash>
    FlushEnqueueResult CacheStorage<Key, Result, KeyHash>::QueueFlushLocked(CacheShard<Key, Result, KeyHash>& shard, const Key& key, Result& result) {
        if (result.flushInFlight || !m_flushFn)
            return FlushEnqueueResult::Failed;
        result.flushGeneration = ++shard.nextFlushGeneration;
        result.flushInFlight = true;
        auto queued = m_flushFn(key, result);
        if (queued != FlushEnqueueResult::Queued)
            result.flushInFlight = false;
        return queued;
    }

    template<typename Key, typename Result, typename KeyHash>
    void CacheStorage<Key, Result, KeyHash>::ForEachDirty(std::function<bool(const Key&, Result&)> fn) {
        for (auto& shard : m_shards) {
            std::lock_guard<std::mutex> lock(shard.mutex);
            for (auto it = shard.dirty_list.begin(); it != shard.dirty_list.end(); ) {
                auto cacheIt = shard.cache_data.find(*it);
                if (cacheIt == shard.cache_data.end()) {
                    it = shard.dirty_list.erase(it);
                    continue;
                }

                auto& result = cacheIt->second;
                if (result.flushInFlight) {
                    ++it;
                    continue;
                }
                result.flushGeneration = ++shard.nextFlushGeneration;
                result.flushInFlight = true;
                if (fn(cacheIt->first, result)) {
                    it = shard.dirty_list.erase(it);
                } else {
                    result.flushInFlight = false;
                    it++;
                }
            }
        }
    }

    template<typename Key, typename Result, typename KeyHash>
    void CacheStorage<Key, Result, KeyHash>::RestoreEvictingEntryLocked(CacheShard<Key, Result, KeyHash>& shard, const Key& key, Result& result) {
        if (result.status != CACHE_STATUS::EVICTING)
            return;

        result.status = CACHE_STATUS::AVAILABLE;
        shard.lru_list.push_front(key);
        shard.lru_pos[key] = shard.lru_list.begin();
    }

    template<typename Key, typename Result, typename KeyHash>
    void CacheStorage<Key, Result, KeyHash>::Rollback(uint16_t shardIndex, const Key& key, uint64_t generation) {
        auto& shard = m_shards[shardIndex];
        std::lock_guard<std::mutex> lock(shard.mutex);
        auto it = shard.cache_data.find(key);
        if (it == shard.cache_data.end()) {
            Core::errorLogger->LogError("cache storage", "rollback target not found");
            return;
        }
        if (!it->second.flushInFlight || it->second.flushGeneration != generation)
            return;
        it->second.flushInFlight = false;
        it->second.rollbackCnt = static_cast<uint8_t>(std::min<int>(it->second.rollbackCnt + 1, 255));

        RestoreEvictingEntryLocked(shard, key, it->second);

        if (it->second.rollbackCnt >= 3) {
            Core::errorLogger->LogError("cache storage", "rollback limit exceeded", "detail", ResultToString(it->first, it->second));
            // 즉시 재시도 중단 — 데이터는 버리지 않고 다음 dirty flush pass에 위임한다.
            shard.dirty_list.insert(key);
            return;
        }
        if (QueueFlushLocked(shard, key, it->second) == FlushEnqueueResult::Failed)
            shard.dirty_list.insert(key);
        else
            shard.dirty_list.erase(key); // retry snapshot includes all mutations so far

    }

    template<typename Key, typename Result, typename KeyHash>
    void CacheStorage<Key, Result, KeyHash>::WriteDone(uint16_t shardIndex, const Key& key, uint64_t generation, bool retryClaim) {
        auto& shard = m_shards[shardIndex];
        std::lock_guard<std::mutex> lock(shard.mutex);
        auto it = shard.cache_data.find(key);
        if (it == shard.cache_data.end())
            return;

        if (!it->second.flushInFlight || it->second.flushGeneration != generation)
            return;
        it->second.flushInFlight = false;
        it->second.rollbackCnt = retryClaim ? static_cast<uint8_t>(std::min<int>(it->second.rollbackCnt + 1, 255)) : 0;
        if (retryClaim) {
            shard.dirty_list.insert(key);
            RestoreEvictingEntryLocked(shard, key, it->second);
        }

        // evicting 중인 것만 삭제
        if (it->second.status == CACHE_STATUS::EVICTING && !shard.lru_pos.contains(key) && !shard.dirty_list.contains(key)) {
            shard.cache_data.erase(it);
        }
    }
}
