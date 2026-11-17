#pragma once

#include <array>
#include <atomic>

#include "IZoneEventQueue.h"
#include "ZoneThread.h"

namespace Core {
    class ZoneEventRouter final : public IZoneEventQueue, public IZoneTransfer {
        struct ZoneEntry {
            std::unique_ptr<ZoneState> state;
            ZoneThread worker;
        };
        std::array<ZoneEntry, ZONE_COUNT> zones;
        std::atomic<bool> acceptingTransfers = false;

        void Initialize(ZoneHandler& handler, CorePerfCollector& perf);
        bool IsReady() const;
        void Start();
        void Stop();

        friend class Initializer;
    public:
        ZoneEventRouter();
        ~ZoneEventRouter() override;
        ZoneState* GetZone(uint16_t zoneID);
        bool TryEnqueue(uint16_t zoneID, ZoneEvent& event) override;
        uint16_t FindDestination(uint16_t sourceZoneID, float x, float y) const override;
        bool TryBeginTransfer(uint16_t destinationID, const std::shared_ptr<FieldZoneTransfer>& transfer) override;
    };
}
