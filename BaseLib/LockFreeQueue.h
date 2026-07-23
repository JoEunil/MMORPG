#pragma once
#include <array>
#include <cstdint>
#include <atomic>
#include <cassert>
#include <new>

#include "Exception.h"

namespace Base {
	// Dmitry Vyukov의 lock free queue 
	// MPMC(multi produce multi consumer)에서 race condition 해결한 방법
	// 각 Slot에 번호(seq)를 매기고 상태 표현 → seq가 유일한 상태 값이 되어 race condition이 발생하지 않음.
	// back-off 정책은 호출자에서 구현. 

	template<typename T, size_t QSize>
	class LockFreeQueue {
		struct alignas(std::hardware_destructive_interference_size) Cell {
			std::atomic<uint64_t> seq;
			T data;
		};
		std::unique_ptr <Cell[]> m_queue;
		// vector로 사용 가능함, 고정크기 + 재할당 방지 목적으로 array가 적합.

		alignas(std::hardware_destructive_interference_size) std::atomic<uint64_t> m_head;
		alignas(std::hardware_destructive_interference_size) std::atomic<uint64_t> m_tail;
		char padding[std::hardware_destructive_interference_size - sizeof(uint64_t)];

		size_t m_mask;

	public:
		// Q 사이즈는 2의 거듭제곱이어야 한다.
		// 모듈러 연산을 & 연산한번으로 하기 위함
		LockFreeQueue() : m_mask(QSize-1) {
			assert((QSize >= 2) &&((QSize & (QSize - 1)) == 0));
			// 2 거듭제곱 체크
			m_head.store(0, std::memory_order_relaxed);
			m_tail.store(0, std::memory_order_relaxed);
			m_queue = std::make_unique<Cell[]>(QSize);
			for (int i = 0; i < QSize; i++)
			{
				m_queue[i].seq.store(i, std::memory_order_relaxed);
			}
		}
		bool push(const T& data) {
			while (true)
			{
 				auto tail = m_tail.load(std::memory_order_relaxed);
				auto idx = tail & m_mask;
				auto seq = m_queue[idx].seq.load(std::memory_order_acquire);
				int64_t diff = static_cast<int64_t>(seq) - static_cast<int64_t>(tail);

				if (diff == 0) {
					if (m_tail.compare_exchange_weak(tail, tail +1, std::memory_order_acq_rel, std::memory_order_relaxed)) {
						// true 인 경우 tail 점유 성공, CAS로 race condition 해결 
						m_queue[idx].data = data;
						m_queue[idx].seq.store(tail + 1, std::memory_order_release); // 순서 바뀌면 안됨, release로 data가 write된 상태 관측 보장.
						return true;
					}
					continue;
				}

				if (diff < 0) {
					// full
					return false;
				}
			}
			// critical error, 논리적으로 도달할 수없음.
			unreachable();
			return false;
		}

		bool pop(T& out) {
			while (true)
			{
				auto head = m_head.load(std::memory_order_relaxed);
				auto idx = head & m_mask;
				auto seq = m_queue[idx].seq.load(std::memory_order_acquire);
				int64_t diff = static_cast<int64_t>(seq) - static_cast<int64_t>(head + 1);
				if (diff == 0) {
					if (m_head.compare_exchange_weak(head, head + 1, std::memory_order_acq_rel, std::memory_order_relaxed)) {
						auto res = m_queue[idx].data;
						m_queue[idx].seq.store(head + QSize, std::memory_order_release);
						out = res;
						return true;
					}
					continue;
				}

				if (diff < 0) {
					// empty
					return false;
				}
			}
			// critical error
			unreachable();
			return false;
		}
	};
}