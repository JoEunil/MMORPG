#pragma once
#include "WorldConfig.h"
#include <cstdint>
#include "../Interface/IPacketView.h"
#include "../CorePerfCollector.h"
#include "ZoneThread.h"
#include "Interface/IZoneTransfer.h"

#include "SessionManager.h"
#include "../Packet/PacketTypes.h"

namespace Core {
    class IServerResponseQueue;
    class PacketWriter;
    class ZoneEventHandler;

    class ZoneHandler {
        IServerResponseQueue* responses = nullptr;
        PacketWriter* writer = nullptr;
        SessionManager* sessionManager = nullptr;
        IZoneTransfer* zoneTransfer = nullptr;
        IZoneRequestQueue* zoneRequests = nullptr;
        ZoneEventHandler* events = nullptr;

        void Initialize(SessionManager* m, IServerResponseQueue& responseQueue, PacketWriter& packetWriter, IZoneTransfer& transfer, IZoneRequestQueue& requests, ZoneEventHandler& eventHandler) {
            sessionManager = m;
            responses = &responseQueue;
            writer = &packetWriter;
            zoneTransfer = &transfer;
            zoneRequests = &requests;
            events = &eventHandler;
        }

        bool IsReady() const {
            if (sessionManager == nullptr) {
                sysLogger->LogError("zone handler", "sessionManager not initialized");
                return false;
            }
            if (writer == nullptr) {
                sysLogger->LogError("zone handler", "packet writer not initialized");
                return false;
            }
            if (responses == nullptr) {
                sysLogger->LogError("zone handler", "response queue not initialized");
                return false;
            }
            if (zoneRequests == nullptr) {
                sysLogger->LogError("zone handler", "zone request queue not initialized");
                return false;
            }
            if (zoneTransfer == nullptr) {
                sysLogger->LogError("zone handler", "zone transfer not initialized");
                return false;
            }
            if (events == nullptr) {
                sysLogger->LogError("zone handler", "zone event handler not initialized");
                return false;
            }
            return true;
        }

        void ProcessAction(ZoneState& zone, const ActionRequestBody& action, uint64_t sessionID, uint16_t zoneID);
        void ProcessItemUse(ZoneState& zone, IPacketView* packet);
        void ProcessEquipment(ZoneState& zone, IPacketView* packet, uint16_t zoneID);
        ParryResponseBody ProcessParry(ZoneState& zone, uint64_t sessionID, uint16_t zoneID, const ParryRequestBody& request);

        struct EventVisitor {
            ZoneHandler& handler;
            ZoneState& zone;
            uint16_t zoneID;

            size_t operator()(const ZonePacketEvent& event) const;
            size_t operator()(const ZoneTransferEvent& event) const;
            size_t operator()(const ZoneTransferRejectedEvent& event) const;
            size_t operator()(const ZoneProfileVersionEvent& event) const;
            size_t operator()(const ZoneBootstrapEvent& event) const;
            size_t operator()(const ZoneLobbyEntryEvent& event) const;
            size_t operator()(const ZoneRespawnEvent& event) const;
            size_t operator()(const ZoneDisconnectEvent& event) const;
        };

        size_t Process(ZoneState& zone, uint16_t zoneID, ZoneEvent& event);
        friend class Initializer;
        friend class ZoneBenchmarkAccess;

      public:
        enum class TickSnapshot {
            None,
            Delta,
            Full
        };

        template <class Queue, class SelectSnapshot>
        size_t RunTick(ZoneState& zone, uint16_t zoneID, Queue& queue, size_t maxEvents, CorePerfCollector& perf, SelectSnapshot selectSnapshot) {
            zone.SkillCoolDown();
            zone.ReplenishMoveBudget();
            size_t processed = 0;
            size_t packets = 0;
            {
                ZoneEvent event;
                while (processed < maxEvents && queue.Dequeue(event)) {
                    ++processed;
                    packets += Process(zone, zoneID, event);
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

        void SaveRemainingCharacters(ZoneState& zone);
        void Process(ZoneState& zone, IPacketView* p, uint16_t zoneID);
        void ProcessDisconnect(ZoneState& zone, uint16_t zoneID, const ZoneDisconnectEvent& request);

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
