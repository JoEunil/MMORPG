#pragma once

#include <cstddef>
#include <cstdint>

namespace Net {
    class IReceiveContext {
    public:
        virtual ~IReceiveContext() = default;
        virtual uint16_t BeginRecvIO(uint64_t expectedSessionID, uint8_t*& buffer) = 0;
        virtual void CompleteRecvIO(uint64_t expectedSessionID) = 0;
        virtual bool IsSessionAlive(uint64_t expectedSessionID) const = 0;
        virtual bool CheckGameSession() const = 0;
        virtual bool EnqueueRecvQ(uint64_t expectedSessionID, uint8_t* buffer, size_t length) = 0;
    };
}
