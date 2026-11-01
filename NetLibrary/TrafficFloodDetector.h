#pragma once
#include <cstdint>
#include <algorithm>

#include "NetTimer.h"
#include "Config.h"

namespace Net {
	class TrafficFloodDetector {

		uint64_t m_recvTokens = RECV_TOKEN_CAPACITY; // tiny packet 방지, recv/s 제한
		uint64_t m_bytesTokens = BYTES_TOKEN_CAPACITY;   // big packet 방지, bytes/s 제한
		uint64_t m_lastRefill = 0;
	public:
		bool ByteReceived(uint32_t receivedBytes, uint64_t now = NetTimer::GetTimeMS()) {
			uint64_t elapsed = now - m_lastRefill;

			// 보충 계산에 사용하는 경과 시간 제한
			uint64_t elapsedForRefill = (std::min<uint64_t>)(elapsed, 5000);
			m_bytesTokens = (std::min)(BYTES_TOKEN_CAPACITY, m_bytesTokens + elapsedForRefill * BYTES_TOKEN_REFILL_RATE);
			m_recvTokens = (std::min)(RECV_TOKEN_CAPACITY, m_recvTokens + elapsedForRefill * RECV_TOKEN_REFILL_RATE);

			m_lastRefill = now;
			if (receivedBytes > m_bytesTokens or m_recvTokens < RECV_UNIT)
				return true;

			m_bytesTokens -= receivedBytes;
			m_recvTokens -= RECV_UNIT;
			return false;
		}
	};
}
