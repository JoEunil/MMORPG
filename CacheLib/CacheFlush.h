#pragma once

#include <queue>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <memory>
#include <any>
#include <condition_variable>

#include <CoreLib/LoggerGlobal.h>
#include "CacheStorageInventory.h"
#include "CacheStorageCurrency.h"
#include "DBConnectionPool.h"
#include "DBConnectionGame.h"
#include "Config.h"

namespace Cache {
    struct FlushCommand {
        uint16_t stmtID;
        std::vector<std::any> params;
    };
    class DBConnectinon;
    class CacheFlush {
        std::vector<std::thread> m_threads;
        std::mutex m_mutex;
        std::condition_variable m_cv;
        std::deque<std::unique_ptr<FlushCommand>> m_flushQ;

        std::atomic<bool> m_running = false;
        DBConnectionPool<DBConnectionGame>* connectionPool;
        CacheStorageInventory* cache_inventory;
        CacheStorageCurrency* cache_currency;
        void Initialize(DBConnectionPool<DBConnectionGame>* p, CacheStorageInventory* c5, CacheStorageCurrency* c7);
        bool IsReady() {
            if (!m_running.load(std::memory_order_relaxed)) {
                Core::sysLogger->LogError("cache flush", "not running");
                return false;
            }
            if (m_threads.size() != FLUSH_THREADPOOL_SIZE) {
                Core::sysLogger->LogError("cache flush", "invalid thread size");
                return false;
            }
            if (connectionPool == nullptr) {
                Core::sysLogger->LogError("cache flush", "connectionPool not initialized");
                return false;
            }
            if (cache_inventory == nullptr) {
                Core::sysLogger->LogError("cache flush", "cache_inventory not initialized");
                return false;
            }
            if (cache_currency == nullptr) {
                Core::sysLogger->LogError("cache flush", "cache_currency not initialized");
                return false;
            }
            return true;
        }
        void Stop();

        void DBWrite(FlushCommand* command);
        void ThreadFunc();
        friend class Initializer;
    public:
        void EnqueueFlushQ(std::unique_ptr<FlushCommand> c);
    };
}
