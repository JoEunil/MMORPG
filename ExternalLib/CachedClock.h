#pragma once

#include <atomic>
#include <chrono>
#include <thread>
#include <CoreLib/ICachedClock.h>

namespace External {
    class CachedClock final : public Core::ICachedClock {
        std::atomic<uint64_t> m_timeCache;
        std::atomic<bool> m_running{ true };
        std::thread m_thread;

        static uint64_t NowMS() {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        }

    public:
        CachedClock() : m_timeCache(NowMS()), m_thread([this] {
            while (m_running.load(std::memory_order_relaxed)) {
                m_timeCache.store(NowMS(), std::memory_order_relaxed);
                // Scheduling can make the update interval longer than 1ms.
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }) {}

        ~CachedClock() override { Stop(); }

        void Stop() {
            m_running.store(false, std::memory_order_relaxed);
            if (m_thread.joinable())
                m_thread.join();
        }

        CachedClock(const CachedClock&) = delete;
        CachedClock& operator=(const CachedClock&) = delete;
        CachedClock(CachedClock&&) = delete;
        CachedClock& operator=(CachedClock&&) = delete;

        uint64_t GetTimeMS() const override {
            return m_timeCache.load(std::memory_order_relaxed);
        }
    };
}
