#pragma once

#include <cstdint>
#include <chrono>
#include <CoreLib/PacketTypes.h>

namespace Net {
	inline constexpr const uint8_t IOCP_THREADPOOL_SIZE = 4; // IOCP 워커
	inline constexpr const uint16_t MAX_CLIENT_CONNECTION = 5000;
    inline constexpr const char* LISTEN_ADDRESS = "0.0.0.0"; // allow anywhere
	inline constexpr const uint16_t LISTEN_PORT = 9999;
    inline constexpr const uint16_t PREPOSTED_ACCEPTS = 10; //worker threadpool * 2~4

	inline constexpr const uint32_t NORMAL_PACKET_LENGTH = 512;
	inline constexpr const uint32_t BIG_PACKET_LENGTH = 4096;

	inline constexpr const uint32_t MAX_PACKETPOOL_SIZE = 10000;
	inline constexpr const uint32_t MIN_PACKETPOOL_SIZE = 200;
	inline constexpr const uint32_t TARGET_PACKETPOOL_SIZE = 5000;

	inline constexpr const uint32_t MAX_BPACKETPOOL_SIZE = 10000;
	inline constexpr const uint32_t MIN_BPACKETPOOL_SIZE = 200;
	inline constexpr const uint32_t TARGET_BPACKETPOOL_SIZE = 5000;

	inline constexpr const uint16_t MAX_OVERLAPPEDPOOL_SIZE = MAX_CLIENT_CONNECTION * 4;
	inline constexpr const uint16_t MIN_OVERLAPPEDPOOL_SIZE = 1000;
	inline constexpr const uint16_t TARGET_OVERLAPPEDPOOL_SIZE = MAX_CLIENT_CONNECTION * 2;

	inline constexpr const uint32_t MAX_PACKETVIEWPOOL_SIZE = 30000;
	inline constexpr const uint32_t MIN_PACKETVIEWPOOL_SIZE = 200;
	inline constexpr const uint32_t TARGET_PACKETVIEWPOOL_SIZE = 10000;

	inline constexpr const uint16_t FLUSH_CONTEXTPOOL = 500;

	// 가장 가까운 2의 거듭제곱 계산
	// 원리:
	// 1. 2의 거듭제곱 - 1은 00011111 형태가 됨
	// 2. 비트 시프트와 OR 연산을 통해 모든 하위 비트를 1로 채움
	// 3. 마지막에 +1을 하면 바로 다음 2의 거듭제곱이 됨
	// -> shift 범위는 자료형의 절반까지 수행하면 충분
	inline constexpr uint16_t NextPowerOf2(uint16_t n) {
		n--;
		n |= n >> 1;
		n |= n >> 2;
		n |= n >> 4;
		n |= n >> 8;
		n++;
		return n;
	}
	inline constexpr const uint16_t RECV_BUFFER_SIZE = 128;  
	inline constexpr const uint32_t RING_BUFFER_SIZE_MIN = RECV_BUFFER_SIZE* 256; // recv buffer size * 16으로 설정했을 때 dummy client 테스트에서 buffer override 발생..
	inline constexpr const uint32_t RING_BUFFER_SIZE = NextPowerOf2(RING_BUFFER_SIZE_MIN);
	inline constexpr const uint32_t RING_BUFFER_SIZE_MASK = RING_BUFFER_SIZE - 1;
	inline constexpr const uint16_t MAX_PACKET_LEN = 4096; // PacketPool에서 미리 할당할 패킷 크기, 대용량 상태 동기화 패킷 처리를 위해 4KB로 설정

	inline constexpr const uint16_t RELEASE_Q_SIZE_MIN = RING_BUFFER_SIZE / sizeof(Core::PacketHeader) + 2;

	inline constexpr const uint16_t RELEASE_Q_SIZE = NextPowerOf2(RELEASE_Q_SIZE_MIN);
	inline constexpr const uint16_t RELEASE_Q_SIZE_MASK = RELEASE_Q_SIZE - 1;

	inline constexpr const uint16_t SESSION_SHARD_SIZE = 16;
	inline constexpr const uint16_t SESSION_SHARD_MASK = SESSION_SHARD_SIZE-1;
	inline constexpr const uint8_t PING_COUNT_LIMIT = 5;

	inline constexpr const std::chrono::seconds PING_LOOP_WAIT = std::chrono::seconds(1);


	constexpr const uint32_t RECV_WINDOW = 20;
	constexpr const uint32_t BYTE_THRESHOLD = RECV_WINDOW* RECV_BUFFER_SIZE * 0.8;
	constexpr const uint32_t MIN_BYTE_PER_WINDOW = sizeof(Core::PacketHeader) * RECV_WINDOW * 0.8;


	template <typename T>
	constexpr bool IsPowerOfTwo(T x) {
		return x != 0 && (x & (x - 1)) == 0;
	}
	static_assert(IsPowerOfTwo(SESSION_SHARD_SIZE), "SESION_SHARD_SIZE must be a power of two");
	static_assert(IsPowerOfTwo(RELEASE_Q_SIZE), "RELEASE_Q_SIZE must be a power of two");
}
