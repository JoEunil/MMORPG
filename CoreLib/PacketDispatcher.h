#pragma once
#include "NoneZoneThreadPool.h"
#include "ZoneThreadSet.h"
#include "IPacketView.h"
#include "IPacketDispatcher.h"
#include "StateManager.h"

namespace Core {
    class NoneZoneThreadPool;
    class ZoneThreadSet;
    class IPacketView;
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
    };
}
