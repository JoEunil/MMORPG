#pragma once

#include <cstdint>

namespace Core {
    // Request target and expected epoch; this is not proof of current ownership.
    // For lobby admission, zoneID names the destination Zone.
    struct ZoneSessionContext {
        uint64_t sessionID = 0;
        uint16_t zoneID = 0;
        uint64_t ownershipEpoch = 0;
    };

    struct ProfileVersionUpdate {
        uint64_t sessionID = 0;
        uint32_t profileID = 0;
        uint32_t version = 0;
    };

    class IZoneRequestQueue {
    public:
        virtual ~IZoneRequestQueue() = default;
        virtual bool TryEnqueueLobbyEntry(const ZoneSessionContext& context) = 0;
        virtual bool TryEnqueueRespawn(const ZoneSessionContext& context) = 0;
        virtual bool TryEnqueueBootstrap(const ZoneSessionContext& context, bool resendZoneChange) = 0;
        virtual bool TryEnqueueProfileVersion(uint16_t zoneID, const ProfileVersionUpdate& update) = 0;
        virtual bool TryEnqueueDisconnect(const ZoneSessionContext& context) = 0;
    };
}
