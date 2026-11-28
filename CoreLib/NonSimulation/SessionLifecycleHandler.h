#pragma once

#include <cstdint>
#include <memory>
#include "../LoggerGlobal.h"

namespace Core {
    class SessionManager;
    class LobbyZone;
    class IZoneRequestQueue;
    class CacheCommandSender;
    class IServerResponseQueue;
    class PacketWriter;
    struct ZoneTransfer;

    class SessionLifecycleHandler {
        SessionManager* sessionManager = nullptr;
        LobbyZone* lobbyZone = nullptr;
        IZoneRequestQueue* zoneRequests = nullptr;
        const CacheCommandSender* cacheCommands = nullptr;
        IServerResponseQueue* responses = nullptr;
        PacketWriter* writer = nullptr;

        void Initialize(SessionManager& sessions, LobbyZone& lobby, IZoneRequestQueue& requests,
            const CacheCommandSender& commands, IServerResponseQueue& responseQueue, PacketWriter& packetWriter) {
            sessionManager = &sessions;
            lobbyZone = &lobby;
            zoneRequests = &requests;
            cacheCommands = &commands;
            responses = &responseQueue;
            writer = &packetWriter;
        }
        bool IsReady() const {
            if (sessionManager == nullptr) {
                sysLogger->LogError("session lifecycle handler", "session manager not initialized");
                return false;
            }
            if (lobbyZone == nullptr) {
                sysLogger->LogError("session lifecycle handler", "lobby zone not initialized");
                return false;
            }
            if (zoneRequests == nullptr) {
                sysLogger->LogError("session lifecycle handler", "zone request queue not initialized");
                return false;
            }
            if (cacheCommands == nullptr) {
                sysLogger->LogError("session lifecycle handler", "cache command sender not initialized");
                return false;
            }
            if (responses == nullptr) {
                sysLogger->LogError("session lifecycle handler", "response queue not initialized");
                return false;
            }
            if (writer == nullptr) {
                sysLogger->LogError("session lifecycle handler", "packet writer not initialized");
                return false;
            }
            return true;
        }
        friend class Initializer;
    public:
        void Disconnect(uint64_t sessionID);
        void ReturnToLobby(const std::shared_ptr<ZoneTransfer>& transfer);
    };
}
