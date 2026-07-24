#include "CacheFlush.h"

#include "DBConnectionPool.h"
#include "DBConnectionGame.h"

#include "Config.h"
#include <format>

namespace Cache {

    void CacheFlush::DBWrite(FlushCommand* command) {
        switch (command->stmtID) {
        case 6: {
            auto conn = connectionPoolGame->Acquire();
            if (conn == nullptr) {
                Key5 key;
                key.characterID = std::any_cast<uint64_t&>(command->params[1]);
                auto shardIndex = key.characterID & SHARD_SIZE_MASK;
                cache_inventory->Rollback(shardIndex, key);
                Core::errorLogger->LogError("cache flush", "connection acquire failed case 6", "char_id", key.characterID);
                return;
            }
            auto& param0 = std::any_cast<std::vector<uint8_t>&>(command->params[0]);
            auto& param1 = std::any_cast<uint64_t&>(command->params[1]); 
            int res = 0;
            try {
                res = conn->ExecuteUpdate(command->stmtID, param0, param1);
            }
            catch (sql::SQLException& e) {
                Core::errorLogger->LogError("cache flush", "DB write exception case 6",
                    "code", e.getErrorCode(),
                    "msg", e.what());
                Key5 key;
                key.characterID = std::any_cast<uint64_t&>(command->params[1]);
                auto shardIndex = key.characterID & SHARD_SIZE_MASK;
                cache_inventory->Rollback(shardIndex, key);
                connectionPoolGame->Return(conn);
                return;
            }

            connectionPoolGame->Return(conn);
            Key5 key;
            key.characterID = param1;
            auto shardIndex = param1 & SHARD_SIZE_MASK;
            
            if (res == 0) {// error
               cache_inventory->Rollback(shardIndex, key);
               Core::errorLogger->LogInfo("cache flush", "DB write failed case 6", "char_id", key.characterID);
               return;
            }

            CrashPoint("CLAIM"); // 인벤토리 blob durable 이후, outbox CLAIM 전

            // blob에서 역직렬화 해서 eventRing 추출
            const InventoryData* inv = reinterpret_cast<const InventoryData*>(param0.data());

            uint8_t pendingCnt = (inv->tail - inv->head) & RING_SIZE_MASK;
            if (pendingCnt == 0 && inv->lastOp)
                pendingCnt = RING_SIZE; // full

            if (pendingCnt > 0) {
                auto bazaarConn = connectionPoolBazaar->Acquire();
                if (bazaarConn == nullptr) {
                    // claim은 멱등 — rollback 없이 스킵, 다음 flush/재접속이 수렴시킴
                    Core::errorLogger->LogError("cache flush", "bazaar connection acquire failed case 6", "char_id", key.characterID);
                }
                else {
                    uint8_t claimedCnt = 0;
                    try {
                        uint8_t idx = inv->head;
                        for (uint8_t k = 0; k < pendingCnt; ++k, idx = (idx + 1) & RING_SIZE_MASK) {
                            bazaarConn->ExecuteUpdate(19, inv->recentEventIds[idx], key.characterID);
                            claimedCnt++; // 0 rows(이미 CLAIMED)도 성공으로 진행
                        }
                    }
                    catch (sql::SQLException& e) {
                        Core::errorLogger->LogError("cache flush", "outbox claim exception case 6", "code", e.getErrorCode(), "msg", e.what(), "char_id", key.characterID);
                        // 여기서 중단 — 성공한 prefix만큼만 head 전진
                    }
                    connectionPoolBazaar->Return(bazaarConn);

                    // head 전진(dirty 재마킹) 후에 WriteDone을 호출해야 entry가 erase되지 않고
                    // 전진된 head가 다음 flush로 영속된다
                    if (claimedCnt > 0) {
                        if(!cache_inventory->AdvanceInboxHead(shardIndex, key, inv->head, claimedCnt))
                            Core::gameLogger->LogInfo("cache flush", "inbox advance failed", "char_id", key.characterID);
                    }
                }
            }
            cache_inventory->WriteDone(shardIndex, key);
            if (m_invFlushedFn)
                m_invFlushedFn(key.characterID, inv->lastLsn);
            break;
        }
        case 8: {
            auto conn = connectionPoolGame->Acquire();
            if (conn == nullptr) {
                Key7 key;
                key.characterID = std::any_cast<uint64_t&>(command->params[1]);
                auto shardIndex = key.characterID & SHARD_SIZE_MASK;
                cache_currency->Rollback(shardIndex, key);
                Core::errorLogger->LogError("cache flush", "connection acquire failed case 8", "char_id", key.characterID);
                return;
            }
            auto& param0 = std::any_cast<uint64_t&>(command->params[0]);
            auto& param1 = std::any_cast<uint64_t&>(command->params[1]);
            int res = 0;
            try {
                res = conn->ExecuteUpdate(command->stmtID, param0, param1);
            }
            catch (sql::SQLException& e) {
                Core::errorLogger->LogError("cache flush", "DB write exception case 8",
                    "code", e.getErrorCode(),
                    "msg", e.what());
                Key7 key;
                key.characterID = std::any_cast<uint64_t&>(command->params[1]);
                auto shardIndex = key.characterID & SHARD_SIZE_MASK;
                cache_currency->Rollback(shardIndex, key);
                connectionPoolGame->Return(conn);
                return;
            }

            connectionPoolGame->Return(conn);
            Key7 key;
            key.characterID = param1;
            auto shardIndex = param1 & SHARD_SIZE_MASK;

            if (res == 0) {// error 
                cache_currency->Rollback(shardIndex, key);
                Core::errorLogger->LogInfo("cache flush", "DB write failed case 8", "char_id", key.characterID);
                return;
            }
            cache_currency->WriteDone(shardIndex, key);
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
        while (m_running.load(std::memory_order_relaxed)) {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock,[&] {return !m_running || !m_flushQ.empty();});
            if (!m_running.load(std::memory_order_relaxed))
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

    void CacheFlush::Initialize(DBConnectionPool<DBConnectionGame>* pg, DBConnectionPool<DBConnectionBazaar>* pb, CacheStorageInventory* c5, CacheStorageCurrency* c7) {
        connectionPoolGame = pg;
        connectionPoolBazaar = pb;
        cache_inventory = c5;
        cache_currency = c7;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_threads.resize(FLUSH_THREADPOOL_SIZE);
        m_running.store(true, std::memory_order_relaxed);
        for (int i = 0; i < FLUSH_THREADPOOL_SIZE; i++)
        {
            m_threads[i] = std::thread(&CacheFlush::ThreadFunc, this);
        }
    }

    void CacheFlush::Stop() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_running.store(false, std::memory_order_relaxed);
        }
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
