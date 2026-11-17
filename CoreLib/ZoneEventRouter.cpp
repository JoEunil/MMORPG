#include "pch.h"
#include "ZoneEventRouter.h"

#include "LoggerGlobal.h"

#include <algorithm>

namespace Core {
    void ZoneEventRouter::Initialize(ZoneHandler& handler, CorePerfCollector& perf) {
        for (std::size_t i = 0; i < zoneThreads.size(); ++i)
            zoneThreads[i].Initialize(static_cast<uint16_t>(i + 1), handler, perf);
    }

    bool ZoneEventRouter::IsReady() const {
        for (const auto& worker : zoneThreads) {
            if (!worker.IsReady())
                return false;
        }
        return true;
    }

    void ZoneEventRouter::Start(const std::array<ZoneState*, ZONE_COUNT>& zones) {
        const auto missing = std::find(zones.begin(), zones.end(), nullptr);
        if (missing != zones.end()) {
            sysLogger->LogError("zone event router", "zone state not initialized", "zoneID", missing - zones.begin() + 1);
            return;
        }
        for (std::size_t i = 0; i < zoneThreads.size(); ++i)
            zoneThreads[i].Start(*zones[i]);
    }

    void ZoneEventRouter::Stop() {
        for (auto& worker : zoneThreads)
            worker.Stop();
    }

    bool ZoneEventRouter::TryEnqueue(uint16_t zoneID, ZoneEvent& event) {
        if (zoneID == 0 || zoneID > zoneThreads.size())
            return false;
        return zoneThreads[zoneID - 1].Enqueue(event);
    }
}
