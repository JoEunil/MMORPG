#pragma once

#include <cstdint>
#include <memory>
#include "ZoneEvent.h"
#include "../LoggerGlobal.h"

namespace Core {
    class ZoneState;
    class SessionManager;
    class LobbyZone;
    class IZoneTransfer;
    class CacheCommandSender;
    class IServerResponseQueue;
    class PacketWriter;
    struct ZoneChangeResult;

    class ZoneEventHandler {
        SessionManager* sessionManager = nullptr;
        LobbyZone* lobbyZone = nullptr;
        IZoneTransfer* zoneTransfer = nullptr;
        IZoneRequestQueue* zoneRequests = nullptr;
        const CacheCommandSender* cacheCommands = nullptr;
        IServerResponseQueue* responses = nullptr;
        PacketWriter* writer = nullptr;

        void Initialize(SessionManager& sessions, LobbyZone& lobby, IZoneTransfer& transfer,
            IZoneRequestQueue& requests, const CacheCommandSender& commands,
            IServerResponseQueue& responseQueue, PacketWriter& packetWriter) {
            sessionManager = &sessions;
            lobbyZone = &lobby;
            zoneTransfer = &transfer;
            zoneRequests = &requests;
            cacheCommands = &commands;
            responses = &responseQueue;
            writer = &packetWriter;
        }
        bool IsReady() const {
            if (sessionManager == nullptr) {
                sysLogger->LogError("zone event handler", "session manager not initialized");
                return false;
            }
            if (lobbyZone == nullptr) {
                sysLogger->LogError("zone event handler", "lobby zone not initialized");
                return false;
            }
            if (zoneTransfer == nullptr) {
                sysLogger->LogError("zone event handler", "zone transfer not initialized");
                return false;
            }
            if (zoneRequests == nullptr) {
                sysLogger->LogError("zone event handler", "zone request queue not initialized");
                return false;
            }
            if (cacheCommands == nullptr) {
                sysLogger->LogError("zone event handler", "cache command sender not initialized");
                return false;
            }
            if (responses == nullptr) {
                sysLogger->LogError("zone event handler", "response queue not initialized");
                return false;
            }
            if (writer == nullptr) {
                sysLogger->LogError("zone event handler", "packet writer not initialized");
                return false;
            }
            return true;
        }
        friend class Initializer;
        void SendZoneChangeFailed(uint64_t sessionID);
        void SendZoneChange(uint64_t sessionID, const ZoneChangeResult& transition);
    public:
        void ProcessLobbyEntry(ZoneState& zone, const ZoneLobbyEntryEvent& request);
        void ProcessRespawn(ZoneState& zone, const ZoneRespawnEvent& request);
        void ProcessZoneTransfer(ZoneState& zone, const std::shared_ptr<ZoneTransfer>& transfer, bool returning);
        void ApplyProfileVersion(ZoneState& zone, const ProfileVersionUpdate& update);
        void ProcessDisconnect(ZoneState& zone, uint16_t zoneID, const ZoneDisconnectEvent& request);
        void ProcessBootstrap(ZoneState& zone, const ZoneBootstrapEvent& request);
        void SaveRemainingCharacters(ZoneState& zone);
    };
}
