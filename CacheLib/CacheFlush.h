#pragma once

#include <queue>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <memory>
#include <any>

#include <condition_variable>

#include "Config.h"

namespace Cache {
    struct FlushCommand {
        uint16_t stmtID;
        std::vector<std::any> params;
    };

    class DBConnectionPool;
    class DBConnectinon;
    class CacheFlush {
        std::vector<std::thread> m_threads;
        std::mutex m_mutex;
        std::condition_variable m_cv;
        std::deque<std::unique_ptr<FlushCommand>> m_flushQ;

        std::atomic<bool> m_running = false;
        DBConnectionPool* connectionPool;

        void Initialize(DBConnectionPool* p);
        bool IsReady() {
            if (!m_running.load() || m_threads.size() != FLUSH_THREADPOOL_SIZE || connectionPool == nullptr)
                return false;
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
