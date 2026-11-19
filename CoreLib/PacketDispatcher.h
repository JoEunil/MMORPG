#pragma once
#include "NonZoneThreadPool.h"
#include "IPacketView.h"
#include "IPacketDispatcher.h"
#include "IZonePacketQueue.h"
#include "LoggerGlobal.h"
#include "SessionManager.h"
#include "PacketTypes.h"
namespace Core {
    class ILogger;
    class PacketDispatcher : public IPacketDispatcher {
        NonZoneThreadPool* nonZoneThreadPool = nullptr;
        IZonePacketQueue* zonePackets = nullptr;
        SessionManager* sessionManager = nullptr;
        void Initialize(NonZoneThreadPool* a, IZonePacketQueue* packets, SessionManager* s) {
            nonZoneThreadPool = a;
            zonePackets = packets;
            sessionManager = s;
        }
        bool IsReady() const {
            if (sessionManager == nullptr) {
                sysLogger->LogError("packet dispatcher", "sessionManager not initialized");
                return false;
            }
            if (nonZoneThreadPool == nullptr) {
                sysLogger->LogError("packet dispatcher", "nonZoneThreadPool not initialized");
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
           return sessionManager->HealthCheck(sessionID);
        }
    };
}
