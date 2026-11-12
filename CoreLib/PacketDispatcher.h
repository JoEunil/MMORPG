#pragma once
#include "NonZoneThreadPool.h"
#include "ZoneThreadSet.h"
#include "IPacketView.h"
#include "IPacketDispatcher.h"
#include "StateManager.h"
#include "PacketTypes.h"
namespace Core {
    class ILogger;
    class PacketDispatcher : public IPacketDispatcher {
        NonZoneThreadPool* nonZoneThreadPool = nullptr;
        ZoneThreadSet*  zoneThreadSet = nullptr;
        StateManager* stateManager = nullptr;
        void Initialize(NonZoneThreadPool* a, ZoneThreadSet* z, StateManager* s) {
            nonZoneThreadPool = a;
            zoneThreadSet = z;
            stateManager = s;
        }
        bool IsReady() {
            if (stateManager == nullptr) return false;
            if (nonZoneThreadPool == nullptr || zoneThreadSet == nullptr)
                return false;
            return true;
        }
        friend class Initializer;
    public:
        void Process(std::unique_ptr<IPacketView, PacketViewDeleter> pv) override;
        void Disconnect(uint64_t sessionID) override;
        uint8_t HealthCheck(uint64_t sessionID) override {
           return stateManager->HealthCheck(sessionID);
        }
    };
}
