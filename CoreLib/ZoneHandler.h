#pragma once
#include <cstdint>

#include "MessageTypes.h"
#include "MessagePool.h"
#include "StateManager.h"
#include "PacketTypes.h"

namespace Core {
    class ILogger;
    class ISessionAuth;
    class IPacketView;
    class IIOCP;
    class IDBCache;
    class PacketWriter;
    class IMessageQueue;

    class ZoneHandler {
        ILogger* logger;
        StateManager* stateManager;
        
        void Initialize(ILogger* l, StateManager* m) {
            logger = l;
            stateManager = m;
        }

        bool IsReady() {
            if (logger == nullptr) return false;
            if (stateManager == nullptr) return false;
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

