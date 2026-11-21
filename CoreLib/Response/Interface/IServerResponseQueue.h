#pragma once

#include <cstdint>
#include <memory>
#include "../../Interface/IPacket.h"

namespace Core {
    class IServerResponseQueue {
      public:
        virtual ~IServerResponseQueue() = default;
        virtual bool Enqueue(uint64_t sessionID, std::unique_ptr<IPacket, PacketDeleter> packet) = 0;
        virtual bool EnqueueZoneChange(uint64_t sessionID, std::unique_ptr<IPacket, PacketDeleter> packet, uint16_t zoneID, uint64_t ownershipEpoch) = 0;
    };
}
