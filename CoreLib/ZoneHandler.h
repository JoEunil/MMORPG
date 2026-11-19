#pragma once
#include "WorldConfig.h"
#include <cstdint>
#include "IPacketView.h"
#include "CorePerfCollector.h"
#include "ZoneThread.h"

#include "MessageTypes.h"
#include "MessagePool.h"
#include "SessionManager.h"
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
    class LobbyZone;

    class ZoneHandler {
        SessionManager* sessionManager = nullptr;
        INetworkSender* networkSender = nullptr;
        PacketWriter* writer = nullptr;
        IZoneTransfer* zoneTransfer = nullptr;
        LobbyZone* lobbyZone = nullptr;
        
        void Initialize(SessionManager* m, INetworkSender* sender, PacketWriter* packetWriter,
            IZoneTransfer& transfer, LobbyZone& lobby) {
            sessionManager = m;
            networkSender = sender;
            writer = packetWriter;
            zoneTransfer = &transfer;
            lobbyZone = &lobby;
        }

        bool IsReady() const {
            if (sessionManager == nullptr) {
                sysLogger->LogError("zone handler", "sessionManager not initialized");
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
            if (zoneTransfer == nullptr) {
                sysLogger->LogError("zone handler", "zone transfer not initialized");
                return false;
            }
            if (lobbyZone == nullptr) {
                sysLogger->LogError("zone handler", "lobby zone not initialized");
                return false;
            }
            return true;
        }
        
        void ProcessAction(ZoneState& zone, const ActionRequestBody& action, uint64_t sessionID, uint16_t zoneID);
        ParryResponseBody ProcessParry(ZoneState& zone, uint64_t sessionID, uint16_t zoneID, const ParryRequestBody& request);
        void ProcessZoneTransfer(ZoneState& zone, const ZoneEvent& event);
        void ApplyProfileVersion(ZoneState& zone, const ProfileVersionUpdate& update);
        void ProcessLobbyEntry(ZoneState& zone, const ZoneRequest& request);
        void ProcessRespawn(ZoneState& zone, const ZoneRequest& request);
        std::shared_ptr<ZoneTransfer> StartRespawnTransfer(ZoneState& source, uint64_t sessionID, uint64_t characterID, uint64_t ownershipEpoch);
        void ProcessBootstrap(ZoneState& zone, const ZoneRequest& request);
        void SendZoneChangeFailed(uint64_t sessionID);
        void SendZoneChange(ZoneState& zone, uint64_t sessionID, const ZoneChangeResult& transition);
        friend class Initializer;
        friend class ZoneThread;
        friend class ZoneBenchmarkAccess;
    public:
        enum class TickSnapshot { None, Delta, Full };

        template<class Queue, class SelectSnapshot>
        size_t RunTick(ZoneState& zone, uint16_t zoneID, Queue& queue, size_t maxEvents,
            CorePerfCollector& perf, SelectSnapshot selectSnapshot) {
            zone.SkillCoolDown();
            zone.ReplenishMoveBudget();
            size_t processed = 0;
            size_t packets = 0;
            {
                ZoneEvent event;
                while (processed < maxEvents && queue.pop(event)) {
                    ++processed;
                    switch (event.type) {
                    case ZoneEventType::Packet:
                        Process(zone, event.packet.get(), zoneID);
                        ++packets;
                        break;
                    case ZoneEventType::Transfer:
                    case ZoneEventType::TransferRejected:
                        ProcessZoneTransfer(zone, event);
                        break;
                    case ZoneEventType::ProfileVersion:
                        ApplyProfileVersion(zone, event.profileVersion);
                        break;
                    case ZoneEventType::Bootstrap:
                        ProcessBootstrap(zone, event.request);
                        break;
                    case ZoneEventType::LobbyEntry:
                        ProcessLobbyEntry(zone, event.request);
                        break;
                    case ZoneEventType::Respawn:
                        ProcessRespawn(zone, event.request);
                        break;
                    }
                }
            }
            perf.AddPacketProcessCnt(zoneID, packets);
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

        void Process(ZoneState& zone, IPacketView* p, uint16_t zoneID);
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
