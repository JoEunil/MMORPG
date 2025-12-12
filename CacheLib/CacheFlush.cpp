#include "CacheFlush.h"

#include "DBConnectionPool.h"
#include "DBConnection.h"

#include "Config.h"

namespace Cache {

    void CacheFlush::DBWrite(FlushCommand* command) {
        switch (command->stmtID) {
        case 6: {
            auto conn = connectionPool->Acquire();
            auto& param0 = std::any_cast<uint64_t&>(command->params[0]);               
            auto& param1 = std::any_cast<std::vector<uint8_t>&>(command->params[1]); 
            auto res = conn->ExecuteUpdate(6, command->stmtID, param0, param1);
            if (res == 0) // error
                break;
            break;
        }
        default:
            break;
        }
    }

    void CacheFlush::ThreadFunc() {
        while (m_running.load()) {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock,[&] {return !m_running || !m_flushQ.empty();});
            if (!m_running.load())
                break;
            while (!m_flushQ.empty()) {
                auto work = std::move(m_flushQ.front());
                m_flushQ.pop_front();
                lock.unlock();
                DBWrite(work.get());
                lock.lock();
            }
        }
    }

    void CacheFlush::Initialize(DBConnectionPool* p) {
        connectionPool = p;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_threads.resize(FLUSH_THREADPOOL_SIZE);
        m_running.store(true);
        for (int i = 0; i < FLUSH_THREADPOOL_SIZE; i++)
        {
            m_threads[i] = std::thread(&CacheFlush::ThreadFunc, this);
        }
    }

    void CacheFlush::Stop() {
        m_running.store(false);
        m_cv.notify_all();

        for (auto& t : m_threads) {
            if (t.joinable())
                t.join();
        }

        while (!m_flushQ.empty()) {
            auto work = std::move(m_flushQ.front());
            m_flushQ.pop_front();
            DBWrite(work.get());
        }
    }

    void CacheFlush::EnqueueFlushQ(std::unique_ptr<FlushCommand> command) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_flushQ.push_back(std::move(command));
        m_cv.notify_one();
    }
}
