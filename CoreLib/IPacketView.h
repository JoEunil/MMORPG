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
        virtual void Release() = 0;
    };
    struct PacketDeleter {
        void operator()(IPacketView* p) const {
            p->Release();
        }
    };
}
