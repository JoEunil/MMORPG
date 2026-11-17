#pragma once

#include <array>

#include "IZoneEventQueue.h"
#include "ZoneThread.h"

namespace Core {
    class ZoneEventRouter final : public IZoneEventQueue {
        std::array<ZoneThread, ZONE_COUNT> zoneThreads;

        void Initialize(ZoneHandler& handler, CorePerfCollector& perf);
        bool IsReady() const;
        void Start(const std::array<ZoneState*, ZONE_COUNT>& zones);
        void Stop();

        friend class Initializer;
    public:
        bool TryEnqueue(uint16_t zoneID, ZoneEvent& event) override;
    };
}
