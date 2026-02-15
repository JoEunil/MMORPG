#pragma once
#include <cstdint>

#include "MessageTypes.h"
#include "MessagePool.h"
#include "StateManager.h"
#include "PacketTypes.h"

namespace Core {
    class ISessionAuth;
    class IPacketView;
    class IIOCP;
    class IDBCache;
    class PacketWriter;
    class IMessageQueue;

    class ZoneHandler {
        StateManager* stateManager;
        
        void Initialize(StateManager* m) {
            stateManager = m;
        }

        bool IsReady() {
            if (stateManager == nullptr) {
                sysLogger->LogError("zone handler", "stateManager not initialized");
                return false;
            }
            return true;
        }
        
        void ProcessAction(ActionRequestBody* body, uint64_t sessionID, uint16_t zoneID);
        friend class Initializer;
    public:
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

