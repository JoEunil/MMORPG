
namespace Cache {
    // 템플릿은 컴파일 타임에 타입이 결정되기 때문에. 템플릿 클래스는 헤더에 정의가 필요함.
    // 다른 tu에서 참조할 경우, 타입에 맞는 소스는 생성되지 않기 때문


    template<typename Key, typename Result, typename KeyHash>
    void CacheStorage<Key, Result, KeyHash>::Initialize() {
        m_shards.resize(SHARD_SIZE);
    }

    template<typename Key, typename Result, typename KeyHash>
    bool CacheStorage<Key, Result, KeyHash>::TryGet(uint16_t shardIndex, const Key& key, Result& outResult) {
        auto& shard = m_shards[shardIndex];
        std::shared_lock<std::shared_mutex> lock(shard.dataMutex);
        auto it = shard.cache_data.find(key);
        if (it == shard.cache_data.end())
            return false;
        outResult = it->second;
        shard.lru_list.erase(shard.lru_pos[key]);
        shard.lru_list.push_front(key);
        shard.lru_pos[key] = shard.lru_list.begin();
        return true;
    }

    template<typename Key, typename Result, typename KeyHash>
    void CacheStorage<Key, Result, KeyHash>::Insert(uint16_t shardIndex, const Key& key, const Result& result) {
        auto& shard = m_shards[shardIndex];
        std::unique_lock<std::shared_mutex> lock(shard.dataMutex);
        if (shard.cache_data.size() >= MAX_CACHE_SIZE) {
            const auto& oldKey = shard.lru_list.back();
            m_flushFn(oldKey, shard.cache_data[oldKey]);
            shard.cache_data.erase(oldKey);
            shard.lru_pos.erase(oldKey);
            shard.lru_list.pop_back();
        } else {
            auto& item = shard.cache_data[key];
            item = result;
            item.lastModified = std::chrono::steady_clock::now();
            shard.lru_list.push_front(key);
            shard.lru_pos[key] = shard.lru_list.begin();
        }
    }

    template<typename Key, typename Result, typename KeyHash>
    void CacheStorage<Key, Result, KeyHash>::ForEachDirty(std::function<void(const Key&, Result&)> fn) {
        for (auto& shard : m_shards) {
            std::unique_lock<std::mutex> dirtyLock(shard.dirtyMutex);
            for (auto it = shard.dirty_list.begin(); it != shard.dirty_list.end(); ) {
                auto cacheIt = shard.cache_data.find(*it);
                if (cacheIt == shard.cache_data.end()) {
                    it = shard.dirty_list.erase(it);
                }
                else {
                    fn(cacheIt->first, cacheIt->second);
                    ++it;
                }
            }
        }
    }

}
