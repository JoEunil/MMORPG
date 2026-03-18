#include "CacheFlush.h"

#include "DBConnectionPool.h"
#include "DBConnection.h"

#include "Config.h"
#include <format>

namespace Cache {

    void CacheFlush::DBWrite(FlushCommand* command) {
        switch (command->stmtID) {
        case 6: {
            auto conn = connectionPool->Acquire();
            if (conn == nullptr) {
                Key5 key;
                key.characterID = std::any_cast<uint64_t&>(command->params[1]);
                auto shardIndex = key.characterID & SHARD_SIZE_MASK;
                cache_5->Rollback(shardIndex, key);
                Core::errorLogger->LogError("cache flush", "connection acquire failed", "char_id", key.characterID);
                return;
            }
            auto& param0 = std::any_cast<std::vector<uint8_t>&>(command->params[0]);
            auto& param1 = std::any_cast<uint64_t&>(command->params[1]); 
            int res = 0;
            try {
                res = conn->ExecuteUpdate(command->stmtID, param0, param1);
            }
            catch (sql::SQLException& e) {
                Core::errorLogger->LogError("cache flush", "DB write exception",
                    "code", e.getErrorCode(),
                    "msg", e.what());
                Key5 key;
                key.characterID = std::any_cast<uint64_t&>(command->params[1]);
                auto shardIndex = key.characterID & SHARD_SIZE_MASK;
                cache_5->Rollback(shardIndex, key);
            }

            connectionPool->Return(conn);
            Key5 key;
            key.characterID = param1;
            auto shardIndex = param1 & SHARD_SIZE_MASK;
            
            if (res == 0) {// error 
               cache_5->Rollback(shardIndex, key);
               Core::errorLogger->LogInfo("cache flush", "DB write failed", "char_id", key.characterID);
               break;
            }
            cache_5->WriteDone(shardIndex, key);
            break;
        }
        default:
            break;
        }
    }

    void CacheFlush::ThreadFunc() {
        auto tid = std::this_thread::get_id();
        std::stringstream ss;
        ss << tid;
        Core::sysLogger->LogInfo("cache flush", "flush thread started", "threadID", ss.str());
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
        Core::sysLogger->LogInfo("cache flush", "flush thread stopped", "threadID", ss.str());
    }

    void CacheFlush::Initialize(DBConnectionPool* p, CacheStorage5* c5) {
        connectionPool = p;
        cache_5 = c5;
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
