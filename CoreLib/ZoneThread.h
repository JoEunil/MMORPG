#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <new>
#include <thread>

#include <BaseLib/MPMCQueue.h>

#include "IPacketView.h"
#include "IZoneRequestQueue.h"
#include "IZoneTransfer.h"
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

    class alignas(std::hardware_destructive_interference_size) ZoneThread {
        std::thread thread;
        Base::MPMCQueue<ZoneEvent, ZONE_QUEUE_SIZE> workQueue;
        struct DeferredProfileUpdate {
            ProfileVersionUpdate update;
            DeferredProfileUpdate* next;
        };
        std::atomic<DeferredProfileUpdate*> deferredProfileUpdates = nullptr;
        std::atomic<bool> running = false;
        uint16_t zoneID = 0;
        ZoneHandler* handler = nullptr;
        CorePerfCollector* perfCollector = nullptr;

        void Initialize(uint16_t id, ZoneHandler& zoneHandler, CorePerfCollector& perf);
        bool IsReady() const;
        void Start(ZoneState& zone);
        void Stop();
        bool Enqueue(ZoneEvent& event);
        void ProcessDeferredProfileUpdates(ZoneState* zone);
        void WorkerFunc(ZoneState& zone);

        friend class ZoneEventRouter;
    public:
        ~ZoneThread();
    };
}
