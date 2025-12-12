#pragma once

#include <unordered_map>
#include <list>
#include <cstdint>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <algorithm>
#include <chrono>
#include <functional>
#include <deque>

#include "Config.h"

namespace Cache {
    template<typename T>
    struct CacheItem {
        std::chrono::steady_clock::time_point lastModified;
        T data;
    };
    class DBConnectionPool;

    template<typename Key, typename Result, typename KeyHash>
    struct CacheShard {
        std::unordered_map<Key, Result, KeyHash> cache_data;
        std::list<Key> lru_list;
        std::unordered_map<Key, typename std::list<Key>::iterator, KeyHash> lru_pos;
        std::set<Key> dirty_list;

        std::shared_mutex dataMutex;
        std::mutex dirtyMutex;
    };

    template<typename Key, typename Result, typename KeyHash>
    class CacheStorage {
        void Initialize();
        bool IsReady() {
            if (m_flushFn == nullptr)
                return false;
            return true;
        }
        friend class Initializer;
    protected:
        DBConnectionPool* connectionPool = nullptr;
        std::deque<CacheShard<Key, Result, KeyHash>> m_shards;
        std::function< void(const Key&, Result&) > m_flushFn;

        bool TryGet(uint16_t shardIndex, const Key& key, Result& outResult);
        void Insert(uint16_t shardIndex, const Key& key, const Result& result);
        
    public:
        void SetFlushFn(std::function<void(const Key&, Result&)> f) {
            m_flushFn = f;
        }
        void ForEachDirty(std::function<void(const Key&, Result&)> fn);
    };

}
#include "CacheStorage.inl"
