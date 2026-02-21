#pragma once
#include <atomic>

namespace Base{
	class SpinLockGuard {
		// lock_guard 처럼 쓸수 있게 RAII 적용
		std::atomic_flag& lock; // lock 용도만을 위해 존재하는 atomic 타입, 내부적으로 boolean 타입 (false는 unlocked, true는 locked)
	public:
		SpinLockGuard(std::atomic_flag& lo) : lock(lo)
		{
			while (lock.test_and_set(std::memory_order_acquire)) {}
			// test_and_set: 1.이전값 반환, 2. 값을 true로 설정 이 두개 명령을 원자적으로 수행
			// 내부적으로 CAS(compare and swap) 계열 명령 사용
		}
		~SpinLockGuard()
		{
			lock.clear(std::memory_order_release);
		}

	};
}