#pragma once

#include <cstdint>

namespace Core {
    struct ZoneEvent;

    class IZoneEventQueue {
    public:
        virtual ~IZoneEventQueue() = default;
        virtual bool TryEnqueue(uint16_t zoneID, ZoneEvent& event) = 0;
    };
}
