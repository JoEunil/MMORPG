#pragma once

#include <vector>
#include <mutex>
#include <cstdint>

#include <CoreLib/IPacket.h>
#include <CoreLib/IPacketPool.h>
#include <CoreLib/LoggerGlobal.h>
#include <BaseLib/FixedObjectPool.h>

#include "Packet.h"


namespace Net {
    class PacketPool : public Core::IPacketPool {
        const uint32_t m_poolSize;
        const uint32_t m_packetLen;

        Base::FixedObjectPool<Packet, PACKETPOOL_SIZE> m_fixedPool{ m_packetLen };

        void Initialize(); 
        bool IsReady() {
            return true;
        }

        friend class Initializer;
    public:
        PacketPool(uint32_t pool, uint32_t packetLen)
            : m_poolSize(pool),m_packetLen(packetLen) {
        }
        std::shared_ptr<Core::IPacket> Acquire() override;
        std::unique_ptr<Core::IPacket, Core::PacketDeleter> AcquireUnique() override;
        void Return(Core::IPacket* packet) override;
    };
}
