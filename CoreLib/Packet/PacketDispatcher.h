#pragma once
#include "../NonSimulation/Interface/INonSimulationQueue.h"
#include "../World/Interface/ISessionStatusReader.h"
#include "../Interface/IPacketView.h"
#include "../Interface/IPacketDispatcher.h"
#include "../World/Interface/IZonePacketQueue.h"
#include "../LoggerGlobal.h"
namespace Core {
    class ILogger;
    class PacketDispatcher : public IPacketDispatcher {
        INonSimulationQueue* nonSimulationQueue = nullptr;
        IZonePacketQueue* zonePackets = nullptr;
        const ISessionStatusReader* sessions = nullptr;
        void Initialize(INonSimulationQueue& nonSimulation, IZonePacketQueue& packets, const ISessionStatusReader& reader) {
            nonSimulationQueue = &nonSimulation;
            zonePackets = &packets;
            sessions = &reader;
        }
        bool IsReady() const {
            if (sessions == nullptr) {
                sysLogger->LogError("packet dispatcher", "session reader not initialized");
                return false;
            }
            if (nonSimulationQueue == nullptr) {
                sysLogger->LogError("packet dispatcher", "non-simulation queue not initialized");
                return false;
            }
            if (zonePackets == nullptr) {
                sysLogger->LogError("packet dispatcher", "zone packet queue not initialized");
                return false;
            }
            return true;
        }
        friend class Initializer;
    public:
        void Process(std::unique_ptr<IPacketView, PacketViewDeleter> pv) override;
        void Disconnect(uint64_t sessionID) override;
        uint8_t HealthCheck(uint64_t sessionID) override {
            return sessions->HealthCheck(sessionID);
        }
    };
}
