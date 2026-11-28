#pragma once

#include <cstdint>

namespace Core {
    struct CharacterRequestContext {
        uint64_t sessionID = 0;
        uint64_t characterID = 0;
        uint32_t requestID = 0;
    };
}
