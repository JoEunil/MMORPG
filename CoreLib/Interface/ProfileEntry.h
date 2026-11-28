#pragma once

#include <cstdint>
#include "../Config.h"

namespace Core {
#pragma pack(push, 1)
    struct ProfileEntry {
        uint32_t profileId;
        uint32_t version;
        uint8_t name[MAX_CHARNAME_LEN];
    };
#pragma pack(pop)
}
