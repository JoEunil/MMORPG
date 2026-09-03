#pragma once
#include <atomic>
#include <cstdint>

#include "LockFreeQueue.h"

namespace Base {
	enum class Priority : uint8_t {
		Droppable = 0,
		Important = 1
	};

	template <typename Bucket, typename T, uint32_t deferQSize>
	class BackPressure {
		static_assert(deferQSize >= 2 && (deferQSize & (deferQSize - 1)) == 0,
			"BackPressure: deferQSize must be a power of 2 and at least 2");

		Bucket m_bucket;
		LockFreeQueue<T, deferQSize> m_defer;
		std::atomic<bool> m_degraded = false;

	public:
		void Enqueue(T& item, Priority priority) {
			if (!m_degraded.load(std::memory_order_relaxed)) {
				if (m_bucket.push(item)) {
					return;
				}
				m_degraded.store(true, std::memory_order_relaxed);
			}

			if (priority == Priority::Droppable) {
				return;
			}
			if (!m_defer.push(item)) {  
				// 중요 요청의 최종 Drop  
				// 비동기 로거가 아닌 WAL 같이 durable한 로그를 써야 한다.  
				// 일반 비동기 로그와 달리 주기적인 fsync를 통해 장애 시 손실 범위를 제한할 수 있다.  
			}
		}

		bool Dequeue(T& out) {
			// producer와 복구 상태 전환이 완전히 직렬화되지 않으므로, degraded 상태에서는 defer queue에 남은 작업을 우선 소진한다.
			if (m_degraded.load(std::memory_order_relaxed)) {
				if (m_bucket.pop(out)) {
					return true;
				}
				if (m_defer.pop(out)) {
					return true;
				}

				m_degraded.store(false, std::memory_order_relaxed);
				return false;
			}

			if (m_bucket.pop(out)) {
				return true;
			}

			return m_defer.pop(out);
		}
	};
}
