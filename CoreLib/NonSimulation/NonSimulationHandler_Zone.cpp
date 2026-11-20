#include "pch.h"
#include "NonSimulationHandler.h"

#include "../Interface/INetworkSender.h"
#include "../Interface/IPacketView.h"
#include "../World/LobbyZone.h"
#include "../Packet/PacketTypes.h"
#include "../Packet/PacketWriter.h"
#include "../World/SessionManager.h"
#include "../World/Interface/IZoneRequestQueue.h"

namespace Core {
    void NonSimulationHandler::ReturnToLobby(const std::shared_ptr<ZoneTransfer>& transfer) {
        const auto sessionID = transfer->character.sessionID;
        const auto epoch = lobbyZone->ReturnZoneTransfer(transfer);
        if (!epoch)
            return;

        auto response = writer->WriteZoneChangeLobby(*epoch);
        if (response)
            networkSender->SendDataUnique(sessionID, std::move(response));
    }

    void NonSimulationHandler::ZoneChange(IPacketView* p) {
        const auto session = p->GetSessionID();
        if (p->GetLength() == sizeof(PacketStruct<ZoneChangeBody>)) {
            const auto request = sessionManager->GetLobbyEntry(session, parseBody<ZoneChangeBody>(p->GetPtr())->op);
            if (request) {
                const auto destination = lobbyZone->FindEntryZone(session, request->characterID);
                if (destination != 0 && zoneRequests->TryEnqueueLobbyEntry(destination, session, request->ownershipEpoch)) return;
            }
        }
        auto response = writer->WriteZoneChangeFailed();
        if (response)
            networkSender->SendDataUnique(session, std::move(response));
    }

    void NonSimulationHandler::RespawnToTown(IPacketView* p) {
        const auto session = p->GetSessionID();
        const bool validHeader = p->GetLength() == sizeof(PacketStruct<RespawnToTownBody>)
            && parseHeader(p->GetPtr())->magic == MAGIC
            && parseHeader(p->GetPtr())->opcode == OP::RESPAWN_TO_TOWN
            && parseHeader(p->GetPtr())->length == p->GetLength()
            && !IsSimulationFlag(parseHeader(p->GetPtr())->flags);
        if (validHeader) {
            const auto route = sessionManager->GetRespawnRoute(session, parseBody<RespawnToTownBody>(p->GetPtr())->ownershipEpoch);
            if (route && (route->alreadyPending || zoneRequests->TryEnqueueRespawn(route->zoneID, session, route->ownershipEpoch))) return;
        }
        auto response = writer->WriteZoneChangeFailed();
        if (response)
            networkSender->SendDataUnique(session, std::move(response));
    }
}
