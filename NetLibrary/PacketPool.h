#pragma once

#include <vector>
#include <mutex>
#include <cstdint>

#include <CoreLib/IPacket.h>
#include <CoreLib/IPacketPool.h>
#include <CoreLib/LoggerGlobal.h>
#include <BaseLib/FixedObjectPool.h>
#include "Config.h"
#include "Packet.h"


namespace Net {
    template <uint32_t PoolSize>
    class PacketPool : public Core::IPacketPool {
        const uint32_t m_packetLen;

        Base::FixedObjectPool<Packet, PoolSize> m_fixedPool{ m_packetLen, this };

        bool IsReady() {
            if (m_fixedPool.GetPoolSize() < PoolSize / 2) {
                Core::sysLogger->LogError("packet pool", "m_fixedPool not initialized");
                return false;
			}
            return true;
        }

        friend class Initializer;
    public:
        PacketPool(uint32_t packetLen) :m_packetLen(packetLen) {
        }

		std::shared_ptr<Core::IPacket> Acquire() override {
			Packet* packet = m_fixedPool.Allocate();
			if (packet == nullptr) {
				return nullptr;
			}
			packet->SetLength(0);

			// 커스텀 deleter: delete 대신 PacketPool에 반환
			return std::shared_ptr<Core::IPacket>(packet, [this](Packet* p) { this->Return(p); });
		}

		std::unique_ptr<Core::IPacket, Core::PacketDeleter> AcquireUnique() override {
			Packet* packet = m_fixedPool.Allocate();
			if (packet == nullptr) {
				return nullptr;
			}
			packet->SetLength(0);
			return std::unique_ptr<Core::IPacket, Core::PacketDeleter>(static_cast<Core::IPacket*>(packet));
		}

		void Return(Core::IPacket* packet) override {
			Packet* p = static_cast<Packet*>(packet);
			m_fixedPool.Deallocate(p);
		}

        size_t GetPoolSize() {
            return m_fixedPool.GetPoolSize();
        }
    };
}
