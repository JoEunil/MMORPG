#pragma once
#include <atomic>


namespace Base {
	// closing 상태, publish가 끝난 permit을 하나의 protocol로 관리한다.
	// Drain 정책은 close 전에 수락한 작업을 모두 소비한 뒤 종료하고,
	// HardStop 정책은 close를 관측하면 남은 작업을 처리하지 않고 종료한다.
	// Drain 사용 전제:
	// 모든 producer의 push 호출이 끝난 뒤 Stop을 호출해야 한다.
	// Stop과 push의 동시 실행은 지원하지 않는다.

	constexpr uint64_t CLOSING_FLAG = 1ULL << 63;
	constexpr uint64_t COUNT_MASK = (1ULL << 63) - 1;
	template <bool Drain>
	class DrainableSemaphore
	{
		std::atomic<uint64_t> m_semaphore{ 0 }; // 상위 1비트: Closing flag, 하위 63비트: permit count
	public:
		void Close() {
			while (true)
			{
				uint64_t oldValue = m_semaphore.load(std::memory_order_relaxed);
				if ((oldValue & CLOSING_FLAG) > 0) {
					return;
				}
				if (m_semaphore.compare_exchange_weak(oldValue, oldValue | CLOSING_FLAG, std::memory_order_release, std::memory_order_relaxed)) {
					m_semaphore.notify_all(); 
					return;
				}
			}

		}

		void Release(uint64_t count) {
			while (true) 
			{
				uint64_t oldValue = m_semaphore.load(std::memory_order_acquire);

				if (count > COUNT_MASK - (oldValue & COUNT_MASK))
					return; // overflow 방지

				if (CLOSING_FLAG & oldValue) {
					return; // 신규 유입 차단.
				}
				if (m_semaphore.compare_exchange_weak(oldValue, oldValue + count, std::memory_order_release, std::memory_order_relaxed)) {
					if (count == 1) {
						m_semaphore.notify_one();
					}
					else {
						m_semaphore.notify_all();
					}
					return;
				}
			}
		}

		void Release() {
			Release(1);
		}
		// HardStop:
		// Close 이후에는 새로운 permit을 획득하지 않는다.
		// Close 전에 permit을 획득한 consumer는 현재 작업을 완료할 수 있다.
		bool Acquire() {
			while (true)
			{
				m_semaphore.wait(0, std::memory_order_acquire); // permit이 0이면 block (C++20 이상)
				uint64_t oldValue = m_semaphore.load(std::memory_order_acquire);
				if ((oldValue & COUNT_MASK) > 0) {
					if (!Drain && oldValue & CLOSING_FLAG) {
						return false; // hard stop 
					}
					if (m_semaphore.compare_exchange_weak(oldValue, oldValue - 1, std::memory_order_acquire, std::memory_order_relaxed)) {
						return true; // permit 획득 성공
					}
				}
				else if (oldValue & CLOSING_FLAG) {
					return false;
				}
			}
		}	
	};
}