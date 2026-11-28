#pragma once

#include <cstdint>

namespace Core {
    // Request target and expected epoch; this is not proof of current ownership.
    // For lobby admission, zoneID names the destination Zone.
    struct ZoneSessionContext {
        uint64_t sessionID = 0;
        uint16_t zoneID = 0;
        uint64_t ownershipEpoch = 0;
    };
}
