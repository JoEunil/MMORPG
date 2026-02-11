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
    inline constexpr const uint16_t NONE_ZONE_THREADPOOL_SIZE = 3;
    
    inline constexpr const uint16_t SHARD_SIZE = 8; // stateManager의 session - zone 매핑 샤드, 접근하는 스레드풀 크기의 2~4배 정도
    inline constexpr const uint16_t SHARD_SIZE_MASK = SHARD_SIZE - 1;

    inline constexpr const uint16_t TARGET_DELTA_SNAPSHOT_POOL_SIZE = 50;
    inline constexpr const uint16_t MAX_DELTA_SNAPSHOT_POOL_SIZE = 100;
    inline constexpr const uint16_t MIN_DELTA_SNAPSHOT_POOL_SIZE = 20;

    inline constexpr const uint16_t TARGET_FULL_SNAPSHOT_POOL_SIZE = 50;
    inline constexpr const uint16_t MAX_FULL_SNAPSHOT_POOL_SIZE = 100;
    inline constexpr const uint16_t MIN_FULL_SNAPSHOT_POOL_SIZE = 20;


    inline constexpr const uint16_t ACTION_RESULT_COUNT = 500;
    inline constexpr const uint16_t MAX_MONSTER_COUNT = 1000;
    inline constexpr const uint16_t MAX_MONSTER_DELTA = 3000;

    inline constexpr const uint16_t MAX_ZONE_CAPACITY = 500;
    inline constexpr const uint16_t MAX_USER_CAPACITY = 5000;
    inline constexpr const uint16_t MAX_CHAT_PACKET = 10;
    inline constexpr const uint16_t DELTA_UPDATE_COUNT = 1000;

    inline constexpr const auto FULL_SNAPSHOT_TICK = std::chrono::milliseconds(1000);
    inline constexpr const auto DELTA_SNAPSHOT_TICK = std::chrono::milliseconds(50);
    inline constexpr const auto GAME_TICK = std::chrono::milliseconds(50);

    inline constexpr const uint16_t MAX_CHEAT_COUNT = 5; // 최대 허용 치트 점수
    inline constexpr const uint8_t  MASK_EXIST = 1 << 0; 
    inline constexpr const uint8_t  MASK_AUTHENTICATED = 1 << 1;
    inline constexpr const uint8_t  MASK_NOT_CHEAT = 1 << 2;
    // - 일반적인 비정상 패킷 1회 발생 시 바로 끊지 않음
    // - 크리티컬한 경우, 점수를 높게 줘서 즉시 Disconnect 가능
    


   inline constexpr const std::chrono::steady_clock::duration CHEAT_FLUSH_TIME = std::chrono::seconds(2);
   // stateManager에서 Cheat Count flush할 시간.
   // 네트워크 문제에 의해 증가된 cheat count 정리하기 위함.. 
   inline constexpr const uint16_t CHAT_QUEUE_SIZE = 512;
   inline constexpr const uint16_t  BROADCAST_QUEUE_SIZE = 512;
   inline constexpr const uint16_t  DISCONNECT_QUEUE_SIZE = 8192;

   inline constexpr const uint32_t  NONE_ZONE_QUEUE_SIZE = 8192;
   inline constexpr const uint32_t  ZONE_QUEUE_SIZE = 16384;  


   template <typename T>
   constexpr bool IsPowerOfTwo(T x) {
       return x != 0 && (x & (x - 1)) == 0;
   }
   static_assert(IsPowerOfTwo(SHARD_SIZE), "SHARD_SIZE must be a power of two");
   static_assert(IsPowerOfTwo(CHAT_QUEUE_SIZE), "CHAT_QUEUE_SIZE must be a power of two");
   static_assert(IsPowerOfTwo(BROADCAST_QUEUE_SIZE), "BROADCAST_QUEUE_SIZE must be a power of two");
   static_assert(IsPowerOfTwo(DISCONNECT_QUEUE_SIZE), "DISCONNECT_QUEUE_SIZE must be a power of two");
   static_assert(IsPowerOfTwo(ZONE_QUEUE_SIZE), "ZONE_QUEUE_SIZE must be a power of two");
   static_assert(IsPowerOfTwo(NONE_ZONE_QUEUE_SIZE), "NONE_ZONE_QUEUE_SIZE must be a power of two");
}
