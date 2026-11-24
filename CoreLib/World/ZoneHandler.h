#pragma once
#include "WorldConfig.h"
#include <cstdint>
#include "../Interface/IPacketView.h"
#include "../CorePerfCollector.h"
#include "ZoneThread.h"
#include "Interface/IZoneTransfer.h"

#include "../Cache/MessageTypes.h"
#include "../Cache/MessagePool.h"
#include "SessionManager.h"
#include "../Packet/PacketTypes.h"

namespace Core {
    class CacheCommandSender;
    class ISessionAuth;
    class IPacketView;
    class IServerResponseQueue;
    class PacketWriter;
    class IMessageQueue;
    class LobbyZone;
    class IInventoryOperations;

    class ZoneHandler {
        IServerResponseQueue* responses = nullptr;
        PacketWriter* writer = nullptr;
        SessionManager* sessionManager = nullptr;
        IZoneTransfer* zoneTransfer = nullptr;
        IZoneRequestQueue* zoneRequests = nullptr;
        IInventoryOperations* inventory = nullptr;
        const CacheCommandSender* cacheCommands = nullptr;
        LobbyZone* lobbyZone = nullptr;

        void Initialize(SessionManager* m, IServerResponseQueue& responseQueue, PacketWriter& packetWriter, IZoneTransfer& transfer, LobbyZone& lobby,
            const CacheCommandSender& commands, IZoneRequestQueue& requests, IInventoryOperations* items) {
            sessionManager = m;
            responses = &responseQueue;
            writer = &packetWriter;
            zoneTransfer = &transfer;
            zoneRequests = &requests;
            inventory = items;
            cacheCommands = &commands;
            lobbyZone = &lobby;
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
            if (lobbyZone == nullptr) {
                sysLogger->LogError("zone handler", "lobby zone not initialized");
                return false;
            }
            if (cacheCommands == nullptr) {
                sysLogger->LogError("zone handler", "cache command sender not initialized");
                return false;
            }
            return true;
        }

        void ProcessAction(ZoneState& zone, const ActionRequestBody& action, uint64_t sessionID, uint16_t zoneID);
        void ProcessItemUse(ZoneState& zone, IPacketView* packet);
        void ProcessEquipment(ZoneState& zone, IPacketView* packet, uint16_t zoneID);
        ParryResponseBody ProcessParry(ZoneState& zone, uint64_t sessionID, uint16_t zoneID, const ParryRequestBody& request);
        void ProcessZoneTransfer(ZoneState& zone, const std::shared_ptr<ZoneTransfer>& transfer, bool returning);
        void ProcessLobbyEntry(ZoneState& zone, const ZoneLobbyEntryEvent& request);
        void ProcessRespawn(ZoneState& zone, const ZoneRespawnEvent& request);
        void ProcessBootstrap(ZoneState& zone, const ZoneBootstrapEvent& request);
        void SendZoneChangeFailed(uint64_t sessionID);
        void SendZoneChange(uint64_t sessionID, const ZoneChangeResult& transition);

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
        void ApplyProfileVersion(ZoneState& zone, const ProfileVersionUpdate& update);

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
