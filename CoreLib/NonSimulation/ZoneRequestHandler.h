#pragma once

#include <cstdint>
#include <memory>
#include "../Interface/IPacketView.h"
#include "../LoggerGlobal.h"

namespace Core {
    class IServerResponseQueue;
    class IZonePacketQueue;
    class IZoneRequestQueue;
    class LobbyZone;
    class PacketWriter;
    class SessionManager;

    class ZoneRequestHandler {
        IServerResponseQueue* responses = nullptr;
        PacketWriter* writer = nullptr;
        SessionManager* sessionManager = nullptr;
        LobbyZone* lobbyZone = nullptr;
        IZonePacketQueue* zonePackets = nullptr;
        IZoneRequestQueue* zoneRequests = nullptr;

        void Initialize(IServerResponseQueue* responsesValue, PacketWriter* writerValue,
            SessionManager* sessionManagerValue, LobbyZone* lobbyZoneValue, IZonePacketQueue* zonePacketsValue,
            IZoneRequestQueue* zoneRequestsValue) {
            responses = responsesValue;
            writer = writerValue;
            sessionManager = sessionManagerValue;
            lobbyZone = lobbyZoneValue;
            zonePackets = zonePacketsValue;
            zoneRequests = zoneRequestsValue;
        }
        bool IsReady() const {
            if (responses == nullptr) {
                sysLogger->LogError("ZoneRequestHandler", "responses not initialized");
                return false;
            }
            if (writer == nullptr) {
                sysLogger->LogError("ZoneRequestHandler", "writer not initialized");
                return false;
            }
            if (sessionManager == nullptr) {
                sysLogger->LogError("ZoneRequestHandler", "sessionManager not initialized");
                return false;
            }
            if (lobbyZone == nullptr) {
                sysLogger->LogError("ZoneRequestHandler", "lobbyZone not initialized");
                return false;
            }
            if (zonePackets == nullptr) {
                sysLogger->LogError("ZoneRequestHandler", "zonePackets not initialized");
                return false;
            }
            if (zoneRequests == nullptr) {
                sysLogger->LogError("ZoneRequestHandler", "zoneRequests not initialized");
                return false;
            }
            return true;
        }
        friend class Initializer;

    public:
        void ZoneChange(IPacketView* p);
        void RespawnToTown(IPacketView* p);
        void ForwardZoneRequest(std::unique_ptr<IPacketView, PacketViewDeleter> packet);
        void Resync(IPacketView* p);
    };
}
