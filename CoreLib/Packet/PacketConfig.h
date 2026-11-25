#pragma once

#include <cstdint>
#include "PacketHeader.h"

namespace Core {
    // 패킷·내부 메시지의 필드 크기와 네트워크 검증 계약.
    inline constexpr uint8_t MAX_CHARACTER = 10;
    inline constexpr uint8_t MAX_CHARNAME_LEN = 32;
    inline constexpr uint16_t MAX_MESSAGE_LEN = 128;
    inline constexpr uint16_t MAX_INVENTORY_ITEMS = 10;
    inline constexpr uint16_t MAX_CHAT_PACKET = 10;
    // 월드의 수용 상한과 스냅샷 배열 크기는 같은 기준을 사용한다.
    inline constexpr uint16_t MAX_ZONE_CAPACITY = 2000;
    inline constexpr uint16_t MAX_MONSTER_COUNT = 1000;
    inline constexpr uint16_t ACTION_RESULT_COUNT = 500;
    inline constexpr uint16_t MAX_MONSTER_DELTA = 3000;
    inline constexpr uint16_t DELTA_UPDATE_COUNT = 5000;
    // 클라이언트가 한 번에 조회할 수 있는 profile 수. 요청 패킷 크기 상한이자 응답 상한.
    inline constexpr uint16_t MAX_PROFILE_BATCH = 50;
    inline constexpr uint32_t INVALID_PROFILE_ID = 0;
    inline constexpr uint8_t MASK_EXIST = 1 << 0;
    inline constexpr uint8_t MASK_AUTHENTICATED = 1 << 1;
    inline constexpr uint8_t MASK_NOT_CHEAT = 1 << 2;
}
