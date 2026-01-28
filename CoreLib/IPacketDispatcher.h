#pragma once

#include <memory>
#include <chrono>

#include "IPacketView.h"
#include "IPacketDispatcher.h"
namespace Core {
    class IPacketDispatcher {
    public:
        virtual ~IPacketDispatcher() = default;
        virtual void Process(std::shared_ptr<IPacketView> pv) = 0;
        virtual void Disconnect(uint64_t sessionID) = 0;
        virtual uint8_t HealthCheck(uint64_t sessionID) = 0;
        virtual uint64_t GetRTT(std::shared_ptr<IPacketView> pv, uint64_t now) = 0;
        virtual void Ping(uint64_t sessionID, uint64_t rtt, uint64_t nowMs) = 0;

    };
}
