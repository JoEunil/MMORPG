#pragma once

#include <cstdint>

namespace Core {
    // Notifications run synchronously under the session shard lock.
    // Queue downstream work promptly; do not reenter SessionManager.
    class ISessionLifecycleListener {
    public:
        virtual ~ISessionLifecycleListener() = default;
        // The initial world-entry ACK needs the allocated chat ID immediately.
        virtual uint64_t OnSessionEnteredWorld(uint64_t sessionID, uint16_t zoneID, uint32_t profileID) = 0;
        virtual void OnSessionZoneChanged(uint64_t sessionID, uint16_t previousZoneID, uint16_t currentZoneID) = 0;
        virtual void OnSessionClosed(uint64_t sessionID, uint16_t zoneID) = 0;
    };
}
