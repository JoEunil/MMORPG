#pragma once
#include <memory>
#include "IPacket.h"
namespace Core {
	class IPingPacketWriter {
	public:
		virtual std::shared_ptr<IPacket> GetPingPacket(uint64_t rtt, uint64_t nowMs) = 0;
	};
}