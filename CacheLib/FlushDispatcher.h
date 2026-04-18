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
        CacheFlush* flush;
        CacheStorageInventory* cache_inventory;
        CacheStorageCurrency* cache_currency;
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
            m_running.store(false, std::memory_order_relaxed);
            if (m_thread.joinable())
                m_thread.join();
            cache_inventory->ForEachDirty([this](auto& key, auto& res) { return  DirtyFlush5(key, res); });
            cache_currency->ForEachDirty([this](auto& key, auto& res) { return  DirtyFlush7(key, res); });
        }

        ~FlushDispatcher() {
            Stop();
        }
        friend class Initializer;
    };
}
