#pragma once
#include <atomic>
#include <algorithm>


namespace Base {
	// MPMC Lock Free 버퍼
	template <typename T>
	class TripleBuffer {
		T* back;
		std::atomic<uint8_t> dirty; // 0: readable, 1: swap , 2: dirty
	public:
		void Init(T* b) {
			back = b;
		}
		void Write(T* write) {
			while (true) {
				uint8_t exp = dirty.load(std::memory_order_relaxed);
				if (exp == 1) 
					continue;
				if (dirty.compare_exchange_weak(exp, 1, std::memory_order_acquire, std::memory_order_relaxed))
					break;
			}
			std::swap(back, write);
			dirty.store(0, std::memory_order_release);
		}
		bool Read(T* read) {
			uint8_t exp = 0;
			if (!dirty.compare_exchange_strong(exp, 1,
				std::memory_order_acquire, std::memory_order_relaxed)) {
				return false; 
			}
			std::swap(back, read);
			dirty.store(2, std::memory_order_release);  
			return true;
		}

	};
}