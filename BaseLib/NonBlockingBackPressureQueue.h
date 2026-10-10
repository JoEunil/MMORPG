#pragma once

#include <atomic>
#include <cstddef>
#include <functional>

#include "BackPressurePolicy.h"
#include "LockFreeQueue.h"

namespace Base {
	template <typename T, size_t Size, size_t DeferSize>
	class NonBlockingBackPressureQueue {
		static_assert(Size >= 2 && (Size & (Size - 1)) == 0,
			"NonBlockingBackPressureQueue: Size must be a power of 2 and at least 2");
		static_assert(DeferSize >= 2 && (DeferSize & (DeferSize - 1)) == 0,
			"NonBlockingBackPressureQueue: DeferSize must be a power of 2 and at least 2");

		LockFreeQueue<T, Size> m_queue;
		LockFreeQueue<T, DeferSize> m_deferQueue;
		std::function<bool(const T&)> m_durableLog;
		std::atomic<bool> m_degraded{ false };

	public:
		bool Enqueue(T&& item, Priority priority) {
			return Enqueue(item, priority);
		}

		bool Enqueue(T& item, Priority priority) {
			if (!m_degraded.load(std::memory_order_relaxed)) {
				if (m_queue.push(item)) {
					return true;
				}
				m_degraded.store(true, std::memory_order_relaxed);
			}

			if (priority == Priority::Droppable) {
				return false;
			}
			if (!m_deferQueue.push(item)) {
				if (m_durableLog)
					m_durableLog(item);
				return false;
			}
			return true;
		}

		// 전체 FIFO는 보장하지 않는다.
		bool Dequeue(T& out) {
			if (m_degraded.load(std::memory_order_relaxed)) {
				if (m_queue.pop(out)) {
					return true;
				}
				if (m_deferQueue.pop(out)) {
					return true;
				}

				m_degraded.store(false, std::memory_order_relaxed);
				return false;
			}

			// 복구 전환과 concurrent producer의 defer push는 직렬화되지 않는다.
			// 전환 직후 늦게 publish된 defer 작업을 먼저 회수한다.
			if (m_deferQueue.pop(out)) {
				return true;
			}

			return m_queue.pop(out);
		}
	};
}
