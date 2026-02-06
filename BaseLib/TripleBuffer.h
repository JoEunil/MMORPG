#pragma once
#include <atomic>
#include <algorithm>


namespace Base {
	// SPMC Lock Free 버퍼
	template <typename T>
	class TripleBuffer {
		T* back;
		std::atomic<uint8_t> dirty; // 0: readable, 1: writing , 2: dirty
	public:
		void Init(T* b) {
			back = b;
		}
		void Write(T* write) {
			dirty.store(1, std::memory_order_acquire); // 이전 swap 작업 완료
			std::swap(back, write);
			dirty.store(0, std::memory_order_release);
		}
		void Read(T* read)) {
			if (dirty.compare_exchange_strong(0, 2, std::memory_order_acq, std::memory_order_relaxed )) {
				std::swap(back, read);
				dirty.store(2, std::memory_order_release);
			}
		}
	};
}