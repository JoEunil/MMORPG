#pragma once

#include <atomic>
#include <cstdint>
#include <thread>

#include "LockFreeQueue.h"
#include "DrainableSemaphore.h"

// Vyukov bounded MPMC queue와 semaphore를 결합한 blocking queue 래퍼.
// 종료 시 남은 작업 drain을 위해 DrainableSemaphore을 사용한다.
// 
// Stop() 호출 이후 push는 queue에는 들어가지만 semaphore에서는 차단되어, pop되지 않는다. 


namespace Base {
	template <typename T, size_t Size, bool Drain>
	class BlockingQueue {
		LockFreeQueue<T, Size> m_queue;
		DrainableSemaphore<Drain> m_semaphore;
	public:
		BlockingQueue() : m_queue(), m_semaphore() {}
		bool push(T& item) {
			if (!m_queue.push(item)) {
				return false;
			}
			m_semaphore.Release();
			return true;
		}
		bool push(T&& item) {
			return push(item);
		}
		bool pop(T& item) {
			if (!m_semaphore.Acquire())
				return false;
			while (!m_queue.pop(item)) {
				std::this_thread::yield();
			}
			return true;
		}
		void Stop() {
			m_semaphore.Close();
		}
	};
}