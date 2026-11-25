#pragma once

#include <cstdint>

namespace Core {
    class IMessage {
    public:
        virtual ~IMessage() = default;
        virtual uint8_t* GetBuffer() = 0;
        virtual const uint8_t* GetBuffer() const = 0;
        virtual uint16_t GetLength() const = 0;
        virtual uint16_t GetCapacity() const = 0;
        virtual void SetLength(uint16_t length) = 0;
    };
}
