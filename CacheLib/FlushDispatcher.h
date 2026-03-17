#pragma once

#include <thread>
#include <condition_variable>
#include <memory>
#include <chrono>
#include <algorithm>

#include <CoreLib/LoggerGlobal.h>
#include "CacheStorage5.h"
#include "CacheFlush.h"

#undef min  
#undef max   
namespace Cache {
    class FlushDispatcher {
        CacheFlush* flush;
        CacheStorage5* cache_5;
        std::thread m_thread;
        std::atomic<bool> m_running = false;
        uint64_t m_minTime; // ms

        bool DirtyFlush5(const Key5& key, Result5& res) {
            if (m_running.load() && res.lastModified + 30000 > CacheTimer::GetTimeMS()) {
                m_minTime = std::min(m_minTime, res.lastModified + 30000);
                return false;
            }
            flush->EnqueueFlushQ(std::move(CacheStorage5::GetFlushCommand(key, res)));
            return true;
        }

        void ThreadFunc() {
            auto tid = std::this_thread::get_id();
            std::stringstream ss;
            ss << tid;
            Core::sysLogger->LogInfo("cache flush dispatcher", "dispatcher thread started", "threadID", ss.str());
            while (m_running.load(std::memory_order_relaxed)) {
                m_minTime = CacheTimer::GetTimeMS() + 30000;
                cache_5->ForEachDirty([this](const Key5& key, Result5& res) { return DirtyFlush5(key, res); });
                uint64_t now = CacheTimer::GetTimeMS();
                uint64_t sleepDuration = (m_minTime > now) ? (m_minTime - now) : 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(sleepDuration));
            }
            Core::sysLogger->LogInfo("cache flush dispatcher", "dispatcher thread stopped", "threadID", ss.str());
        }

        void Initialize(CacheFlush* f, CacheStorage5* s) {
            flush = f;
            cache_5 = s;
            cache_5->SetFlushFn(
                [this](const Key5& key, Result5& res) 
                { 
                    flush->EnqueueFlushQ(std::move(CacheStorage5::GetFlushCommand(key, res)));
                }
            );
            m_running.store(true);
            m_thread = std::thread(&FlushDispatcher::ThreadFunc, this);
        }
        bool IsReady() {
            if (!m_running.load()) {
                Core::sysLogger->LogError("cache flush dispatcher", "not running");
                return false;
            }
            if (flush == nullptr) {
                Core::sysLogger->LogError("cache flush dispatcher", "flush not initialized");
                return false;
            } 
            if (cache_5 == nullptr) {
                Core::sysLogger->LogError("cache flush dispatcher", "cache_5 not initialized");
                return false;
            }
            return true;
        }
        void Stop() {
            m_running.store(false);
            if (m_thread.joinable())
                m_thread.join();
            cache_5->ForEachDirty([this](auto& key, auto& res) { return  DirtyFlush5(key, res); });
        }

        ~FlushDispatcher() {
            Stop();
        }
        friend class Initializer;
    };
}
