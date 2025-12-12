#pragma once

#include <cstdint>

namespace Core {
    class IPacketView {
    public:
        virtual ~IPacketView() = default;
        virtual uint64_t GetSessionID() const = 0;
        virtual uint8_t* GetPtr() const = 0;
        virtual uint16_t GetLength() const = 0;
        virtual uint8_t GetOpcode() const = 0;
    };
}
