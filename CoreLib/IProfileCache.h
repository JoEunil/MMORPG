#pragma once
#include <cstdint>

#include "PacketTypes.h"

namespace Core {
    class IProfileCache {
        // read 전용 경로, 동기적으로 read 수행하기 위함.
    public:
        virtual ~IProfileCache() = default;
        virtual uint16_t GetBatch(const uint32_t* profileIds, uint16_t count, ProfileEntry* out) = 0;
    };
}
