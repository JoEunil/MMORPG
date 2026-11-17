#pragma once
#include "WorldConfig.h"
#include <cstdint>
#include "IPacketView.h"
#include "CorePerfCollector.h"
#include "ZoneThread.h"

#include "MessageTypes.h"
#include "MessagePool.h"
#include "StateManager.h"
#include "PacketTypes.h"
#include "INetworkSender.h"
#include "PacketWriter.h"

namespace Core {
    class ISessionAuth;
    class IPacketView;
    class INetworkSender;
    class IDBCache;
    class PacketWriter;
    class IMessageQueue;

    class ZoneHandler {
        StateManager* stateManager = nullptr;
        INetworkSender* networkSender = nullptr;
        PacketWriter* writer = nullptr;
        
        void Initialize(StateManager* m, INetworkSender* sender, PacketWriter* packetWriter) {
            stateManager = m;
            networkSender = sender;
            writer = packetWriter;
        }

        bool IsReady() const {
            if (stateManager == nullptr) {
                sysLogger->LogError("zone handler", "stateManager not initialized");
                return false;
            }
            if (networkSender == nullptr) {
                sysLogger->LogError("zone handler", "networkSender not initialized");
                return false;
            }
            if (writer == nullptr) {
                sysLogger->LogError("zone handler", "writer not initialized");
                return false;
            }
            return true;
        }
        
        void ProcessAction(const ActionRequestBody* body, uint64_t sessionID, uint16_t zoneID);
        friend class Initializer;
        friend class ZoneBenchmarkAccess;
    public:
        enum class TickSnapshot { None, Delta, Full };

        template<class Queue, class SelectSnapshot>
        size_t RunTick(ZoneState& zone, uint16_t zoneID, Queue& queue, size_t maxPackets,
            CorePerfCollector& perf, SelectSnapshot selectSnapshot) {
            zone.SkillCoolDown();
            zone.ReplenishMoveBudget();
            size_t processed = 0;
            {
                ZoneEvent event;
                while (processed < maxPackets && queue.pop(event)) {
                    switch (event.type) {
                    case ZoneEventType::Packet:
                        Process(event.packet.get(), zoneID);
                        ++processed;
                        break;
                    }
                }
            }
            perf.AddPacketProcessCnt(zoneID, processed);
            zone.ApplySkill();
            zone.UpdateMonster();
            zone.FlushCheat();
            switch (selectSnapshot()) {
            case TickSnapshot::Full: BroadcastFullState(zone); break;
            case TickSnapshot::Delta: BroadcastDeltaState(zone); break;
            case TickSnapshot::None: break;
            }
            return processed;
        }

        void Process(IPacketView* p, uint16_t zoneID);
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

