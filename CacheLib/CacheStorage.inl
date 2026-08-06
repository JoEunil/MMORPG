
namespace Cache {
    // 템플릿은 컴파일 타임에 타입이 결정되기 때문에. 템플릿 클래스는 헤더에 정의가 필요함.
    // 다른 tu에서 참조할 경우, 타입에 맞는 소스는 생성되지 않기 때문


    template<typename Key, typename Result, typename KeyHash>
    void CacheStorage<Key, Result, KeyHash>::Initialize(DBWorker<DBConnectionGame>* c) {
        m_shards.resize(SHARD_SIZE);
        dbWorker = c;
    }

    template<typename Key, typename Result, typename KeyHash>
    CACHE_STATUS CacheStorage<Key, Result, KeyHash>::TryGet(uint16_t shardIndex, const Key& key, Result& outResult) {
        auto& shard = m_shards[shardIndex];
        std::lock_guard<std::mutex> lock(shard.mutex);
        auto it = shard.cache_data.find(key);
        if (it == shard.cache_data.end()) {
            //std::cout << "cache miss\n";
            return CACHE_STATUS::EMPTY;
        }
        if (it->second.status != CACHE_STATUS::AVAILABLE) {
            //std::cout << "data not ready, status: " << (int)it->second.status <<  " \n";
            return it->second.status;
        }
        //std::cout << "cache hit\n";
        outResult = it->second;
        // splice: list 전용 메서드, O(1), 연결리스트 포인터 교체
        // splice(삽입할 위치, 원본 리스트, 가져올 iterator)
        auto posIt = shard.lru_pos.find(key);
        if (posIt != shard.lru_pos.end()) {
            shard.lru_list.splice(shard.lru_list.begin(), shard.lru_list, posIt->second);
            posIt->second = shard.lru_list.begin();
        }
        else {
            // AVAILABLE인데 LRU 추적에서 이탈한 상태 — operator[]로 접근하면
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

        auto it = shard.cache_data.find(key);
        if (it != shard.cache_data.end() && it->second.status != CACHE_STATUS::DB_READING) {
            return; 
        }
        shard.lru_list.push_front(key);
        shard.lru_pos[key] = shard.lru_list.begin();

        auto& item = shard.cache_data[key];
        item.data = result.data;
        item.lastModified = CacheTimer::GetTimeMS();
        item.status = CACHE_STATUS::AVAILABLE;

        if (shard.lru_list.size() >= MAX_CACHE_SIZE) {
            const auto oldKey = shard.lru_list.back();
            shard.lru_pos.erase(oldKey);
            shard.lru_list.pop_back();

            auto it = shard.dirty_list.find(oldKey);
            if (it != shard.dirty_list.end()) {
                shard.cache_data[oldKey].status = CACHE_STATUS::EVICTING;
                m_flushFn(oldKey, shard.cache_data[oldKey]);
                // EVICTING이라 flush 중 재수정 불가 — 여기서 dirty를 지워야 WriteDone이 erase함
                shard.dirty_list.erase(oldKey);
            } else {
                shard.cache_data.erase(oldKey);
                // dirty list에 없으면 db와 동일한 상태임으로 바로 지워도 됨.
            }
        }
    }

    template<typename Key, typename Result, typename KeyHash>
    CACHE_STATUS CacheStorage<Key, Result, KeyHash>::TrySetReading(uint16_t shardIndex, const Key& key) {
        auto& shard = m_shards[shardIndex];
        std::lock_guard<std::mutex> lock(shard.mutex);
        auto it = shard.cache_data.find(key);
        if (it != shard.cache_data.end()) {
            return it->second.status;
        }
        shard.cache_data[key].status = CACHE_STATUS::DB_READING;
        return CACHE_STATUS::EMPTY; // 기존 READING 상태가 있어, EMPTY를 성공 상태로. 
    }

    template<typename Key, typename Result, typename KeyHash>
    void CacheStorage<Key, Result, KeyHash>::SetEmpty(uint16_t shardIndex, const Key& key) {
        auto& shard = m_shards[shardIndex];
        std::lock_guard<std::mutex> lock(shard.mutex);
        shard.cache_data.erase(key);
    }

    template<typename Key, typename Result, typename KeyHash>
    void CacheStorage<Key, Result, KeyHash>::ForEachDirty(std::function<bool(const Key&, Result&)> fn) {
        for (auto& shard : m_shards) {
            std::lock_guard<std::mutex> lock(shard.mutex);
            for (auto it = shard.dirty_list.begin(); it != shard.dirty_list.end(); ) {
                auto cacheIt = shard.cache_data.find(*it);
                if (cacheIt == shard.cache_data.end()) {
                    it = shard.dirty_list.erase(it);
                }
                else {
                    if (fn(cacheIt->first, cacheIt->second)) {
                        it = shard.dirty_list.erase(it);
                    } else {
                        it++;
                    }
                }
            }
        }
    }

    template<typename Key, typename Result, typename KeyHash>
    void CacheStorage<Key, Result, KeyHash>::Rollback(uint16_t shardIndex, const Key& key) {
        auto& shard = m_shards[shardIndex];
        std::lock_guard<std::mutex> lock(shard.mutex);
        auto it = shard.cache_data.find(key);
        if (it == shard.cache_data.end()) {
            Core::errorLogger->LogError("cache storage", "rollback target not found");
            return;
        }
        it->second.rollbackCnt++;

        if (it->second.status == CACHE_STATUS::EVICTING) {
            // DB가 죽은 상황이면 cache에 무한히 증식되는 상황이 아니라서 rollback 자체가 문제되지는 않음.
            it->second.status = CACHE_STATUS::AVAILABLE;
            shard.lru_list.push_front(key);
            shard.lru_pos[key] = shard.lru_list.begin();
        }

        if (it->second.rollbackCnt >= 3) {
            Core::errorLogger->LogError("cache storage", "rollback limit exceeded", "detail", ResultToString(it->first, it->second));
            // 즉시 재시도 중단 — 데이터는 버리지 않고 dirty 재마킹으로 다음 flush 주기(최대 30초)에 위임한다.
            shard.dirty_list.insert(key);
            return;
        }
        m_flushFn(key, it->second);

    }

    template<typename Key, typename Result, typename KeyHash>
    void CacheStorage<Key, Result, KeyHash>::WriteDone(uint16_t shardIndex, const Key& key) {
        auto& shard = m_shards[shardIndex];
        std::lock_guard<std::mutex> lock(shard.mutex);
        auto it = shard.cache_data.find(key);
        if (it == shard.cache_data.end())
            return;

        it->second.rollbackCnt = 0;

        // evicting 중인 것만 삭제
        auto itLru = shard.lru_pos.find(key);
        if (itLru == shard.lru_pos.end()) {
            shard.cache_data.erase(key);
        }
    }
}
