#pragma once
#include <cstdint>
#include <array>

namespace Base {
	// 고정 크기 bounded ring queue. thread-safe하지 않으므로 동시 접근은 호출자가 동기화한다.
	// front == rear를 empty 상태로 사용하므로 실제 수용량은 SIZE - 1이다.
	// push/pop은 full/empty에서 false를 반환하며 기존 원소나 출력값을 변경하지 않는다.
	// ClientContext에서 SendQueue, pending 중 큐잉하기 위해 사용. 
	// ClientContextPool에서 FlushQueue, 최대 크기 고정된 케이스. 
	template <typename T, uint32_t SIZE>
	class RingQueue {
		std::array<T, SIZE> queue;
		uint32_t front, rear;
	public:
		RingQueue() : front(0), rear(0) {

		}
		static_assert(SIZE >= 2 && (SIZE & (SIZE - 1)) == 0, "Ring Queue Size must be a power of 2 and at least 2");

		bool push(const T& data) {
			if (full())
				return false;
			queue[rear] = data;
			rear = (rear + 1) & (SIZE - 1);
			return true;
		}
		bool push(T&& data) {
			if (full())
				return false;
			queue[rear] = std::move(data);
			rear = (rear + 1) & (SIZE - 1);
			return true;
		}
		bool pop(T& out) {
			if (empty())
				return false;
			out = std::move(queue[front]);
			front = (front + 1) & (SIZE - 1);
			return true;
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