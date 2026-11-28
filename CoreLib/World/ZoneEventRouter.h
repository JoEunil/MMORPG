#pragma once

#include <array>
#include <atomic>

#include "Interface/ISessionSnapshotReader.h"
#include "Interface/IZonePacketQueue.h"
#include "Interface/IZoneRequestQueue.h"
#include "Interface/IZoneTransfer.h"
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
        bool TryEnqueueDeferred(uint16_t zoneID, ZoneEvent& event);

        friend class Initializer;
    public:
        ZoneEventRouter();
        ~ZoneEventRouter() override;
        std::optional<SessionSnapshotView> ReadSessionSnapshot(uint16_t zoneID) const override;
        bool TryEnqueuePacket(uint16_t zoneID, std::unique_ptr<IPacketView, PacketViewDeleter> packet) override;
        bool TryEnqueueLobbyEntry(const ZoneSessionContext& context) override;
        bool TryEnqueueRespawn(const ZoneSessionContext& context) override;
        bool TryEnqueueBootstrap(const ZoneSessionContext& context, bool resendZoneChange) override;
        bool TryEnqueueProfileVersion(uint16_t zoneID, const ProfileVersionUpdate& update) override;
        bool TryEnqueueDisconnect(const ZoneSessionContext& context) override;
        uint16_t FindDestination(uint16_t sourceZoneID, float x, float y) const override;
        bool TryEnqueueTransfer(uint16_t destinationID, const std::shared_ptr<ZoneTransfer>& transfer) override;
        bool TryEnqueueTransferRejected(const std::shared_ptr<ZoneTransfer>& transfer) override;
        void EnqueueLobbyReturn(const std::shared_ptr<ZoneTransfer>& transfer) override;
    };
}
