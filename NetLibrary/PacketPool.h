#pragma once

#include <deque>
#include <mutex>
#include <cstdint>

#include <CoreLib/IPacketPool.h>

namespace Core {
    class IPacket;
}
namespace Net {
    class Packet;
    class PacketPool : public Core::IPacketPool {
        uint16_t m_remains;
        std::deque<Packet*> m_packets;
        // Pool 요청, 해제가 빈도가 잦기 때문에 raw pointer로 관리
        std::mutex m_mutex;
        
        uint16_t m_targetPool, m_maxPool, m_minPool;
        void Initialize(); 
        bool IsReady() {
            return m_packets.size() > 0;
        }
        void Adjust();
        void Increase(uint16_t& size);
        void Decrease(uint16_t& size);
        
        friend class Initializer;
    public:
        PacketPool(uint16_t target, uint16_t max, uint16_t min) {
            m_targetPool = target;
            m_maxPool = max;
            m_minPool = min;
        }
        ~PacketPool();
        std::shared_ptr<Core::IPacket> Acquire() override;
        void Return(Core::IPacket* packet) override;
    };
}
