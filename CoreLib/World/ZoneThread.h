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
    class ZoneTickRunner;

    using ZoneEventQueue = Base::NonBlockingBackPressureQueue<ZoneEvent, ZONE_QUEUE_SIZE, 0>;

    class alignas(std::hardware_destructive_interference_size) ZoneThread {
        std::thread thread;
        ZoneEventQueue workQueue;
        std::atomic<bool> running = false;
        uint16_t zoneID = 0;
        ZoneHandler* handler = nullptr;
        ZoneState* zone = nullptr;
        ZoneTickRunner* tickRunner = nullptr;
        CorePerfCollector* perfCollector = nullptr;

        void Initialize(ZoneState& state, ZoneTickRunner& runner, ZoneHandler& zoneHandler, CorePerfCollector& perf);
        bool IsReady() const;
        void Start();
        void Stop();
        bool CanEnqueue(const ZoneEvent& event) const;
        bool Enqueue(ZoneEvent& event);
        bool EnqueueDeferred(ZoneEvent& event);
        void WorkerFunc();

        friend class ZoneEventRouter;
    public:
        ~ZoneThread();
    };
}
