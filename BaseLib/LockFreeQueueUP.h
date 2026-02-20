#pragma once
#include <array>
#include <cstdint>
#include <atomic>
#include <cassert>
#include <new>
#include <memory>

#include "Exeption.h"

namespace Base {

	/*
	std::unique_ptr<T> tmp = std::move(uptr);
	while (tmp) { 
		tmp = queue.push(std::move(tmp)); 
		std::this_thread::yield();  // back-off 정책
	}
	*/
	// 재시도 안한다면 return value 무시하면 됨.
	template<typename T, size_t QSize>
	class LockFreeQueueUP {
		// T = unique_ptr<t>
		struct Cell {
			std::atomic<uint64_t> seq;
			T data;
		};

		std::unique_ptr <Cell[]> m_queue alignas(std::hardware_destructive_interference_size);

		std::atomic<uint64_t> m_head alignas(std::hardware_destructive_interference_size);

		std::atomic<uint64_t> m_tail alignas(std::hardware_destructive_interference_size);

		uint16_t m_mask;

	public:
		LockFreeQueueUP() : m_mask(QSize - 1) {
			assert((QSize >= 2) && ((QSize & (QSize - 1)) == 0));
			m_head.store(0);
			m_tail.store(0);
			m_queue = std::make_unique<Cell[]>(QSize);
			for (int i = 0; i < QSize; i++)
			{
				m_queue[i].seq.store(i, std::memory_order_relaxed);
			}
		}
		T push(T data) {
			while (true)
			{
				auto tail = m_tail.load();
				auto idx = tail & m_mask;
				auto seq = m_queue[idx].seq.load(std::memory_order_acquire);
				int64_t diff = static_cast<int64_t>(seq) - static_cast<int64_t>(tail);

				if (diff == 0) {
					if (m_tail.compare_exchange_weak(tail, tail + 1)) {
						m_queue[idx].data = std::move(data);
						m_queue[idx].seq.store(tail + 1, std::memory_order_release); 
						return T{};
					}
					continue;
				}

				if (diff < 0) {
					return data;
				}
			}
			unreachable();
			return data;
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
						auto res = std::move(m_queue[idx].data);
						m_queue[idx].seq.store(head + QSize, std::memory_order_release);
						return std::move(res);
					}
					continue;
				}

				if (diff < 0) {
					return T{};
				}
			}
			unreachable();
			return T{};
		}
	};
}