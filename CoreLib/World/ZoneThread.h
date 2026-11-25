#pragma once

#include <atomic>
#include <cstdint>
#include <thread>

#include <BaseLib/NonBlockingBackPressureQueue.h>

#include "ZoneEvent.h"
#include "WorldConfig.h"

namespace Core {
    class CorePerfCollector;
    class ZoneHandler;
    class ZoneState;

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
