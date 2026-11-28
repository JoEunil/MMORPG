#pragma once

#include <cstddef>
#include "ZoneState.h"
#include "ZoneHandler.h"
#include "../CorePerfCollector.h"

namespace Core {
    class ZoneTickRunner {
        ZoneState& zone;
        ZoneHandler& handler;
        CorePerfCollector& perf;

    public:
        ZoneTickRunner(ZoneState& state, ZoneHandler& eventHandler, CorePerfCollector& collector)
            : zone(state), handler(eventHandler), perf(collector) {}

        enum class TickSnapshot {
            None,
            Delta,
            Full
        };

        template <class Queue, class SelectSnapshot>
        size_t RunTick(Queue& queue, size_t maxEvents, SelectSnapshot selectSnapshot) {
            const auto zoneID = zone.GetZoneID();
            zone.SkillCoolDown();
            zone.ReplenishMoveBudget();
            size_t processed = 0;
            size_t packets = 0;
            {
                ZoneEvent event;
                while (processed < maxEvents && queue.Dequeue(event)) {
                    ++processed;
                    packets += handler.ProcessEvent(zone, zoneID, event);
                }
            }
            perf.AddPacketProcessCnt(zoneID, packets);
            zone.ApplySkill();
            zone.UpdateMonster();
            zone.FlushCheat();
            switch (selectSnapshot()) {
            case TickSnapshot::Full:
                BroadcastFullState(zone);
                break;
            case TickSnapshot::Delta:
                BroadcastDeltaState(zone);
                break;
            case TickSnapshot::None:
                break;
            }
            return processed;
        }

    private:
        static void BroadcastDeltaState(ZoneState& zone) {
            zone.DeltaSnapshot();
            zone.DeltaSnapshotMonster();
            zone.ActionSnapshot();
        }
        static void BroadcastFullState(ZoneState& zone) {
            zone.FullSnapshot();
            zone.FullSnapshotMonster();
            zone.ActionSnapshot();
        }
    };
}
