#pragma once
#include "NoneZoneThreadPool.h"
#include "ZoneThreadSet.h"
#include "IPacketView.h"
#include "IPacketDispatcher.h"
#include "StateManager.h"
#include "PacketTypes.h"

namespace Core {
    class ILogger;
    class PacketDispatcher : public IPacketDispatcher {
        NoneZoneThreadPool* noneZoneThreadPool;
        ZoneThreadSet*  zoneThreadSet;
        ILogger* logger;
        StateManager* stateManager;
        void Initialize(NoneZoneThreadPool* a, ZoneThreadSet* z, ILogger* l, StateManager* s) {
            noneZoneThreadPool = a;
            zoneThreadSet = z;
            logger = l;
            stateManager = s;
        }
        bool IsReady() {
            if (logger == nullptr) return false;
            if (stateManager == nullptr) return false;
            if (noneZoneThreadPool == nullptr || zoneThreadSet == nullptr)
                return false;
            return true;
        }
        friend class Initializer;
    public:
        void Process(std::shared_ptr<IPacketView> pv) override;
        void Disconnect(uint64_t sessionID) override;
        uint8_t HealthCheck(uint64_t sessionID) override {
           return stateManager->HealthCheck(sessionID);
        }
        uint64_t GetRTT(std::shared_ptr<IPacketView> pv) override {
            Pong* body = parseBody<Pong>(pv->GetPtr());
            auto now = std::chrono::steady_clock::now();
            auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                now.time_since_epoch()
            ).count();
            return ns - body->serverTimeNs;
        }
        void Ping(uint64_t sessionID, uint64_t rtt, std::chrono::steady_clock::time_point now) override {
            noneZoneThreadPool->Ping(sessionID, rtt, now);
        }
    };
}
