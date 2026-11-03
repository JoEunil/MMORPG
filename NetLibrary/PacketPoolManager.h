#pragma once

#include <CoreLib/IPacketPoolManager.h>
#include "Config.h"
#include "PacketPool.h"

namespace Net {
    class PacketPoolManager : public Core::IPacketPoolManager {
        PacketPool<PACKETPOOL_SIZE> m_normalPool{ NORMAL_PACKET_LENGTH };
        PacketPool<BPACKETPOOL_SIZE> m_bigPool{ BIG_PACKET_LENGTH };
        PacketPool<GPACKETPOOL_SIZE> m_giantPool{ GIANT_PACKET_LENGTH };

        bool IsReady() const {
            return m_normalPool.IsReady() && m_bigPool.IsReady() && m_giantPool.IsReady();
        }
        friend class Initializer;

    public:
        std::shared_ptr<Core::IPacket> Acquire(Core::PacketPoolType type) override {
            switch (type) {
            case Core::PacketPoolType::Normal: return m_normalPool.Acquire();
            case Core::PacketPoolType::Big: return m_bigPool.Acquire();
            case Core::PacketPoolType::Giant: return m_giantPool.Acquire();
            }
            return nullptr;
        }

        std::unique_ptr<Core::IPacket, Core::PacketDeleter> AcquireUnique(Core::PacketPoolType type) override {
            switch (type) {
            case Core::PacketPoolType::Normal: return m_normalPool.AcquireUnique();
            case Core::PacketPoolType::Big: return m_bigPool.AcquireUnique();
            case Core::PacketPoolType::Giant: return m_giantPool.AcquireUnique();
            }
            return nullptr;
        }

        size_t GetPoolSize(Core::PacketPoolType type) const {
            switch (type) {
            case Core::PacketPoolType::Normal: return m_normalPool.GetPoolSize();
            case Core::PacketPoolType::Big: return m_bigPool.GetPoolSize();
            case Core::PacketPoolType::Giant: return m_giantPool.GetPoolSize();
            }
            return 0;
        }
    };
}
