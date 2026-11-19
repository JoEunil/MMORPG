#pragma once

#include <cstdint>

namespace Core {
    struct ProfileVersionUpdate {
        uint64_t sessionID = 0;
        uint32_t profileID = 0;
        uint32_t version = 0;
    };

    class IZoneRequestQueue {
    public:
        virtual ~IZoneRequestQueue() = default;
        virtual bool TryEnqueueLobbyEntry(uint16_t zoneID, uint64_t sessionID, uint64_t ownershipEpoch) = 0;
        virtual bool TryEnqueueRespawn(uint16_t zoneID, uint64_t sessionID, uint64_t ownershipEpoch) = 0;
        virtual bool TryEnqueueBootstrap(uint16_t zoneID, uint64_t sessionID, uint64_t ownershipEpoch, bool resendZoneChange) = 0;
        virtual bool TryEnqueueProfileVersion(uint16_t zoneID, const ProfileVersionUpdate& update) = 0;
        virtual bool TryEnqueueDisconnect(uint16_t zoneID, uint64_t sessionID, uint64_t ownershipEpoch) = 0;
    };
}
