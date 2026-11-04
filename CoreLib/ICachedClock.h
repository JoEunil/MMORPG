#pragma once

#include <cstdint>

namespace Core {
    class ICachedClock {
    public:
        virtual ~ICachedClock() = default;
        // Cached steady_clock milliseconds for elapsed time, not wall-clock timestamps.
        virtual uint64_t GetTimeMS() const = 0;
    };
}
