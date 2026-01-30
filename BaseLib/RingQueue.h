#pragma once
#include <cstdint>
#include <array>

namespace Base {
	// not thread safe, full이 발생하지 않는 상황에서만 사용
	template <typename T, uint32_t SIZE>
	class RingQueue {
		std::array<T, SIZE> queue;
		uint32_t front, rear;
	public:
		RingQueue() : front(0), rear(0) {

		}
		static_assert((SIZE& (SIZE - 1)) == 0, "Ring Queue Size must be power of 2");

		void push(const T& data) {
			queue[rear] = data;
			rear = (rear + 1) & (SIZE - 1);
		}
		T pop() {
			auto temp = front;
			front = (front + 1) & (SIZE - 1);
			return queue[temp];
		}
		bool empty() const {
			return front == rear;
		}
		bool full() const {
			return ((rear + 1) & (SIZE - 1)) == front; 
		}
		size_t size() const {
			if (rear >= front) {
				return rear - front;
			}
			else {
				return SIZE - front + rear;
			}
		}
	};
}