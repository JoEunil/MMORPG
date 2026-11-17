#include "pch.h"
#include "ZoneEventRouter.h"

#include "WorldMap.h"
#include "NonZoneThreadPool.h"
#include "LoggerGlobal.h"
#include "ZoneState.h"

namespace Core {
    ZoneEventRouter::ZoneEventRouter() = default;

    ZoneEventRouter::~ZoneEventRouter() {
        Stop();
    }

    void ZoneEventRouter::Initialize(ZoneHandler& handler, CorePerfCollector& perf, NonZoneThreadPool& nonZone) {
        nonZoneWorkers = &nonZone;
        for (std::size_t i = 0; i < zones.size(); ++i) {
            const auto zoneID = static_cast<uint16_t>(i + 1);
            zones[i].state = std::make_unique<ZoneState>(zoneID);
            zones[i].worker.Initialize(zoneID, handler, perf);
        }
    }

    bool ZoneEventRouter::IsReady() const {
        if (nonZoneWorkers == nullptr) {
            sysLogger->LogError("zone router", "non-zone workers not initialized");
            return false;
        }
        for (const auto& zone : zones) {
            if (!zone.worker.IsReady())
                return false;
        }
        return true;
    }

    void ZoneEventRouter::Start() {
        for (auto& zone : zones)
            zone.worker.Start(*zone.state);
        acceptingTransfers.store(true, std::memory_order_release);
    }

    void ZoneEventRouter::Stop() {
        acceptingTransfers.store(false, std::memory_order_release);
        for (auto& zone : zones)
            zone.worker.Stop();
    }

    ZoneState* ZoneEventRouter::GetZone(uint16_t zoneID) {
        return zoneID != 0 && zoneID <= zones.size() ? zones[zoneID - 1].state.get() : nullptr;
    }

    std::optional<SessionSnapshotView> ZoneEventRouter::ReadSessionSnapshot(uint16_t zoneID) const {
        if (zoneID == 0 || zoneID > zones.size())
            return std::nullopt;
        const auto& zone = zones[zoneID - 1].state;
        if (!zone)
            return std::nullopt;
        return zone->ReadSessionSnapshot();
    }

    bool ZoneEventRouter::TryEnqueue(uint16_t zoneID, ZoneEvent& event) {
        if (event.type == ZoneEventType::Transfer || event.type == ZoneEventType::TransferRejected
            || zoneID == 0 || zoneID > zones.size())
            return false;
        return zones[zoneID - 1].worker.Enqueue(event);
    }

    uint16_t ZoneEventRouter::FindDestination(uint16_t sourceZoneID, float x, float y) const {
        return WorldMap::FindTransitionZone(sourceZoneID, x, y);
    }

    bool ZoneEventRouter::TryEnqueueTransfer(uint16_t destinationID, const std::shared_ptr<FieldZoneTransfer>& transfer) {
        if (!transfer || destinationID == 0 || destinationID > zones.size()
            || !acceptingTransfers.load(std::memory_order_acquire))
            return false;
        ZoneEvent event{ZoneEventType::Transfer, nullptr, transfer};
        return zones[destinationID - 1].worker.Enqueue(event);
    }

    void ZoneEventRouter::EnqueueLobbyReturn(const std::shared_ptr<FieldZoneTransfer>& transfer) {
        nonZoneWorkers->EnqueueLobbyReturn(transfer);
    }

    bool ZoneEventRouter::TryEnqueueTransferRejected(const std::shared_ptr<FieldZoneTransfer>& transfer) {
        if (!transfer || transfer->sourceZoneID == 0 || transfer->sourceZoneID > zones.size()
            || !acceptingTransfers.load(std::memory_order_acquire))
            return false;
        ZoneEvent event{ZoneEventType::TransferRejected, nullptr, transfer};
        return zones[transfer->sourceZoneID - 1].worker.Enqueue(event);
    }
}
