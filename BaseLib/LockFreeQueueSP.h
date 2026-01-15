#pragma once
#include <vector>
#include <cstdint>
#include <atomic>
#include <cassert>
#include <memory>

#include "Exeption.h"

namespace Base {
	template<typename T>
	struct Cell {
		std::atomic<uint64_t> seq;
		std::shared_ptr<T> data;
	};
	template<typename T>
	class LockFreeQueueSP {
		alignas(std::hardware_destructive_interference_size);
		std::vector<Cell<T>> m_queue;

		alignas(std::hardware_destructive_interference_size);
		std::atomic<uint64_t> m_head;

		alignas(std::hardware_destructive_interference_size);
		std::atomic<uint64_t> m_tail;

		uint16_t m_capacity;
		uint16_t m_mask;

	public:
		LockFreeQueueSP(uint16_t QSize) : m_capacity(QSize), m_mask(QSize - 1) {
			assert((m_capacity >= 2) && ((m_capacity & (m_capacity - 1)) == 0));

			m_queue.resize(QSize);
			m_capacity = QSize;
			for (int i = 0; i < m_capacity; i++)
			{
				m_queue[i].seq = i;
			}
		}
		bool push(std::shared_ptr<T> data) {
			while (true)
			{
				auto tail = m_tail.load();
				auto idx = tail & m_mask;
				auto seq = m_queue[idx].seq.load(std::memory_order_acquire);
				int64_t diff = static_cast<int64_t>(seq) - static_cast<int64_t>(tail);

				if (diff == 0) {
					if (m_tail.compare_exchange_weak(tail, (tail + 1) & m_mask)) {
						m_queue[idx].data = data;
						m_queue[idx].seq.store(tail + 1, std::memory_order_release); 
						return true;
					}
					continue;
				}

				if (diff < 0) {
					return false;
				}
			}
			unreachable();
			return false;
		}

		bool pop(std::shared_ptr<T>& out) {
			while (true)
			{
				auto head = m_head.load();
				auto idx = head & m_mask;
				auto seq = m_queue[idx].seq.load(std::memory_order_acquire);
				int64_t diff = static_cast<int64_t>(seq) - static_cast<int64_t>(head + 1);
				if (diff == 0) {
					if (m_head.compare_exchange_weak(head, (head + 1) & m_mask)) {
						auto res = m_queue[idx].data;
						m_queue[idx].data.reset(); 
						// 이 부분이 없으면 queue에서 shared_ptr을 소유하게 되어 다음 push로 해당 cell이 변경되기 전까지 메모리 해제가 되지 않음
						m_queue[idx].seq.store(head + m_capacity + 1, std::memory_order_release);
						out = res;
						return true;
					}
					continue;
				}

				if (diff < 0) {
					return false;
				}
			}
			unreachable();
			return false;
		}
	};
}