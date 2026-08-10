#pragma once

#include <cstdint>
#include <chrono>

namespace Core {
    inline constexpr uint8_t CHANNEL_ID = 1;
    inline constexpr uint16_t MAGIC = 0xABCD;

    inline constexpr uint8_t MAX_CHARACTER = 10;
    inline constexpr uint8_t MAX_CHARNAME_LEN = 32;
    inline constexpr uint16_t MAX_MESSAGE_LEN = 128;


    inline constexpr uint16_t MSGPOOL_SIZE = 100;

    inline constexpr uint16_t MESSAGE_LEN = 1024;
    inline constexpr uint16_t MQ_THREADPOOL_SIZE = 2; // 수신 큐

    inline constexpr uint16_t ZONE_COUNT = 4; // lobby 제외

    inline constexpr uint16_t MAX_INVENTORY_ITEMS = 10;
    inline constexpr uint8_t BROADCAST_THREADPOOL_SIZE = 6;
    inline constexpr uint16_t NON_ZONE_THREADPOOL_SIZE = 3;
    
    inline constexpr uint16_t SHARD_SIZE = 8; // stateManager의 session - zone 매핑 샤드, 접근하는 스레드풀 크기의 2~4배 정도
    inline constexpr uint16_t SHARD_SIZE_MASK = SHARD_SIZE - 1;


    inline constexpr uint16_t ACTION_RESULT_COUNT = 500;
    inline constexpr uint16_t MAX_MONSTER_COUNT = 1000;
    inline constexpr uint16_t MAX_MONSTER_DELTA = 3000;

    inline constexpr uint32_t INVALID_ZONE_INTERNAL_ID = 0; // 발급 실패 sentinel
    // zoneInternalID는 zone별로 단조 증가하며 재사용하지 않는다.
    // 초당 1000회 발급해도 약 50일치라 정상 운영 중 고갈되지 않지만,
    // wrap-around로 ID가 중복되면 조용히 오동작하므로 상한에서 발급을 중단한다.
    inline constexpr uint32_t MAX_ZONE_INTERNAL_ID = UINT32_MAX;

    inline constexpr uint16_t MAX_ZONE_CAPACITY = 2000;
    inline constexpr uint16_t MAX_USER_CAPACITY = 7000;
    inline constexpr uint16_t MAX_CHAT_PACKET = 10;

    // 클라이언트가 한 번에 조회할 수 있는 profile 수. 요청 패킷 크기 상한이자 응답 상한.
    inline constexpr uint16_t MAX_PROFILE_BATCH = 50;
    inline constexpr uint32_t INVALID_PROFILE_ID = 0;
    inline constexpr uint16_t DELTA_UPDATE_COUNT = 5000;

    inline constexpr auto FULL_SNAPSHOT_TICK = std::chrono::milliseconds(1000);
    inline constexpr auto DELTA_SNAPSHOT_TICK = std::chrono::milliseconds(50);
    inline constexpr auto GAME_TICK = std::chrono::milliseconds(50);

    inline constexpr float MOVE_BUDGET_PER_TICK = 1.0f;  // 틱당 허용 이동 거리
    inline constexpr float MOVE_BUDGET_CAP = 3.0f;       // 이월 상한(네트워크 지터 허용). 서버에서 충돌 판정이 없다면, cap을 map의 가장 얇은 벽 두께로 제한해야 한다. 
    inline constexpr float MOVE_BUDGET_EPSILON = 0.001f; // float 오차로 정상 이동이 거부되지 않도록

    inline constexpr uint16_t MAX_CHEAT_COUNT = 10; // 최대 허용 치트 점수
    inline constexpr uint8_t  MASK_EXIST = 1 << 0; 
    inline constexpr uint8_t  MASK_AUTHENTICATED = 1 << 1;
    inline constexpr uint8_t  MASK_NOT_CHEAT = 1 << 2;
    // - 일반적인 비정상 패킷 1회 발생 시 바로 끊지 않음
    // - 크리티컬한 경우, 점수를 높게 줘서 즉시 Disconnect 가능
    


   inline constexpr std::chrono::steady_clock::duration CHEAT_FLUSH_TIME = std::chrono::seconds(2);
   // stateManager에서 Cheat Count flush할 시간.
   // 네트워크 문제에 의해 증가된 cheat count 정리하기 위함.. 
   inline constexpr uint16_t CHAT_QUEUE_SIZE = 512;
   inline constexpr uint16_t  BROADCAST_QUEUE_SIZE = 512;
   inline constexpr uint16_t  DISCONNECT_QUEUE_SIZE = 8192;

   inline constexpr uint32_t  NON_ZONE_QUEUE_SIZE = 8192;
   inline constexpr uint32_t  ZONE_QUEUE_SIZE = 16384;  

   inline constexpr size_t MQ_SIZE = 256;

   template <typename T>
   constexpr bool IsPowerOfTwo(T x) {
       return x != 0 && (x & (x - 1)) == 0;
   }
   static_assert(IsPowerOfTwo(SHARD_SIZE), "SHARD_SIZE must be a power of two");
   static_assert(IsPowerOfTwo(CHAT_QUEUE_SIZE), "CHAT_QUEUE_SIZE must be a power of two");
   static_assert(IsPowerOfTwo(BROADCAST_QUEUE_SIZE), "BROADCAST_QUEUE_SIZE must be a power of two");
   static_assert(IsPowerOfTwo(DISCONNECT_QUEUE_SIZE), "DISCONNECT_QUEUE_SIZE must be a power of two");
   static_assert(IsPowerOfTwo(ZONE_QUEUE_SIZE), "ZONE_QUEUE_SIZE must be a power of two");
   static_assert(IsPowerOfTwo(NON_ZONE_QUEUE_SIZE), "NON_ZONE_QUEUE_SIZE must be a power of two");
}
