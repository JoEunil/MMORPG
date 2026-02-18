#pragma once
#include <chrono>
#include <thread>
#include <atomic>
#include <CoreLib/LoggerGlobal.h>

namespace Net {
	class NetTimer {
		// std::chrono::steady_clock::now() 호출 비용이 커서 전역으로 캐시해서 사용할 수 있도록, 전용 스레드에서 업데이트.
		inline static std::atomic<uint64_t> m_timeCache;
		// steady_clock::time_point는
		// trivially copyable이 표준상 보장되지 않고
		// 크기 및 내부 표현이 구현체 의존적이어서
		// std::atomic<T>로 사용하기에 부적합하다.
		// trivially copyable: 복사/이동/소멸 시 사용자 정의 동작이 없는 타입
		// data tearing 방지하기 위해 atomic 적용.
		inline static std::thread m_thread;
		inline static std::atomic<bool> m_running;
		static void StartThread() {
			m_thread = std::thread(ThreadFunc);
		}
		static void ThreadFunc() {
			m_running.store(true, std::memory_order_relaxed);
			auto tid = std::this_thread::get_id();
			std::stringstream ss;
			ss << tid;
			Core::sysLogger->LogInfo("net timer", "Net timer thread started", "threadID", ss.str());
			while (m_running.load()) {
				auto now = std::chrono::steady_clock::now();
				uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
				// 1ms 단위로 갱신하기 때문에 nano second 단위의 정밀성 필요 없음.
				// rtt 측정하는 데에는 milisecond도 충분
				m_timeCache.store(ms, std::memory_order_relaxed);
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
		static void Stop() {
			m_running.store(false, std::memory_order_relaxed);
			if (m_thread.joinable())	
				m_thread.join();
			Core::sysLogger->LogInfo("net timer", "Net timer thread stopped");
		}
		friend class Initializer;
	public:
		static uint64_t GetTimeMS() {
			return m_timeCache.load(std::memory_order_relaxed);
		}
	};
}