#pragma once

#include <array>
#include <atomic>

#include "ISessionSnapshotReader.h"
#include "IZoneEventQueue.h"
#include "ZoneThread.h"

namespace Core {
    class NonZoneThreadPool;

    class ZoneEventRouter final : public IZoneEventQueue, public IZoneTransfer, public ISessionSnapshotReader {
        struct ZoneEntry {
            std::unique_ptr<ZoneState> state;
            ZoneThread worker;
        };
        std::array<ZoneEntry, ZONE_COUNT> zones;
        std::atomic<bool> acceptingTransfers = false;

        NonZoneThreadPool* nonZoneWorkers = nullptr;

        void Initialize(ZoneHandler& handler, CorePerfCollector& perf, NonZoneThreadPool& nonZone);
        bool IsReady() const;
        void Start();
        void Stop();

        friend class Initializer;
    public:
        ZoneEventRouter();
        ~ZoneEventRouter() override;
        ZoneState* GetZone(uint16_t zoneID);
        std::optional<SessionSnapshotView> ReadSessionSnapshot(uint16_t zoneID) const override;
        bool TryEnqueue(uint16_t zoneID, ZoneEvent& event) override;
        uint16_t FindDestination(uint16_t sourceZoneID, float x, float y) const override;
        bool TryEnqueueTransfer(uint16_t destinationID, const std::shared_ptr<FieldZoneTransfer>& transfer) override;
        bool TryEnqueueTransferRejected(const std::shared_ptr<FieldZoneTransfer>& transfer) override;
        void EnqueueLobbyReturn(const std::shared_ptr<FieldZoneTransfer>& transfer) override;
    };
}
