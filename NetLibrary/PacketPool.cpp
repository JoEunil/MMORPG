#include "pch.h"
#include "PacketPool.h"
#include "Packet.h"
#include "Config.h"

#include <CoreLib/IPacket.h>
#include <CoreLib/LoggerGlobal.h>

namespace Net {
	std::shared_ptr<Core::IPacket> PacketPool::Acquire()
	{
		Packet* packet = m_fixedPool.Allocate();
		if (packet == nullptr) {
			return nullptr;
		}
		
		// 커스텀 deleter: delete 대신 PacketPool에 반환
		return std::shared_ptr<Core::IPacket>(packet, [this](Packet* p) { this->Return(p); });
	}

	std::unique_ptr<Core::IPacket, Core::PacketDeleter> PacketPool::AcquireUnique()
	{
		Packet* packet = m_fixedPool.Allocate();
		if (packet == nullptr) {
			return nullptr;
		}
		return std::unique_ptr<Core::IPacket, Core::PacketDeleter>(static_cast<Core::IPacket*>(packet));
	}

	void PacketPool::Return(Core::IPacket* packet) {
		Packet* p = static_cast<Packet*>(packet);
		m_fixedPool.Deallocate(p);
	}

}
