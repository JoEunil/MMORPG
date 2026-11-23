#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>

#include <BaseLib/NonBlockingBackPressureQueue.h>

#include "../Interface/IPacketView.h"
#include "Interface/IZoneRequestQueue.h"
#include "Interface/IZoneTransfer.h"
#include "WorldConfig.h"

namespace Core {
    class CorePerfCollector;
    class ZoneHandler;
    class ZoneState;

    enum class ZoneEventType : uint8_t {
        Packet,
        Transfer,
        TransferRejected,
        ProfileVersion,
        Bootstrap,
        LobbyEntry,
        Respawn,
        Disconnect,
    };

    struct ZoneRequest {
        uint64_t sessionID = 0;
        uint64_t ownershipEpoch = 0;
        bool resendZoneChange = false;
    };

    struct ZoneEvent {
        ZoneEventType type = ZoneEventType::Packet;
        std::unique_ptr<IPacketView, PacketViewDeleter> packet;
        std::shared_ptr<ZoneTransfer> transfer;
        ProfileVersionUpdate profileVersion;
        ZoneRequest request;
    };

    using ZoneEventQueue = Base::NonBlockingBackPressureQueue<ZoneEvent, ZONE_QUEUE_SIZE, 0>;

    class alignas(std::hardware_destructive_interference_size) ZoneThread {
        std::thread thread;
        ZoneEventQueue workQueue;
        std::atomic<bool> running = false;
        uint16_t zoneID = 0;
        ZoneHandler* handler = nullptr;
        CorePerfCollector* perfCollector = nullptr;

        void Initialize(uint16_t id, ZoneHandler& zoneHandler, CorePerfCollector& perf);
        bool IsReady() const;
        void Start(ZoneState& zone);
        void Stop();
        bool CanEnqueue(const ZoneEvent& event) const;
        bool Enqueue(ZoneEvent& event);
        bool EnqueueDeferred(ZoneEvent& event);
        void WorkerFunc(ZoneState& zone);

        friend class ZoneEventRouter;
    public:
        ~ZoneThread();
    };
}
