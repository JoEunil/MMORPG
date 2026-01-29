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

	inline constexpr const uint16_t MAX_PACKETPOOL_SIZE = 200;
	inline constexpr const uint16_t MIN_PACKETPOOL_SIZE = 50;
	inline constexpr const uint16_t TARGET_PACKETPOOL_SIZE = 100;

	inline constexpr const uint16_t MAX_BPACKETPOOL_SIZE = 200;
	inline constexpr const uint16_t MIN_BPACKETPOOL_SIZE = 50;
	inline constexpr const uint16_t TARGET_BPACKETPOOL_SIZE = 100;

	inline constexpr const uint16_t MAX_OVERLAPPEDPOOL_SIZE = 200;
	inline constexpr const uint16_t MIN_OVERLAPPEDPOOL_SIZE = 50;
	inline constexpr const uint16_t TARGET_OVERLAPPEDPOOL_SIZE = 100;

	inline constexpr const uint16_t MAX_CONTEXTPOOL_SIZE = 200;
	inline constexpr const uint16_t MIN_CONTEXTPOOL_SIZE = 50;
	inline constexpr const uint16_t TARGET_CONTEXTPOOL_SIZE = 100;
	inline constexpr const uint16_t FLUSH_CONTEXTPOOL = 50;

	inline constexpr const uint16_t RECV_BUFFER_SIZE = 1024;  // MTU 고려. 내부에서 buffer를 패킷으로 조각내서 처리하기 때문에 buffer 수신은 한번에 크게 읽어서 시스템 콜 비용 절감.
	inline constexpr const uint16_t RING_BUFFER_SIZE = RECV_BUFFER_SIZE * 16; // 서버 수신 패킷은 크기가 작아서 충분, ring buffer에 남은 버퍼공간 반납하는 로직도 있어서 안전함.
	inline constexpr const uint16_t MAX_PACKET_LEN = 4096; // PacketPool에서 미리 할당할 패킷 크기, 대용량 상태 동기화 패킷 처리를 위해 4KB로 설정

	inline constexpr const uint8_t MAX_ACCEPT_BUFFER_CNT = 10;

	inline constexpr const uint16_t SESSION_SHARD_SIZE = 16;
	inline constexpr const uint16_t SESSION_SHARD_MASK = SESSION_SHARD_SIZE-1;
	inline constexpr const uint8_t PING_COUNT_LIMIT = 5;

	inline constexpr const uint16_t PING_STACK_RESERVE = 1000; 
	inline constexpr const std::chrono::seconds PING_LOOP_WAIT = std::chrono::seconds(1);


	constexpr const uint32_t RECV_WINDOW = 20;
	constexpr const uint32_t BYTE_THRESHOLD = RECV_WINDOW* RECV_BUFFER_SIZE * 0.8;
	constexpr const uint32_t MIN_BYTE_PER_WINDOW = sizeof(Core::PacketHeader) * RECV_WINDOW * 0.8;

	template <typename T>
	constexpr bool IsPowerOfTwo(T x) {
		return x != 0 && (x & (x - 1)) == 0;
	}
	static_assert(IsPowerOfTwo(SESSION_SHARD_SIZE), "SESION_SHARD_SIZE must be a power of two");
}
