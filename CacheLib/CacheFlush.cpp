#include "CacheFlush.h"

#include "DBConnectionPool.h"
#include "DBConnectionGame.h"

#include "Config.h"
#include <format>
#include <chrono>

namespace Cache {
    void CacheFlush::ExecuteWrite(FlushCommand* command) {
        try {
            DBWrite(command);
            return;
        }
        catch (const std::exception& e) {
            Core::errorLogger->LogError("cache flush", "flush command failed", "msg", e.what());
        }
        catch (...) {
            Core::errorLogger->LogError("cache flush", "flush command failed with unknown exception");
        }
        // Scoped connections are returned during unwinding; release the matching
        // in-flight generation and retain the latest cache state for retry.
        if (command->params.size() > 1) {
            if (const auto* id = std::any_cast<uint64_t>(&command->params[1])) {
                if (command->stmtID == 6)
                    cache_inventory->Rollback(*id & SHARD_SIZE_MASK, Key5{ *id }, command->generation);
                else if (command->stmtID == 8)
                    cache_currency->Rollback(*id & SHARD_SIZE_MASK, Key7{ *id }, command->generation);
            }
        }
    }

    void CacheFlush::DBWrite(FlushCommand* command) {
        switch (command->stmtID) {
        case 6: {
            auto conn = connectionPoolGame->AcquireScoped();
            if (conn == nullptr) {
                Key5 key;
                key.characterID = std::any_cast<uint64_t&>(command->params[1]);
                auto shardIndex = key.characterID & SHARD_SIZE_MASK;
                cache_inventory->Rollback(shardIndex, key, command->generation);
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
                cache_inventory->Rollback(shardIndex, key, command->generation);
                return;
            }

            conn.reset();
            Key5 key;
            key.characterID = param1;
            auto shardIndex = param1 & SHARD_SIZE_MASK;
            
            if (res <= 0) { // 0: 대상 행 없음, 음수: 내부 실행 실패
               cache_inventory->Rollback(shardIndex, key, command->generation);
               Core::errorLogger->LogInfo("cache flush", "DB write failed case 6", "char_id", key.characterID);
               return;
            }

            CrashPoint("CLAIM"); // 인벤토리 blob durable 이후, outbox CLAIM 전

            // blob에서 역직렬화 해서 eventRing 추출
            const InventoryData* inv = reinterpret_cast<const InventoryData*>(param0.data());

            uint8_t pendingCnt = (inv->tail - inv->head) & RING_SIZE_MASK;
            if (pendingCnt == 0 && inv->lastOp)
                pendingCnt = RING_SIZE; // full

            bool retryClaim = false;
            if (pendingCnt > 0) {
                auto bazaarConn = connectionPoolBazaar->AcquireScoped();
                if (bazaarConn == nullptr) {
                    // claim은 멱등 — rollback 없이 스킵, 다음 flush/재접속이 수렴시킴
                    retryClaim = true;
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
                    bazaarConn.reset();
                    retryClaim = claimedCnt < pendingCnt;

                    // head 전진(dirty 재마킹) 후에 WriteDone을 호출해야 entry가 erase되지 않고
                    // 전진된 head가 다음 flush로 영속된다
                    if (claimedCnt > 0) {
                        if(!cache_inventory->AdvanceInboxHead(shardIndex, key, inv->head, claimedCnt))
                            Core::gameLogger->LogInfo("cache flush", "inbox advance failed", "char_id", key.characterID);
                    }
                }
            }
            cache_inventory->WriteDone(shardIndex, key, command->generation, retryClaim);
            if (m_invFlushedFn)
                m_invFlushedFn(key.characterID, inv->lastLsn);
            break;
        }
        case 8: {
            auto conn = connectionPoolGame->AcquireScoped();
            if (conn == nullptr) {
                Key7 key;
                key.characterID = std::any_cast<uint64_t&>(command->params[1]);
                auto shardIndex = key.characterID & SHARD_SIZE_MASK;
                cache_currency->Rollback(shardIndex, key, command->generation);
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
                cache_currency->Rollback(shardIndex, key, command->generation);
                return;
            }

            conn.reset();
            Key7 key;
            key.characterID = param1;
            auto shardIndex = param1 & SHARD_SIZE_MASK;

            if (res <= 0) { // 0: 대상 행 없음, 음수: 내부 실행 실패
                cache_currency->Rollback(shardIndex, key, command->generation);
                Core::errorLogger->LogInfo("cache flush", "DB write failed case 8", "char_id", key.characterID);
                return;
            }
            cache_currency->WriteDone(shardIndex, key, command->generation);
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
            m_cv.wait(lock,[&] {return !m_running || !m_flushQ->empty();});
            if (!m_running.load(std::memory_order_relaxed))
                break;
            std::unique_ptr<FlushCommand> work;
            while (m_flushQ->pop(work)) {
                ++m_inFlight;
                lock.unlock();
                ExecuteWrite(work.get());
                lock.lock();
                --m_inFlight;
                // DBWrite 중 Rollback이 재enqueue할 수 있으므로 큐와 in-flight를 함께 확인
                if (m_flushQ->empty() && m_inFlight == 0)
                    m_drainCv.notify_all();
            }
        }
        Core::sysLogger->LogInfo("cache flush", "flush thread stopped", "threadID", ss.str());
    }

    void CacheFlush::WaitUntilDrained() {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!m_running.load(std::memory_order_relaxed))
            return; // 워커가 이미 종료됨 — Stop()의 최종 드레인이 처리한다
        // 마지막 dirty snapshot을 발행하기 전에 기존 flush 작업의 완료를 확인한다.
        // DB 응답이 지연되면 종료도 대기한다.
        m_drainCv.wait(lock, [&] { return m_flushQ->empty() && m_inFlight == 0; });
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

        std::unique_ptr<FlushCommand> work;
        while (m_flushQ->pop(work)) {
            ExecuteWrite(work.get());
        }
    }

    FlushEnqueueResult CacheFlush::EnqueueFlushQ(std::unique_ptr<FlushCommand> command) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_flushQ->push(std::move(command))) {
                m_cv.notify_one();
                return FlushEnqueueResult::Queued;
            }
        }
        if (command != nullptr) Core::errorLogger->LogError("cache flush", "queue full", "stmtID", command->stmtID);
        return FlushEnqueueResult::Failed;
    }
}
