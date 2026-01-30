#pragma once

#include <cstdint>

namespace Core {
    class IPacket {
    public:
        virtual ~IPacket() = default;
        virtual uint8_t* GetBuffer() = 0;
        virtual uint16_t GetLength() = 0;
        virtual void SetLength(uint16_t len) = 0;
        virtual void Release() = 0;
    };
    struct PacketDeleter {
        void operator()(IPacket* p) const {
            p->Release();
        }
    };
}
