#pragma once

#include <queue>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <memory>
#include <any>
#include <condition_variable>
#include <functional>

#include <CoreLib/LoggerGlobal.h>
#include "CacheStorageInventory.h"
#include "CacheStorageCurrency.h"
#include "DBConnectionPool.h"
#include "DBConnectionGame.h"
#include "DBConnectionBazaar.h"
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
        std::condition_variable m_drainCv;   // 큐가 비고 진행 중인 DBWrite도 없을 때 통지
        std::deque<std::unique_ptr<FlushCommand>> m_flushQ;
        uint32_t m_inFlight = 0;             // pop 후 DBWrite 실행 중인 command 수
        std::function<void(uint64_t, uint64_t)> m_invFlushedFn;

        std::atomic<bool> m_running = false;
        DBConnectionPool<DBConnectionGame>* connectionPoolGame;
        DBConnectionPool<DBConnectionBazaar>* connectionPoolBazaar;
        CacheStorageInventory* cache_inventory;
        CacheStorageCurrency* cache_currency;
        void Initialize(DBConnectionPool<DBConnectionGame>* pg, DBConnectionPool<DBConnectionBazaar>* pb, CacheStorageInventory* c5, CacheStorageCurrency* c7);
        void SetInvFlushedFn(std::function<void(uint64_t, uint64_t)> f) {
            m_invFlushedFn = std::move(f);
        }
        bool IsReady() {
            if (!m_running.load(std::memory_order_relaxed)) {
                Core::sysLogger->LogError("cache flush", "not running");
                return false;
            }
            if (m_threads.size() != FLUSH_THREADPOOL_SIZE) {
                Core::sysLogger->LogError("cache flush", "invalid thread size");
                return false;
            }
            if (connectionPoolGame == nullptr) {
                Core::sysLogger->LogError("cache flush", "connectionPoolGame not initialized");
                return false;
            }
            if (connectionPoolBazaar == nullptr) {
                Core::sysLogger->LogError("cache flush", "connectionPoolBazaar not initialized");
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
        // 큐가 비고 진행 중인 DBWrite도 끝날 때까지 블록한다.
        // 워커 스레드가 살아있는 동안(= Stop() 이전)에만 의미가 있다.
        void WaitUntilDrained();
    };
}
