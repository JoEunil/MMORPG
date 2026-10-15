#pragma once
#include <cstdint>
#include "Config.h"

namespace Net {
	class TrafficFloodDetector {
		// 시간 측정 없는 RECV 크기 휴리스틱이다. bytes/s 또는 events/s 제한이 아니다.
		uint32_t m_bytes = 0;
		uint32_t m_recvCount = 0;

	public:
		bool ByteReceived(uint32_t byte) {
			m_recvCount++;
			m_bytes += byte;

			if (m_recvCount >= RECV_WINDOW) {
				if (m_bytes > BYTE_THRESHOLD) return true;
				if (m_bytes < MIN_BYTE_PER_WINDOW) return true;
				m_recvCount = 0;
				m_bytes = 0;
			}
			return false;
		}
	};
}