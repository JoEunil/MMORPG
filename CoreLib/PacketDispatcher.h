#pragma once
#include "AsyncThreadPool.h"
#include "ZoneThreadSet.h"
#include "IPacketView.h"
#include "IPacketDispatcher.h"
#include "StateManager.h"

namespace Core {
    class AsyncThreadPool;
    class ZoneThreadSet;
    class IPacketView;
    class ILogger;
    class PacketDispatcher : public IPacketDispatcher {
        AsyncThreadPool* asyncThreadPool;
        ZoneThreadSet*  zoneThreadSet;
        ILogger* logger;
        StateManager* stateManager;
        void Initialize(AsyncThreadPool* a, ZoneThreadSet* z, ILogger* l, StateManager* s) {
            asyncThreadPool = a;
            zoneThreadSet = z;
            logger = l;
            stateManager = s;
        }
        bool IsReady() {
            if (logger == nullptr) return false;
            if (stateManager == nullptr) return false;
            if (asyncThreadPool == nullptr || zoneThreadSet == nullptr)
                return false;
            return true;
        }
        friend class Initializer;
    public:
        void Process(std::shared_ptr<IPacketView> pv) override;
        void Disconnect(uint64_t sessionID) override;
    };
}
