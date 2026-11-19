#pragma once

#include <cstdint>
#include <memory>

#include "../../Interface/IPacketView.h"

namespace Core {
    class IZonePacketQueue {
    public:
        virtual ~IZonePacketQueue() = default;
        virtual bool TryEnqueuePacket(uint16_t zoneID, std::unique_ptr<IPacketView, PacketViewDeleter> packet) = 0;
    };
}
