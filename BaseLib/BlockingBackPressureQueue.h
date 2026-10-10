#pragma once

#include <cstddef>
#include <functional>
#include <thread>
#include <utility>

#include "BackPressurePolicy.h"
#include "LockFreeQueue.h"
#include "DrainableSemaphore.h"

namespace Base
{
	// 별도 degraded 상태를 추적하지 않는다.
	// Primary queue가 full인 순간의 중요 작업만 defer queue에 넣는다.
	// Important 작업의 처리 우선권을 보장하기 위해 defer queue를 먼저 소비하며,
	// 따라서 전체 FIFO는 보장하지 않는다.
	template <typename T, size_t Size, size_t DeferSize, bool Drain>
	class BlockingBackPressureQueue {
		static_assert(Size >= 2 && (Size & (Size - 1)) == 0,
			"BlockingBackPressureQueue: Size must be a power of 2 and at least 2");
		static_assert(DeferSize >= 2 && (DeferSize & (DeferSize - 1)) == 0,
			"BlockingBackPressureQueue: DeferSize must be a power of 2 and at least 2");

		LockFreeQueue<T, Size> m_queue;
		LockFreeQueue<T, DeferSize> m_deferQueue;
		DrainableSemaphore<Drain> m_semaphore;
		std::function<bool(const T&)> m_durableLog;

	public:
		explicit BlockingBackPressureQueue(std::function<bool(const T&)> durableLog = {})
			: m_durableLog(std::move(durableLog)) {
		}

		void Stop() {
			m_semaphore.Close();
		}

		bool Enqueue(T&& item, Priority priority) {
			return Enqueue(item, priority);
		}

		bool Enqueue(T& item, Priority priority) {
			if (m_queue.push(item)) {
				m_semaphore.Release();
				return true;
			}

			if (priority == Priority::Droppable) {
				return false;
			}
			if (!m_deferQueue.push(item)) {
				return m_durableLog && m_durableLog(item);
			}
			m_semaphore.Release();
			return true;
		}

		// 전체 FIFO는 보장하지 않는다.
		bool Dequeue(T& out) {
			if (!m_semaphore.Acquire())
				return false;
			while (true) 
			{
				if (m_deferQueue.pop(out)) {
					break;
				}

				if (m_queue.pop(out)) {
					break;
				}
				std::this_thread::yield();
			}
			return true;
		}
	};
}
