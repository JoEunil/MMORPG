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
#include <iostream>

#include <mysqlconn/include/mysql/jdbc.h>
#include "DBConnectionGame.h"
#include "DBWorker.h"
#include "CacheTimer.h"
#include "Config.h"

namespace Cache {
    enum class CACHE_STATUS : uint8_t {
        AVAILABLE, // 0
        EVICTING, //  1
        DB_READING, // 2
        EMPTY, // 3
        BLOCKED, // 4
        DUPLICATED, // 5 - 이미 배송된 event. BLOCKED와 달리 재시도 불필요, 캐시 상태
    };
    template<typename T>
    struct CacheItem {
        uint64_t lastModified;
        CACHE_STATUS status; // 0: available, 1:evicting, 2: DB reading, 3: Empty
        uint8_t rollbackCnt = 0;
        T data;
    };

    template<typename Key, typename Result, typename KeyHash>
    struct CacheShard {
        std::unordered_map<Key, Result, KeyHash> cache_data;
        std::list<Key> lru_list;
        std::unordered_map<Key, typename std::list<Key>::iterator, KeyHash> lru_pos;
        std::set<Key> dirty_list;

        std::mutex mutex;
    };

    template<typename Key, typename Result, typename KeyHash>
    class CacheStorage {
        void Initialize(DBWorker<DBConnectionGame>* d);
        bool IsReady() {
            if (m_flushFn == nullptr) {
                Core::sysLogger->LogError("cache storage", "m_flushFn not initialized");
                return false;
            }
            if (dbWorker == nullptr) {
                Core::sysLogger->LogError("cache storage", "dbWorker not initialized");
                return false;
            }
            return true;
        }
        friend class Initializer;
    protected:
        DBWorker<DBConnectionGame>* dbWorker;
        std::deque<CacheShard<Key, Result, KeyHash>> m_shards;
        std::function< void(const Key&, Result&) > m_flushFn;

        CACHE_STATUS TryGet(uint16_t shardIndex, const Key& key, Result& outResult);
        void Insert(uint16_t shardIndex, const Key& key, const Result& result);

        CACHE_STATUS TrySetReading(uint16_t shardIndex, const Key& key);
        void SetEmpty(uint16_t shardIndex, const Key& key);

    public:
        void SetFlushFn(std::function<void(const Key&, Result&)> f) {
            m_flushFn = f;
        }
        void ForEachDirty(std::function<bool(const Key&, Result&)> fn);
        void Rollback(uint16_t shardIndex, const Key& key);
        void WriteDone(uint16_t shardIndex, const Key& key);

        virtual std::string ResultToString(const Key& key, const Result& result) {
            return "null";
        }
    };

}
#include "CacheStorage.inl"
