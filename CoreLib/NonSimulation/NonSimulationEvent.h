#pragma once

#include <cstdint>
#include <memory>
#include <variant>

#include "../Interface/IPacketView.h"

namespace Core {
    struct ZoneTransfer;

    struct NonSimulationWorkEvent {
        std::unique_ptr<IPacketView, PacketViewDeleter> work;
    };

    struct NonSimulationDisconnectEvent {
        uint64_t sessionID = 0;
    };

    struct NonSimulationLobbyReturnEvent {
        std::shared_ptr<ZoneTransfer> transfer;
    };

    using NonSimulationEvent = std::variant<NonSimulationWorkEvent,
        NonSimulationDisconnectEvent, NonSimulationLobbyReturnEvent>;
}
