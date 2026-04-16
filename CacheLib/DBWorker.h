#pragma once
#include <queue>
#include <functional>
#include <thread>
#include <condition_variable>

#include "DBConnection.h"
#include "DBConnectionPool.h"
#include "Config.h"
#include <CoreLib/LoggerGlobal.h>

namespace Cache {
    class DBWorker {
        std::queue<std::function<void(DBConnection*)>> m_queue;
        std::vector<std::thread> m_threads;

        std::atomic<bool> m_running;
        std::mutex m_mutex;
        std::condition_variable m_cv;
        DBConnectionPool* connectionPool;

        void Initialize(DBConnectionPool* c) {
            std::lock_guard<std::mutex> lock(m_mutex);
            connectionPool = c;
            m_threads.resize(DB_WORKER_THREADPOOL_SIZE);
            m_running.store(true, std::memory_order_relaxed);
            for (int i = 0; i < DB_WORKER_THREADPOOL_SIZE; i++)
            {
                m_threads[i] = std::thread(&DBWorker::ThreadFunc, this);
            }
        }

        bool IsReady() {
            if (!m_running.load(std::memory_order_relaxed)) {
                Core::sysLogger->LogError("DB worker thread", "not running");
                return false;
            }
            if (connectionPool == nullptr) {
                Core::sysLogger->LogError("DB worker thread", "connectionPool not initilaized");
                return false;
            }
            if (m_threads.size() != DB_WORKER_THREADPOOL_SIZE) {
                Core::sysLogger->LogError("DB worker thread", "invalid thread size");
                return false;
            }
            return true;
        }

        void ThreadFunc() {
            auto tid = std::this_thread::get_id();
            std::stringstream ss;
            ss << tid;
            Core::sysLogger->LogInfo("DB worker thread", "DB worker thread started", "threadID", ss.str());
            
            while (m_running.load(std::memory_order_relaxed))
            {
                std::function<void(DBConnection*)> work;
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_cv.wait(lock, [this] {
                        return !m_queue.empty() || !m_running.load(std::memory_order_relaxed);
                        });
                    if (!m_running.load(std::memory_order_relaxed) || m_queue.empty())
                        break;
                    work = std::move(m_queue.front());
                    m_queue.pop();
                }
                DBConnection* conn = connectionPool->Acquire();
                work(conn);
                connectionPool->Return(conn);
            }

            Core::sysLogger->LogInfo("DB worker thread", "DB worker thread stopped", "threadID", ss.str());
        }
        void Stop() {
            m_running.store(false, std::memory_order_relaxed);
            m_cv.notify_all();
            for (int i = 0; i < DB_WORKER_THREADPOOL_SIZE; i++)
            {
                if (m_threads[i].joinable())
                    m_threads[i].join();
            }
        }
        friend class Initializer;
    public:
        ~DBWorker() {
            Stop();
        }

        void Enqueue(std::function<void(DBConnection*)> work) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push({ std::move(work) });
            m_cv.notify_one();
        }

    };
}