#pragma once

#include <memory>
#include <chrono>

#include "IPacketView.h"
#include "IPacketDispatcher.h"
namespace Core {
    class IPacketDispatcher {
    public:
        virtual void Process(std::shared_ptr<IPacketView> pv) = 0;
        virtual void Disconnect(uint64_t sessionID) = 0;
        virtual uint8_t HealthCheck(uint64_t sessionID) = 0;
        virtual uint64_t GetRTT(std::shared_ptr<IPacketView> pv) = 0;
        virtual void Ping(uint64_t sessionID, uint64_t rtt, std::chrono::steady_clock::time_point now) = 0;

    };
}
