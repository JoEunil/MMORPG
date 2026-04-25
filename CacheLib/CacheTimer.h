#pragma once
#include <chrono>
#include <thread>
#include <atomic>
#include <CoreLib/LoggerGlobal.h>

namespace Cache {
	class CacheTimer {
		inline static std::atomic<uint64_t> m_timeCache;
		inline static std::thread m_thread;
		inline static std::atomic<bool> m_running;
		static void StartThread() {
			if (m_running.load(std::memory_order_relaxed)) {
				return;
			}
			m_running.store(true, std::memory_order_relaxed);
			m_thread = std::thread(ThreadFunc);
		}
		static void ThreadFunc() {
			auto tid = std::this_thread::get_id();
			std::stringstream ss;
			ss << tid;
			Core::sysLogger->LogInfo("cache timer", "Cache timer thread started", "threadID", ss.str());
			while (m_running.load(std::memory_order_relaxed)) {
				auto now = std::chrono::steady_clock::now();
				uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
				m_timeCache.store(ms, std::memory_order_relaxed);
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			Core::sysLogger->LogInfo("cache timer", "Cache timer thread stopped", "threadID", ss.str());
		}
		static void Stop() {
			bool expected = true;
			if (!m_running.compare_exchange_strong(expected, false,
				std::memory_order_relaxed)) {
				return;  
			}

			if (m_thread.joinable())
				m_thread.join();
		}
		friend class Initializer;
	public:
		static uint64_t GetTimeMS() {
			return m_timeCache.load(std::memory_order_relaxed);
		}
	};
}