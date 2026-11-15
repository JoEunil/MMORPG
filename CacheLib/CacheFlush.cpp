#include "CacheFlush.h"

#include "DBConnectionPool.h"

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
                if (command->stmtID == StatementId::UpdateInventory)
                    cache_inventory->Rollback(*id & SHARD_SIZE_MASK, InventoryKey{ *id }, command->generation);
                else if (command->stmtID == StatementId::UpsertCurrency)
                    cache_currency->Rollback(*id & SHARD_SIZE_MASK, CurrencyKey{ *id }, command->generation);
            }
        }
    }

    void CacheFlush::DBWrite(FlushCommand* command) {
        switch (command->stmtID) {
        case StatementId::UpdateInventory:
            FlushInventory(command);
            break;
        case StatementId::UpsertCurrency:
            FlushCurrency(command);
            break;
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
        if (command != nullptr) Core::errorLogger->LogError("cache flush", "queue full", "stmtID", static_cast<uint16_t>(command->stmtID));
        return FlushEnqueueResult::Failed;
    }
}
