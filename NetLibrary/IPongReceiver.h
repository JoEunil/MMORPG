#pragma once

#include <cstdint>

namespace Net {
    class IPongReceiver {
    public:
        virtual ~IPongReceiver() = default;
        virtual void PongReceived(uint64_t sessionID, uint64_t rtt) = 0;
    };
}
