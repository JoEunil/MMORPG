#pragma once

#include <cstdint>

namespace Core {
    class IPacket {
        uint16_t m_zoneID = 0;
    public:
        virtual ~IPacket() = default;
        virtual uint8_t* GetBuffer() = 0;
        virtual uint16_t GetLength() = 0;
        virtual void SetLength(uint16_t len) = 0;
        virtual void Release() = 0;

        // Core 에서만 사용
        void SetZone(uint16_t z) {
            m_zoneID = z;
        }
        uint16_t GetZone() {
            return m_zoneID;
         }
    };
    struct PacketDeleter {
        void operator()(IPacket* p) const {
            p->Release();
        }
    };
}
