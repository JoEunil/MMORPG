#pragma once
#include <memory>
#include "../Interface/IPacketView.h"
#include "NonSimulationEvent.h"
#include "../LoggerGlobal.h"

namespace Core {
    class SessionLifecycleHandler;
    class WorldEntryHandler;
    class BazaarRequestHandler;
    class ZoneRequestHandler;
    class PlayerDataRequestHandler;
    class ChatRequestHandler;
    class NonSimulationDispatcher {
        struct EventVisitor {
            NonSimulationDispatcher& dispatcher;
            void operator()(NonSimulationWorkEvent& event) const;
            void operator()(NonSimulationDisconnectEvent& event) const;
            void operator()(NonSimulationLobbyReturnEvent& event) const;
        };
        WorldEntryHandler* worldEntry = nullptr;
        BazaarRequestHandler* bazaar = nullptr;
        ZoneRequestHandler* zone = nullptr;
        PlayerDataRequestHandler* playerData = nullptr;
        ChatRequestHandler* chatRequests = nullptr;
        SessionLifecycleHandler* lifecycle = nullptr;
        void Initialize(WorldEntryHandler& worldEntryHandler, BazaarRequestHandler& bazaarHandler,
            ZoneRequestHandler& zoneHandler, PlayerDataRequestHandler& playerDataHandler,
            ChatRequestHandler& chatRequestsHandler, SessionLifecycleHandler& sessionLifecycle) {
            worldEntry = &worldEntryHandler;
            bazaar = &bazaarHandler;
            zone = &zoneHandler;
            playerData = &playerDataHandler;
            chatRequests = &chatRequestsHandler;
            lifecycle = &sessionLifecycle;
        }

        bool IsReady() const {
            if (worldEntry == nullptr) {
                sysLogger->LogError("non simulation dispatcher", "worldEntry handler not initialized");
                return false;
            }
            if (bazaar == nullptr) {
                sysLogger->LogError("non simulation dispatcher", "bazaar handler not initialized");
                return false;
            }
            if (zone == nullptr) {
                sysLogger->LogError("non simulation dispatcher", "zone handler not initialized");
                return false;
            }
            if (playerData == nullptr) {
                sysLogger->LogError("non simulation dispatcher", "playerData handler not initialized");
                return false;
            }
            if (chatRequests == nullptr) {
                sysLogger->LogError("non simulation dispatcher", "chatRequests handler not initialized");
                return false;
            }
            if (lifecycle == nullptr) {
                sysLogger->LogError("non simulation dispatcher", "lifecycle handler not initialized");
                return false;
            }
            return true;
        }
        void ProcessPacket(std::unique_ptr<IPacketView, PacketViewDeleter> packet);
        friend class Initializer;
    public:
        void Process(NonSimulationEvent& event);
    };
}
