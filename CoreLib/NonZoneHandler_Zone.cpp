#include "pch.h"
#include "NonZoneHandler.h"

#include "INetworkSender.h"
#include "IPacketView.h"
#include "LobbyZone.h"
#include "PacketTypes.h"
#include "PacketWriter.h"
#include "SessionManager.h"

namespace Core {
    void NonZoneHandler::ReturnToLobby(const std::shared_ptr<FieldZoneTransfer>& transfer) {
        const auto sessionID = transfer->character.sessionID;
        const auto epoch = lobbyZone->ReturnFieldTransfer(transfer, *chat);
        if (epoch == 0)
            return;

        auto response = writer->WriteZoneChangeLobby(epoch);
        if (response)
            networkSender->SendDataUnique(sessionID, std::move(response));
    }

    void NonZoneHandler::ZoneChange(IPacketView* p) {
        ZoneChangeResult result;
        const auto session = p->GetSessionID();
        const bool success = p->GetLength() == sizeof(PacketStruct<ZoneChangeBody>)
            && sessionManager->ChangeZone(session, parseBody<ZoneChangeBody>(p->GetPtr())->op, result);
        // ChangeZone releases the shard and all Zone locks before network transmission.
        auto response = success
            ? writer->WriteZoneChangeSucess(result)
            : writer->WriteZoneChangeFailed();
        if (response)
            networkSender->SendDataUnique(session, std::move(response));
        if (success)
            sessionManager->SendBootstrap(session, result.ownershipEpoch);
    }

    void NonZoneHandler::RespawnToTown(IPacketView* p) {
        ZoneChangeResult result;
        const auto session = p->GetSessionID();
        const bool validHeader = p->GetLength() == sizeof(PacketStruct<RespawnToTownBody>)
            && parseHeader(p->GetPtr())->magic == MAGIC
            && parseHeader(p->GetPtr())->opcode == OP::RESPAWN_TO_TOWN
            && parseHeader(p->GetPtr())->length == p->GetLength()
            && !IsSimulationFlag(parseHeader(p->GetPtr())->flags);
        const bool success = validHeader
            && sessionManager->RespawnToTown(session, *parseBody<RespawnToTownBody>(p->GetPtr()), result);
        auto response = success
            ? writer->WriteZoneChangeSucess(result)
            : writer->WriteZoneChangeFailed();
        if (response)
            networkSender->SendDataUnique(session, std::move(response));
        if (success)
            sessionManager->SendBootstrap(session, result.ownershipEpoch);
    }
}
