#pragma once

#include <cstdint>
#include "../Config.h"

namespace Core {
#pragma pack(push, 1)
    // 프로필 캐시 조회와 패킷 응답이 공유하는 값 계약.
    struct ProfileEntry {
        uint32_t profileId;
        uint32_t version;
        uint8_t name[MAX_CHARNAME_LEN];
    };
#pragma pack(pop)
}
