#pragma once

#include <vector>
#include <mutex>
#include <cstdint>

#include <CoreLib/IPacket.h>
#include <CoreLib/IPacketPool.h>

namespace Net {
    class Packet;
    class PacketPool : public Core::IPacketPool {
        uint32_t m_remains;
        std::vector<Packet*> m_packets;
        // Pool 요청, 해제가 빈도가 잦기 때문에 raw pointer로 관리
        std::mutex m_mutex;
        
        const uint32_t m_targetPool, m_maxPool, m_minPool;
        const uint32_t m_packetLen;
        void Initialize(); 
        bool IsReady() {
            return m_packets.size() > 0;
        }
        void Adjust();
        void Increase(uint32_t& size);
        void Decrease(uint32_t& size);
        
        friend class Initializer;
    public:
        PacketPool(uint32_t target, uint32_t max, uint32_t min, uint32_t packetLen)
            : m_targetPool(target), m_maxPool(max), m_minPool(min),m_packetLen(packetLen) {
        }
        ~PacketPool();
        std::shared_ptr<Core::IPacket> Acquire() override;
        std::unique_ptr<Core::IPacket, Core::PacketDeleter> AcquireUnique() override;
        void Return(Core::IPacket* packet) override;
    };
}
