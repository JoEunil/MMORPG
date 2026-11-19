#pragma once

#include <chrono>
#include <cstdint>
#include "Config.h"
#include "WorldMap.h"

namespace Core {
    static_assert(WorldMap::FormatVersion == 1 && WorldMap::ZoneColumns > 0 && WorldMap::ZoneRows > 0
        && WorldMap::Zones.size() == WorldMap::ZoneColumns * WorldMap::ZoneRows
        && WorldMap::Zones.size() <= UINT16_MAX,
        "The exported world must contain a complete supported layout with wire-compatible zone IDs.");
    inline constexpr uint16_t ZONE_COUNT = static_cast<uint16_t>(WorldMap::Zones.size()); // Lobby excluded.
    inline constexpr uint16_t TOWN_ZONE_ID = 1;
    inline constexpr float TOWN_SPAWN_X = 10.0f;
    inline constexpr float TOWN_SPAWN_Y = 10.0f;
    inline constexpr uint8_t BROADCAST_THREADPOOL_SIZE = 6; // Each Zone stays on one worker; different Zones send in parallel.
    inline constexpr uint16_t BROADCAST_QUEUE_SIZE = 512;
    inline constexpr uint32_t ZONE_QUEUE_SIZE = 16384;
    inline constexpr uint16_t SHARD_SIZE = 8; // stateManager의 session - zone 매핑 샤드, 접근하는 스레드풀 크기의 2~4배 정도
    inline constexpr uint16_t SHARD_SIZE_MASK = SHARD_SIZE - 1;
    inline constexpr uint64_t INVALID_CHARACTER_ID = 0;
    inline constexpr uint32_t INVALID_ZONE_INTERNAL_ID = 0; // 발급 실패 sentinel
    // zoneInternalID는 zone별로 단조 증가하며 재사용하지 않는다.
    // 초당 1000회 발급해도 약 50일치라 정상 운영 중 고갈되지 않지만,
    // wrap-around로 ID가 중복되면 조용히 오동작하므로 상한에서 발급을 중단한다.
    inline constexpr uint32_t MAX_ZONE_INTERNAL_ID = UINT32_MAX;
    inline constexpr auto FULL_SNAPSHOT_TICK = std::chrono::milliseconds(1000);
    inline constexpr auto DELTA_SNAPSHOT_TICK = std::chrono::milliseconds(50);
    inline constexpr auto GAME_TICK = std::chrono::milliseconds(50);
    inline constexpr float WORLD_MOVEMENT_SCALE = 0.2f;
    inline constexpr float MOVE_BUDGET_PER_TICK = 0.3825f; // Player: 7.65 units/second at 50ms (0.85x); monster scale stays separate.
    inline constexpr float MOVE_BUDGET_CAP = 3.0f;       // 이월 상한(네트워크 지터 허용). 서버에서 충돌 판정이 없다면, cap을 map의 가장 얇은 벽 두께로 제한해야 한다.
    inline constexpr float MOVE_BUDGET_EPSILON = 0.001f; // float 오차로 정상 이동이 거부되지 않도록
    // 일반적인 비정상 패킷 1회 발생 시 바로 끊지 않음.
    // 크리티컬한 경우, 점수를 높여 즉시 Disconnect 가능.
    inline constexpr uint16_t MAX_CHEAT_COUNT = 10; // 최대 허용 치트 점수
    // 네트워크 문제에 의해 증가된 stateManager의 cheat count를 정리하는 간격.
    inline constexpr std::chrono::steady_clock::duration CHEAT_FLUSH_TIME = std::chrono::seconds(2);

    static_assert(BROADCAST_THREADPOOL_SIZE > 0);
    static_assert(IsPowerOfTwo(SHARD_SIZE), "SHARD_SIZE must be a power of two");
    static_assert(IsPowerOfTwo(BROADCAST_QUEUE_SIZE), "BROADCAST_QUEUE_SIZE must be a power of two");
    static_assert(IsPowerOfTwo(ZONE_QUEUE_SIZE), "ZONE_QUEUE_SIZE must be a power of two");
}
