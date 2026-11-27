#pragma once

#include <memory>

#include "IPacketView.h"
namespace Core {
    class IPacketDispatcher {
    public:
        virtual ~IPacketDispatcher() = default;
        virtual void Process(std::unique_ptr<IPacketView, PacketViewDeleter> pv) = 0;
        virtual void Disconnect(uint64_t sessionID) = 0;
        virtual uint8_t HealthCheck(uint64_t sessionID) = 0;

    };
}
