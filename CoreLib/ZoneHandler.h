#pragma once
#include <cstdint>
#include "IPacketView.h"
#include "CorePerfCollector.h"

#include "MessageTypes.h"
#include "MessagePool.h"
#include "StateManager.h"
#include "PacketTypes.h"
#include "IIOCP.h"
#include "PacketWriter.h"

namespace Core {
    class ISessionAuth;
    class IPacketView;
    class IIOCP;
    class IDBCache;
    class PacketWriter;
    class IMessageQueue;

    class ZoneHandler {
        StateManager* stateManager = nullptr;
        IIOCP* iocp = nullptr;
        PacketWriter* writer = nullptr;
        
        void Initialize(StateManager* m, IIOCP* io, PacketWriter* packetWriter) {
            stateManager = m;
            iocp = io;
            writer = packetWriter;
        }

        bool IsReady() {
            if (stateManager == nullptr) {
                sysLogger->LogError("zone handler", "stateManager not initialized");
                return false;
            }
            if (iocp == nullptr || writer == nullptr) return false;
            return true;
        }
        
        void ProcessAction(const ActionRequestBody* body, uint64_t sessionID, uint16_t zoneID);
        friend class Initializer;
        friend class ZoneBenchmarkAccess;
    public:
        enum class TickSnapshot { None, Delta, Full };

        template<class Queue, class SelectSnapshot>
        size_t RunTick(uint16_t zoneID, Queue& queue, size_t maxPackets,
            CorePerfCollector& perf, SelectSnapshot selectSnapshot) {
            SkillCoolDown(zoneID);
            ReplenishMoveBudget(zoneID);
            size_t processed = 0;
            {
                std::unique_ptr<IPacketView, PacketViewDeleter> packet;
                while (processed < maxPackets && queue.pop(packet)) {
                    Process(packet.get(), zoneID);
                    ++processed;
                }
            }
            perf.AddPacketProcessCnt(zoneID, processed);
            ApplySkill(zoneID);
            UpdateMonster(zoneID);
            FlushCheat(zoneID);
            switch (selectSnapshot()) {
            case TickSnapshot::Full: BroadcastFullState(zoneID); break;
            case TickSnapshot::Delta: BroadcastDeltaState(zoneID); break;
            case TickSnapshot::None: break;
            }
            return processed;
        }

        void Process(IPacketView* p, uint16_t zoneID);
        void BroadcastDeltaState(uint16_t zoneID) {
            auto zoneState = stateManager->GetZone(zoneID);
            zoneState->DeltaSnapshot();
            zoneState->DeltaSnapshotMonster();
            zoneState->ActionSnapshot();
        }
        void BroadcastFullState(uint16_t zoneID) {
            auto zoneState = stateManager->GetZone(zoneID);
            zoneState->FullSnapshot();
            zoneState->FullSnapshotMonster();
            zoneState->ActionSnapshot();
        }
        void FlushCheat(uint16_t zoneID) {
            auto zoneState = stateManager->GetZone(zoneID);
            zoneState->FlushCheat();
        }
        void UpdateMonster(uint16_t zoneID) {
            auto zoneState = stateManager->GetZone(zoneID);
            zoneState->UpdateMonster();
        }
        void SkillCoolDown(uint16_t zoneID) {
            auto zoneState = stateManager->GetZone(zoneID);
            zoneState->SkillCoolDown();
        }
        void ReplenishMoveBudget(uint16_t zoneID) {
            auto zoneState = stateManager->GetZone(zoneID);
            zoneState->ReplenishMoveBudget();
        }
        void ApplySkill(uint16_t zoneID) {
            auto zoneState = stateManager->GetZone(zoneID);
            zoneState->ApplySkill();
        }
        void SpawnMonster(uint16_t zoneID) {
            auto zoneState = stateManager->GetZone(zoneID);
            zoneState->InitializeMonster();
        }
    };
}

