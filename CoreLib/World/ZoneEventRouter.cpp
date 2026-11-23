#include "pch.h"
#include "ZoneEventRouter.h"

#include "WorldMap.h"
#include "Interface/ILobbyReturnQueue.h"
#include "../LoggerGlobal.h"
#include "ZoneState.h"

namespace Core {
    ZoneEventRouter::ZoneEventRouter() = default;

    ZoneEventRouter::~ZoneEventRouter() {
        Stop();
    }

    void ZoneEventRouter::Initialize(ZoneHandler& handler, CorePerfCollector& perf, ILobbyReturnQueue& lobbyQueue) {
        lobbyReturns = &lobbyQueue;
        for (std::size_t i = 0; i < zones.size(); ++i) {
            const auto zoneID = static_cast<uint16_t>(i + 1);
            zones[i].state = std::make_unique<ZoneState>(zoneID);
            zones[i].worker.Initialize(zoneID, handler, perf);
        }
    }

    bool ZoneEventRouter::IsReady() const {
        if (lobbyReturns == nullptr) {
            sysLogger->LogError("zone router", "lobby return queue not initialized");
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

    std::optional<SessionSnapshotView> ZoneEventRouter::ReadSessionSnapshot(uint16_t zoneID) const {
        if (zoneID == 0 || zoneID > zones.size())
            return std::nullopt;
        const auto& zone = zones[zoneID - 1].state;
        if (!zone)
            return std::nullopt;
        return zone->ReadSessionSnapshot();
    }

    bool ZoneEventRouter::TryEnqueue(uint16_t zoneID, ZoneEvent& event) {
        if (zoneID == 0 || zoneID > zones.size())
            return false;
        return zones[zoneID - 1].worker.Enqueue(event);
    }

    bool ZoneEventRouter::TryEnqueueDeferred(uint16_t zoneID, ZoneEvent& event) {
        if (zoneID == 0 || zoneID > zones.size())
            return false;
        return zones[zoneID - 1].worker.EnqueueDeferred(event);
    }

    bool ZoneEventRouter::TryEnqueuePacket(uint16_t zoneID, std::unique_ptr<IPacketView, PacketViewDeleter> packet) {
        if (!packet)
            return false;
        ZoneEvent event{ZoneEventType::Packet, std::move(packet)};
        return TryEnqueue(zoneID, event);
    }

    bool ZoneEventRouter::TryEnqueueLobbyEntry(uint16_t zoneID, uint64_t sessionID, uint64_t ownershipEpoch) {
        ZoneEvent event{ZoneEventType::LobbyEntry};
        event.request = {sessionID, ownershipEpoch};
        return TryEnqueue(zoneID, event);
    }

    bool ZoneEventRouter::TryEnqueueRespawn(uint16_t zoneID, uint64_t sessionID, uint64_t ownershipEpoch) {
        ZoneEvent event{ZoneEventType::Respawn};
        event.request = {sessionID, ownershipEpoch};
        return TryEnqueue(zoneID, event);
    }

    bool ZoneEventRouter::TryEnqueueBootstrap(uint16_t zoneID, uint64_t sessionID, uint64_t ownershipEpoch, bool resendZoneChange) {
        ZoneEvent event{ZoneEventType::Bootstrap};
        event.request = {sessionID, ownershipEpoch, resendZoneChange};
        return TryEnqueue(zoneID, event);
    }

    bool ZoneEventRouter::TryEnqueueProfileVersion(uint16_t zoneID, const ProfileVersionUpdate& update) {
        ZoneEvent event{ZoneEventType::ProfileVersion};
        event.profileVersion = update;
        return TryEnqueueDeferred(zoneID, event);
    }

    uint16_t ZoneEventRouter::FindDestination(uint16_t sourceZoneID, float x, float y) const {
        return WorldMap::FindTransitionZone(sourceZoneID, x, y);
    }

    bool ZoneEventRouter::TryEnqueueDisconnect(uint16_t zoneID, uint64_t sessionID, uint64_t ownershipEpoch) {
        ZoneEvent event{ZoneEventType::Disconnect};
        event.request = {sessionID, ownershipEpoch};
        return TryEnqueueDeferred(zoneID, event);
    }

    bool ZoneEventRouter::TryEnqueueTransfer(uint16_t destinationID, const std::shared_ptr<ZoneTransfer>& transfer) {
        if (!transfer || !acceptingTransfers.load(std::memory_order_acquire))
            return false;
        ZoneEvent event{ZoneEventType::Transfer, nullptr, transfer};
        return TryEnqueue(destinationID, event);
    }

    void ZoneEventRouter::EnqueueLobbyReturn(const std::shared_ptr<ZoneTransfer>& transfer) {
        lobbyReturns->EnqueueLobbyReturn(transfer);
    }

    bool ZoneEventRouter::TryEnqueueTransferRejected(const std::shared_ptr<ZoneTransfer>& transfer) {
        if (!transfer || !acceptingTransfers.load(std::memory_order_acquire))
            return false;
        ZoneEvent event{ZoneEventType::TransferRejected, nullptr, transfer};
        return TryEnqueue(transfer->sourceZoneID, event);
    }
}
