#pragma once
#include <memory>
#include "../Interface/IPacketView.h"
#include "NonSimulationEvent.h"

namespace Core {
    class SessionLifecycleHandler;
    class WorldEntryHandler;
    class BazaarRequestHandler;
    class ZoneRequestHandler;
    class PlayerDataRequestHandler;
    class ChatRequestHandler;
    class NonSimulationHandler {
        struct EventVisitor {
            NonSimulationHandler& handler;
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
        void Initialize(WorldEntryHandler& worldEntryHandler, BazaarRequestHandler& bazaarHandler, ZoneRequestHandler& zoneHandler, PlayerDataRequestHandler& playerDataHandler, ChatRequestHandler& chatRequestsHandler, SessionLifecycleHandler& sessionLifecycle);
        bool IsReady() const;
        friend class Initializer;
    public:
        void Process(NonSimulationEvent& event);
        void Process(std::unique_ptr<IPacketView, PacketViewDeleter> packet);
    };
}
