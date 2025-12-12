#pragma once

#include <cstdint>
#include <chrono>

namespace Core {
    inline constexpr const uint8_t CHANNEL_ID = 1;
    inline constexpr const uint16_t MAGIC = 0xABCD;

    inline constexpr const uint8_t MAX_CHARACTER = 10;
    inline constexpr const uint8_t MAX_CHARNAME_LEN = 32;
    inline constexpr const uint16_t MAX_MESSAGE_LEN = 128;


    inline constexpr const uint16_t TARGET_MSGPOOL_SIZE = 50;
    inline constexpr const uint16_t MAX_MSGPOOL_SIZE = 100;
    inline constexpr const uint16_t MIN_MSGPOOL_SIZE = 20;

    inline constexpr const uint16_t MESSGAGE_LEN = 1024;
    inline constexpr const uint16_t MQ_THREADPOOL_SIZE = 2; // 수신 큐

    inline constexpr const uint16_t ZONE_COUNT = 4; // lobby 제외

    inline constexpr const uint16_t MAX_INVENTORY_ITEMS = 10;
    inline constexpr const uint8_t BROADCAST_THREADPOOL_SIZE = 2;
    inline constexpr const uint16_t ASYNC_THREADPOOL_SIZE = 3;
    
    inline constexpr const uint16_t SHARD_SIZE = 8; // stateManager의 session - zone 매핑 샤드, 접근하는 스레드풀 크기의 2~4배 정도

    inline constexpr const uint16_t TARGET_DELTA_SNAPSHOT_POOL_SIZE = 50;
    inline constexpr const uint16_t MAX_DELTA_SNAPSHOT_POOL_SIZE = 100;
    inline constexpr const uint16_t MIN_DELTA_SNAPSHOT_POOL_SIZE = 20;

    inline constexpr const uint16_t TARGET_FULL_SNAPSHOT_POOL_SIZE = 50;
    inline constexpr const uint16_t MAX_FULL_SNAPSHOT_POOL_SIZE = 100;
    inline constexpr const uint16_t MIN_FULL_SNAPSHOT_POOL_SIZE = 20;

    inline constexpr const uint16_t MAX_ZONE_CAPACITY = 2000;
    inline constexpr const uint16_t MAX_USER_CAPACITY = 5000;
    inline constexpr const uint16_t MAX_CHAT_PACKET = 10;
    inline constexpr const uint16_t DELTA_UPDATE_COUNT = 2000;

    inline constexpr const float ZONE_SIZE = 100.0f;     // zone 한 칸 크기
    inline constexpr const int ZONE_HORIZON = 3; // 3x3
    inline constexpr const int ZONE_VERTICAL = 3;
    inline constexpr const float TRANSITION_BUFFER = 5.0f; // 겹치는 영역

    inline constexpr const auto FULL_SNAPSHOT_TICK = std::chrono::milliseconds(1000);
    inline constexpr const auto DELTA_SNAPSHOT_TICK = std::chrono::milliseconds(50);
    inline constexpr const auto GAME_TICK = std::chrono::milliseconds(50);
}
