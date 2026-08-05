#pragma once

#include <thread>
#include <condition_variable>
#include <memory>
#include <chrono>
#include <algorithm>

#include <CoreLib/LoggerGlobal.h>
#include "CacheStorageInventory.h"
#include "CacheStorageCurrency.h"
#include "CacheFlush.h"

#undef min  
#undef max   
namespace Cache {
    class FlushDispatcher {
        CacheFlush* flush = nullptr;
        CacheStorageInventory* cache_inventory = nullptr;
        CacheStorageCurrency* cache_currency = nullptr;
        std::thread m_thread;
        std::atomic<bool> m_running = false;
        uint64_t m_minTime; // ms

        bool DirtyFlush5(const Key5& key, Result5& res) {
            if (m_running.load(std::memory_order_relaxed) && res.lastModified + 30000 > CacheTimer::GetTimeMS()) {
                m_minTime = std::min(m_minTime, res.lastModified + 30000);
                return false;
            }
            flush->EnqueueFlushQ(std::move(CacheStorageInventory::GetFlushCommand(key, res)));
            return true;
        }

        bool DirtyFlush7(const Key7& key, Result7& res) {
            if (m_running.load(std::memory_order_relaxed) && res.lastModified + 30000 > CacheTimer::GetTimeMS()) {
                m_minTime = std::min(m_minTime, res.lastModified + 30000);
                return false;
            }
            flush->EnqueueFlushQ(std::move(CacheStorageCurrency::GetFlushCommand(key, res)));
            return true;
        }

        void ThreadFunc() {
            auto tid = std::this_thread::get_id();
            std::stringstream ss;
            ss << tid;
            Core::sysLogger->LogInfo("cache flush dispatcher", "dispatcher thread started", "threadID", ss.str());
            while (m_running.load(std::memory_order_relaxed)) {
                m_minTime = CacheTimer::GetTimeMS() + 30000;
                cache_inventory->ForEachDirty([this](const Key5& key, Result5& res) { return DirtyFlush5(key, res); });
                cache_currency->ForEachDirty([this](const Key7& key, Result7& res) { return DirtyFlush7(key, res); });
                uint64_t now = CacheTimer::GetTimeMS();
                uint64_t sleepDuration = (m_minTime > now) ? (m_minTime - now) : 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(sleepDuration));
            }
            Core::sysLogger->LogInfo("cache flush dispatcher", "dispatcher thread stopped", "threadID", ss.str());
        }

        void Initialize(CacheFlush* f, CacheStorageInventory* s, CacheStorageCurrency* c) {
            flush = f;
            cache_inventory = s;
            cache_inventory->SetFlushFn(
                [this](const Key5& key, Result5& res) 
                { 
                    flush->EnqueueFlushQ(std::move(CacheStorageInventory::GetFlushCommand(key, res)));
                }
            );
            cache_currency = c;
            cache_currency->SetFlushFn(
                [this](const Key7& key, Result7& res)
                {
                    flush->EnqueueFlushQ(std::move(CacheStorageCurrency::GetFlushCommand(key, res)));
                }
            );
            m_running.store(true, std::memory_order_relaxed);
            m_thread = std::thread(&FlushDispatcher::ThreadFunc, this);
        }
        bool IsReady() {
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
            if (!m_running.exchange(false, std::memory_order_relaxed))
                return;
            if (m_thread.joinable())
                m_thread.join();

            // 최종 dispatch 전에 큐를 비운다.
            // m_running=false라 아래 ForEachDirty는 30초 게이트를 단락 평가로 통과해
            // 모든 dirty entry를 즉시 enqueue한다. 이미 dispatch되어 큐에 남아있는
            // 옛 스냅샷과 겹치면 두 flush 스레드가 같은 key를 동시에 써서
            // 오래된 blob이 최신 blob을 덮을 수 있다.
            flush->WaitUntilDrained();

            cache_inventory->ForEachDirty([this](auto& key, auto& res) { return  DirtyFlush5(key, res); });
            cache_currency->ForEachDirty([this](auto& key, auto& res) { return  DirtyFlush7(key, res); });
        }

        ~FlushDispatcher() {
            Stop();
        }
        friend class Initializer;
    };
}
