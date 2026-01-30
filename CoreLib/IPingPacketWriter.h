#pragma once
#include <memory>
#include "IPacket.h"
namespace Core {
	class IPingPacketWriter {
	public:
		virtual ~IPingPacketWriter() = default;
		virtual std::unique_ptr<IPacket, PacketDeleter> GetPingPacket(uint64_t rtt, uint64_t nowMs) = 0;
	};
}