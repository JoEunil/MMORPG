#pragma once

#include <cstdint>
#include <memory>

#include "IPacketView.h"

namespace Core {
    class INonSimulationQueue {
    public:
        virtual ~INonSimulationQueue() = default;
        virtual void EnqueuePacket(std::unique_ptr<IPacketView, PacketViewDeleter> packet) = 0;
        virtual void EnqueueDisconnect(uint64_t sessionID) = 0;
    };
}
