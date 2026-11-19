#pragma once

#include <array>
#include <atomic>

#include "ISessionSnapshotReader.h"
#include "IZonePacketQueue.h"
#include "IZoneRequestQueue.h"
#include "ZoneThread.h"

namespace Core {
    class ILobbyReturnQueue;

    class ZoneEventRouter final : public IZonePacketQueue, public IZoneRequestQueue, public IZoneTransfer, public ISessionSnapshotReader {
        struct ZoneEntry {
            std::unique_ptr<ZoneState> state;
            ZoneThread worker;
        };
        std::array<ZoneEntry, ZONE_COUNT> zones;
        std::atomic<bool> acceptingTransfers = false;

        ILobbyReturnQueue* lobbyReturns = nullptr;

        void Initialize(ZoneHandler& handler, CorePerfCollector& perf, ILobbyReturnQueue& lobbyQueue);
        bool IsReady() const;
        void Start();
        void Stop();
        bool TryEnqueue(uint16_t zoneID, ZoneEvent& event);

        friend class Initializer;
    public:
        ZoneEventRouter();
        ~ZoneEventRouter() override;
        ZoneState* GetZone(uint16_t zoneID);
        std::optional<SessionSnapshotView> ReadSessionSnapshot(uint16_t zoneID) const override;
        bool TryEnqueuePacket(uint16_t zoneID, std::unique_ptr<IPacketView, PacketViewDeleter> packet) override;
        bool TryEnqueueLobbyEntry(uint16_t zoneID, uint64_t sessionID, uint64_t ownershipEpoch) override;
        bool TryEnqueueRespawn(uint16_t zoneID, uint64_t sessionID, uint64_t ownershipEpoch) override;
        bool TryEnqueueBootstrap(uint16_t zoneID, uint64_t sessionID, uint64_t ownershipEpoch, bool resendZoneChange) override;
        bool TryEnqueueProfileVersion(uint16_t zoneID, const ProfileVersionUpdate& update) override;
        uint16_t FindDestination(uint16_t sourceZoneID, float x, float y) const override;
        bool TryEnqueueTransfer(uint16_t destinationID, const std::shared_ptr<ZoneTransfer>& transfer) override;
        bool TryEnqueueTransferRejected(const std::shared_ptr<ZoneTransfer>& transfer) override;
        void EnqueueLobbyReturn(const std::shared_ptr<ZoneTransfer>& transfer) override;
    };
}
