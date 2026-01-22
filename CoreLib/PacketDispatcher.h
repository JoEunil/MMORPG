#pragma once
#include "NoneZoneThreadPool.h"
#include "ZoneThreadSet.h"
#include "IPacketView.h"
#include "IPacketDispatcher.h"
#include "StateManager.h"
#include "PacketTypes.h"
#include "IPingPacketWriter.h"
#include "IIOCP.h"
namespace Core {
    class ILogger;
    class PacketDispatcher : public IPacketDispatcher {
        NoneZoneThreadPool* noneZoneThreadPool;
        ZoneThreadSet*  zoneThreadSet;
        ILogger* logger;
        StateManager* stateManager;
        IPingPacketWriter* writer;
        IIOCP* iocp;
        void Initialize(NoneZoneThreadPool* a, ZoneThreadSet* z, ILogger* l, StateManager* s, IPingPacketWriter* w, IIOCP* i) {
            noneZoneThreadPool = a;
            zoneThreadSet = z;
            logger = l;
            stateManager = s;
            writer = w;
            iocp = i;
        }
        bool IsReady() {
            if (logger == nullptr) return false;
            if (stateManager == nullptr) return false;
            if (noneZoneThreadPool == nullptr || zoneThreadSet == nullptr)
                return false;
            if (writer == nullptr) return false;
            if (iocp == nullptr) return false;
            return true;
        }
        friend class Initializer;
    public:
        void Process(std::shared_ptr<IPacketView> pv) override;
        void Disconnect(uint64_t sessionID) override;
        uint8_t HealthCheck(uint64_t sessionID) override {
           return stateManager->HealthCheck(sessionID);
        }
        uint64_t GetRTT(std::shared_ptr<IPacketView> pv, uint64_t now) override {
            Pong* body = parseBody<Pong>(pv->GetPtr());
            return now - body->serverTimeMs;
        }
        void Ping(uint64_t sessionID, uint64_t rtt, uint64_t nowMs) override {
            auto packet = writer->GetPingPacket(rtt, nowMs);
            iocp->SendData(sessionID, packet);
        }
    };
}
