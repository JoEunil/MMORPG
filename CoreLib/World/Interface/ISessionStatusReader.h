#pragma once

#include <cstdint>

namespace Core {
    class ISessionStatusReader {
    public:
        virtual ~ISessionStatusReader() = default;
        virtual int16_t GetZoneID(uint64_t sessionID) const = 0;
        virtual uint8_t HealthCheck(uint64_t sessionID) const = 0;
    };
}
