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
	template<typename T>
    class DBWorker {
        std::queue<std::function<void(T*)>> m_queue;
        std::vector<std::thread> m_threads;

        std::atomic<bool> m_running;
        std::mutex m_mutex;
        size_t m_threadPoolSize;
        std::condition_variable m_cv;
        DBConnectionPool<T>* connectionPool;

        void Initialize(DBConnectionPool<T>* c, size_t threadPoolSize) {
            std::lock_guard<std::mutex> lock(m_mutex);
            connectionPool = c;
			m_threadPoolSize = threadPoolSize;
            m_threads.resize(threadPoolSize);
            m_running.store(true, std::memory_order_relaxed);
            for (int i = 0; i < threadPoolSize; i++)
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
            if (m_threads.size() != m_threadPoolSize) {
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
                std::function<void(T*)> work;
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
                T* conn = connectionPool->Acquire();
                work(conn);
                conn->ClearResults();
                connectionPool->Return(conn);
            }

            Core::sysLogger->LogInfo("DB worker thread", "DB worker thread stopped", "threadID", ss.str());
        }
        void Stop() {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_running.store(false, std::memory_order_relaxed);
            }
            // lost wakeup 방지: 워커가 predicate 평가 후 wait 진입하기 전 틈에
            // notify가 끼면 신호를 놓치고 영원히 잠듦.
            // 락을 잡고 플래그를 바꾸면 그 틈에 끼어들 수 없음.
            m_cv.notify_all();
            for (int i = 0; i < m_threadPoolSize; i++)
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

        void Enqueue(std::function<void(T*)> work) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push({ std::move(work) });
            m_cv.notify_one();
        }

    };
}