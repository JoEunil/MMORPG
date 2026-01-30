#pragma once
#include <array>
#include <cstdint>
#include <atomic>
#include <cassert>
#include <new>
#include <memory>

#include "Exeption.h"

namespace Base {
	template<typename T, size_t QSize>
	class LockFreeQueueSP {
		// T = shared_ptr<t>

		struct Cell {
			std::atomic<uint64_t> seq;
			T data;
		};
		std::array<Cell, QSize> m_queue alignas(std::hardware_destructive_interference_size);

		std::atomic<uint64_t> m_head alignas(std::hardware_destructive_interference_size);

		std::atomic<uint64_t> m_tail alignas(std::hardware_destructive_interference_size);

		uint16_t m_mask;

	public:
		LockFreeQueueSP() : m_mask(QSize - 1) {
			assert((QSize >= 2) && ((QSize & (QSize - 1)) == 0));
			m_head.store(0);
			m_tail.store(0);
			for (int i = 0; i < QSize; i++)
			{
				m_queue[i].seq.store(i, std::memory_order_relaxed);
			}
		}
		bool push(T data) {
			while (true)
			{
				auto tail = m_tail.load();
				auto idx = tail & m_mask;
				auto seq = m_queue[idx].seq.load(std::memory_order_acquire);
				int64_t diff = static_cast<int64_t>(seq) - static_cast<int64_t>(tail);

				if (diff == 0) {
					if (m_tail.compare_exchange_weak(tail, tail + 1)) {
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

		T pop() {
			while (true)
			{
				auto head = m_head.load();
				auto idx = head & m_mask;
				auto seq = m_queue[idx].seq.load(std::memory_order_acquire);
				int64_t diff = static_cast<int64_t>(seq) - static_cast<int64_t>(head + 1);
				if (diff == 0) {
					if (m_head.compare_exchange_weak(head, head + 1)) {
						auto res = m_queue[idx].data;
						m_queue[idx].seq.store(head + QSize, std::memory_order_release);
						m_queue[idx].data = T{};
						return res;
					}
					continue;
				}

				if (diff < 0) {
					return T{}; // default type으로 변환 여기서는 nullptr
				}
			}
			unreachable();
			return T{};
		}
	};
}