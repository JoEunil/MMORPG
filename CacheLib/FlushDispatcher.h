#pragma once

#include <thread>
#include <condition_variable>
#include <memory>
#include <chrono>
#include <algorithm>

#include <CoreLib/LoggerGlobal.h>
#include <CoreLib/ICachedClock.h>
#include "CacheStorageInventory.h"
#include "CacheStorageCurrency.h"
#include "CacheFlush.h"

#undef min  
#undef max   
namespace Cache {
    class FlushDispatcher {
        Core::ICachedClock* clock = nullptr;
        CacheFlush* flush = nullptr;
        CacheStorageInventory* cache_inventory = nullptr;
        CacheStorageCurrency* cache_currency = nullptr;
        std::thread m_thread;
        std::mutex m_waitMutex;
        std::condition_variable m_wake;
        std::atomic<bool> m_running = false;
        uint64_t m_minTime; // ms

        bool DirtyFlush5(const InventoryKey& key, InventoryCacheItem& res) {
            if (m_running.load(std::memory_order_relaxed) && res.lastModified + 30000 > clock->GetTimeMS()) {
                m_minTime = std::min(m_minTime, res.lastModified + 30000);
                return false;
            }
            return flush->EnqueueFlushQ(std::move(CacheStorageInventory::GetFlushCommand(key, res))) != FlushEnqueueResult::Failed;
        }

        bool DirtyFlush7(const CurrencyKey& key, CurrencyCacheItem& res) {
            if (m_running.load(std::memory_order_relaxed) && res.lastModified + 30000 > clock->GetTimeMS()) {
                m_minTime = std::min(m_minTime, res.lastModified + 30000);
                return false;
            }
            return flush->EnqueueFlushQ(std::move(CacheStorageCurrency::GetFlushCommand(key, res))) != FlushEnqueueResult::Failed;
        }

        void ThreadFunc() {
            auto tid = std::this_thread::get_id();
            std::stringstream ss;
            ss << tid;
            Core::sysLogger->LogInfo("cache flush dispatcher", "dispatcher thread started", "threadID", ss.str());
            while (m_running.load(std::memory_order_relaxed)) {
                m_minTime = clock->GetTimeMS() + 30000;
                cache_inventory->ForEachDirty([this](const InventoryKey& key, InventoryCacheItem& res) { return DirtyFlush5(key, res); });
                cache_currency->ForEachDirty([this](const CurrencyKey& key, CurrencyCacheItem& res) { return DirtyFlush7(key, res); });
                uint64_t now = clock->GetTimeMS();
                uint64_t sleepDuration = (m_minTime > now) ? (m_minTime - now) : 0;
                std::unique_lock lock(m_waitMutex);
                m_wake.wait_for(lock, std::chrono::milliseconds(sleepDuration),
                    [this] { return !m_running.load(std::memory_order_relaxed); });
            }
            Core::sysLogger->LogInfo("cache flush dispatcher", "dispatcher thread stopped", "threadID", ss.str());
        }

        void Initialize(CacheFlush* f, CacheStorageInventory* s, CacheStorageCurrency* c, Core::ICachedClock* cl) {
            clock = cl;
            flush = f;
            cache_inventory = s;
            cache_inventory->SetFlushFn(
                [this](const InventoryKey& key, InventoryCacheItem& res)
                { 
                    return flush->EnqueueFlushQ(std::move(CacheStorageInventory::GetFlushCommand(key, res)));
                }
            );
            cache_currency = c;
            cache_currency->SetFlushFn(
                [this](const CurrencyKey& key, CurrencyCacheItem& res)
                {
                    return flush->EnqueueFlushQ(std::move(CacheStorageCurrency::GetFlushCommand(key, res)));
                }
            );
            m_running.store(true, std::memory_order_relaxed);
            m_thread = std::thread(&FlushDispatcher::ThreadFunc, this);
        }
        bool IsReady() {
            if (clock == nullptr) {
                Core::sysLogger->LogError("cache flush dispatcher", "clock not initialized");
                return false;
            }
            if (!m_running.load(std::memory_order_relaxed)) {
                Core::sysLogger->LogError("cache flush dispatcher", "not running");
                return false;
            }
            if (flush == nullptr) {
                Core::sysLogger->LogError("cache flush dispatcher", "flush not initialized");
                return false;
            } 
            if (cache_inventory == nullptr) {
                Core::sysLogger->LogError("cache flush dispatcher", "cache_inventory not initialized");
                return false;
            }
            if (cache_currency == nullptr) {
                Core::sysLogger->LogError("cache flush dispatcher", "cache_currency not initialized");
                return false;
            }
            return true;
        }
        void Stop() {
            {
                std::lock_guard lock(m_waitMutex);
                if (!m_running.exchange(false, std::memory_order_relaxed))
                    return;
            }
            m_wake.notify_all();
            if (m_thread.joinable())
                m_thread.join();

            // No business producers remain. Drain, then dispatch in batches: the
            // bounded queue may not fit all dirty entries in a single pass.
            // Claim/head updates can also become dirty while a batch is written.
            flush->WaitUntilDrained();
            bool firstPass = true;
            for (;;) {
                size_t queued = 0;
                cache_inventory->ForEachDirty([&](auto& key, auto& res) {
                    if (!firstPass && res.rollbackCnt >= 3) return false;
                    bool accepted = DirtyFlush5(key, res);
                    queued += accepted;
                    return accepted;
                });
                cache_currency->ForEachDirty([&](auto& key, auto& res) {
                    if (!firstPass && res.rollbackCnt >= 3) return false;
                    bool accepted = DirtyFlush7(key, res);
                    queued += accepted;
                    return accepted;
                });
                if (queued == 0)
                    break;
                flush->WaitUntilDrained();
                firstPass = false;
            }
        }

        ~FlushDispatcher() {
            Stop();
        }
        friend class Initializer;
    };
}
