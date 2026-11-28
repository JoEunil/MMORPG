#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "BackPressurePolicy.h"
#include "MPMCQueue.h"

namespace Base {
	template <typename T, size_t Size, size_t DeferSize>
	class NonBlockingBackPressureQueue {
		static_assert(Size >= 2 && (Size & (Size - 1)) == 0,
			"NonBlockingBackPressureQueue: Size must be a power of 2 and at least 2");
		static_assert(DeferSize == 0 || (DeferSize >= 2 && (DeferSize & (DeferSize - 1)) == 0),
			"NonBlockingBackPressureQueue: DeferSize must be 0 (dynamic) or a power of 2 and at least 2");

		// DeferSize == 0 preserves Important work dynamically. Shared ownership makes
		// concurrent pops safe without exposing node lifetime management to callers.
		// Dynamic defer is a stack; FIFO order is not guaranteed.
		class DynamicDeferQueue {
			struct Node {
				T item;
				std::atomic<std::shared_ptr<Node>> next;
				explicit Node(T& value) : item(std::move(value)) {}
			};
			std::atomic<std::shared_ptr<Node>> m_head;
		public:
			~DynamicDeferQueue() {
				// Producers/consumers must already be stopped, as with the primary queue.
				auto node = m_head.exchange(nullptr);
				while (node) node = node->next.exchange(nullptr);
			}
			bool push(T& item) {
				std::shared_ptr<Node> node;
				try { node = std::make_shared<Node>(item); }
				catch (const std::bad_alloc&) { return false; }
				auto next = m_head.load(std::memory_order_acquire);
				do { node->next.store(next, std::memory_order_relaxed); }
				while (!m_head.compare_exchange_weak(next, node,
					std::memory_order_acq_rel, std::memory_order_acquire));
				return true;
			}
			bool pop(T& out) {
				auto node = m_head.load(std::memory_order_acquire);
				while (node) {
					auto next = node->next.load(std::memory_order_acquire);
					if (m_head.compare_exchange_weak(node, next,
						std::memory_order_acq_rel, std::memory_order_acquire)) {
						// A stalled contender must not retain an entire popped chain.
						node->next.store(nullptr, std::memory_order_release);
						out = std::move(node->item);
						return true;
					}
				}
				return false;
			}
		};

		MPMCQueue<T, Size> m_queue;
		std::conditional_t<DeferSize == 0, DynamicDeferQueue, MPMCQueue<T, DeferSize>> m_deferQueue;
		std::function<bool(const T&)> m_durableLog;
		std::atomic<bool> m_degraded{ false };

	public:
		void SetDurableLog(std::function<bool(const T&)> durableLog) { m_durableLog = std::move(durableLog); }

		// Use primary capacity first; preserve this work in defer under congestion.
		EnqueueResult EnqueueDeferred(T& item) { return Enqueue(item, Priority::Important); }
		EnqueueResult EnqueueDeferred(T&& item) { return Enqueue(item, Priority::Important); }

		EnqueueResult Enqueue(T&& item, Priority priority = Priority::Droppable) {
			return Enqueue(item, priority);
		}

		EnqueueResult Enqueue(T& item, Priority priority = Priority::Droppable) {
			if (!m_degraded.load(std::memory_order_relaxed)) {
				if (m_queue.push(item)) {
					return EnqueueResult::Queued;
				}
				m_degraded.store(true, std::memory_order_relaxed);
			}

			if (priority == Priority::Droppable) {
				return EnqueueResult::Rejected;
			}
			if (!m_deferQueue.push(item)) {
				return m_durableLog && m_durableLog(item)
					? EnqueueResult::RecordedForRecovery : EnqueueResult::Rejected;
			}
			return EnqueueResult::Queued;
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
