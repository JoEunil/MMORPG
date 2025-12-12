#pragma once

#include <cstdint>
#include <chrono>

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

	inline constexpr const uint16_t RING_BUFFER_SIZE = 1024;
	inline constexpr const uint16_t RECV_BUFFER_SIZE = 128;
	inline constexpr const uint16_t MAX_PACKET_LEN = 1024; // PacketPool에서 미리 할당할 패킷 크기

	inline constexpr const uint8_t MAX_ACCEPT_BUFFER_CNT = 10;

}
