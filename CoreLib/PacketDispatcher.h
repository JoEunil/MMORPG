#pragma once
#include "NonZoneThreadPool.h"
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
        NonZoneThreadPool* nonZoneThreadPool;
        ZoneThreadSet*  zoneThreadSet;
        StateManager* stateManager;
        IPingPacketWriter* writer;
        IIOCP* iocp;
        void Initialize(NonZoneThreadPool* a, ZoneThreadSet* z, StateManager* s, IPingPacketWriter* w, IIOCP* i) {
            nonZoneThreadPool = a;
            zoneThreadSet = z;
            stateManager = s;
            writer = w;
            iocp = i;
        }
        bool IsReady() {
            if (stateManager == nullptr) return false;
            if (nonZoneThreadPool == nullptr || zoneThreadSet == nullptr)
                return false;
            if (writer == nullptr) return false;
            if (iocp == nullptr) return false;
            return true;
        }
        friend class Initializer;
    public:
        void Process(std::unique_ptr<IPacketView, PacketViewDeleter> pv) override;
        void Disconnect(uint64_t sessionID) override;
        uint8_t HealthCheck(uint64_t sessionID) override {
           return stateManager->HealthCheck(sessionID);
        }
        uint64_t GetRTT(std::unique_ptr<IPacketView, PacketViewDeleter> pv, uint64_t now) override {
            Pong* body = parseBody<Pong>(pv->GetPtr());
            return now - body->serverTimeMs;
        }
        void Ping(uint64_t sessionID, uint64_t rtt, uint64_t nowMs) override {
            auto p = writer->GetPingPacket(rtt, nowMs);
            if (!p) {
                return;
            }
            iocp->SendDataUnique(sessionID, std::move(p));
        }
    };
}
