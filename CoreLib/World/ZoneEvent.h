#pragma once

#include <cstdint>
#include <memory>
#include <variant>

#include "../Interface/IPacketView.h"
#include "Interface/IZoneRequestQueue.h"

namespace Core {
    struct ZoneTransfer;

    struct ZonePacketEvent {
        std::unique_ptr<IPacketView, PacketViewDeleter> packet;
    };

    struct ZoneTransferEvent {
        std::shared_ptr<ZoneTransfer> transfer;
    };

    struct ZoneTransferRejectedEvent {
        std::shared_ptr<ZoneTransfer> transfer;
    };

    struct ZoneProfileVersionEvent {
        ProfileVersionUpdate update;
    };

    struct ZoneBootstrapEvent {
        uint64_t sessionID = 0;
        uint64_t ownershipEpoch = 0;
        bool resendZoneChange = false;
    };

    struct ZoneLobbyEntryEvent {
        uint64_t sessionID = 0;
        uint64_t ownershipEpoch = 0;
    };

    struct ZoneRespawnEvent {
        uint64_t sessionID = 0;
        uint64_t ownershipEpoch = 0;
    };

    struct ZoneDisconnectEvent {
        uint64_t sessionID = 0;
        uint64_t ownershipEpoch = 0;
    };

    using ZoneEvent = std::variant<ZonePacketEvent, ZoneTransferEvent, ZoneTransferRejectedEvent,
        ZoneProfileVersionEvent, ZoneBootstrapEvent, ZoneLobbyEntryEvent, ZoneRespawnEvent, ZoneDisconnectEvent>;

}
