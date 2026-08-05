#pragma once
#include <chrono>
#include <thread>
#include <atomic>
#include <CoreLib/LoggerGlobal.h>

class NetTimerTest;
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
			m_running.store(true, std::memory_order_relaxed);
			m_thread = std::thread(ThreadFunc);
		}
		static void ThreadFunc() {
			auto tid = std::this_thread::get_id();
			std::stringstream ss;
			ss << tid;
			Core::sysLogger->LogInfo("net timer", "Net timer thread started", "threadID", ss.str());
			while (m_running.load(std::memory_order_relaxed)) {
				auto now = std::chrono::steady_clock::now();
				uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
				// ms 단위로 캐시한다. nanosecond 정밀도는 아래 갱신 주기 한계상 의미가 없다.
				m_timeCache.store(ms, std::memory_order_relaxed);
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				// 실제 갱신 주기는 Windows 타이머 해상도(기본 약 15.6ms)에 종속된다.
				// sleep_for(1ms)는 다음 클럭 틱까지 자므로 이 루프는 ~64Hz로 돈다.
				// 따라서 GetTimeMS()는 최대 ~15.6ms 낡은 값이고, RTT처럼 두 번 읽는
				// 용도에서는 오차가 ~31ms까지 벌어진다 (UnitTests/NetLib/NetTimer.cpp 참고).
				// 현재 RTT는 로깅·클라이언트 표시용이라 이 정밀도로 충분하다.
				// 되감기(lag compensation) 도입 시에는 CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
				// 기반 대기로 교체해야 한다 — timeBeginPeriod와 달리 전역 클럭 주기를
				// 바꾸지 않아 다른 프로세스에 비용을 전가하지 않는다.
			}
		}
		static void Stop() {
			m_running.store(false, std::memory_order_relaxed);
			if (m_thread.joinable())	
				m_thread.join();
			Core::sysLogger->LogInfo("net timer", "Net timer thread stopped");
		}
		friend class Initializer;
		friend class ::NetTimerTest;
	public:
		static uint64_t GetTimeMS() {
			return m_timeCache.load(std::memory_order_relaxed);
		}
	};
}