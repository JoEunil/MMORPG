#include "pch.h"
#include "ZoneEventHandler.h"
#include "ZoneState.h"
#include "SessionManager.h"
#include "LobbyZone.h"
#include "Interface/IZoneTransfer.h"
#include "../Cache/CacheCommandSender.h"
#include "../Response/Interface/IServerResponseQueue.h"
#include "../Packet/PacketWriter.h"

namespace Core {
    void ZoneEventHandler::ProcessLobbyEntry(ZoneState& zone, const ZoneLobbyEntryEvent& request) {
        const auto sessionID = request.sessionID;
        ZoneChangeResult result;
        const auto outcome = lobbyZone->EnterField(sessionID, request.ownershipEpoch, zone, result);
        if (outcome == ZoneOperationResult::Completed)
            SendZoneChange(sessionID, result);
        else if (outcome == ZoneOperationResult::Rejected)
            SendZoneChangeFailed(sessionID);
    }

    void ZoneEventHandler::ProcessRespawn(ZoneState& zone, const ZoneRespawnEvent& request) {
        const auto sessionID = request.sessionID;
        const auto respawn = sessionManager->CheckRespawnRequest({sessionID, zone.GetZoneID(), request.ownershipEpoch});
        if (respawn.status == RespawnRequest::Status::Ignored)
            return;
        if (respawn.status == RespawnRequest::Status::Rejected) {
            SendZoneChangeFailed(sessionID);
            return;
        }
        if (zone.GetZoneID() != TOWN_ZONE_ID) {
            if (!zone.StartRespawnTransfer(sessionID, respawn.characterID, request.ownershipEpoch, *zoneTransfer))
                SendZoneChangeFailed(sessionID);
            return;
        }

        ZoneChangeResult result;
        if (!zone.DescribeOrRespawnInTown(sessionID, respawn.characterID, respawn.status == RespawnRequest::Status::Replay, result)) {
            SendZoneChangeFailed(sessionID);
            return;
        }
        if (sessionManager->CompleteTownRespawn(sessionID, request.ownershipEpoch, result))
            SendZoneChange(sessionID, result);
    }

    void ZoneEventHandler::ProcessZoneTransfer(ZoneState& zone, const std::shared_ptr<ZoneTransfer>& transfer, bool returning) {
        if (!transfer)
            return;

        const auto sessionID = transfer->character.sessionID;
        const auto snapshot = sessionManager->ReadZoneTransfer(transfer, zone.GetZoneID(), returning);
        if (!snapshot)
            return;

        ZoneChangeResult result;
        if (!zone.TryResolveZoneTransfer(*snapshot, returning, result)) {
            if (returning || !zoneTransfer->TryEnqueueTransferRejected(transfer))
                zoneTransfer->EnqueueLobbyReturn(transfer);
            return;
        }

        const auto committed = sessionManager->CompleteZoneTransfer(transfer, zone.GetZoneID(), returning);
        if (!committed) {
            // Disconnect may have taken the pending record while admission was in progress.
            CharacterState discarded;
            if (!zone.EmigrateChar(sessionID, discarded))
                errorLogger->LogError("zone handler", "could not roll back transfer admission", {{"sessionID", sessionID}});
            return;
        }
        zone.UpdateProfileVersion(sessionID, committed->profileID, committed->profileVersion);
        if (returning) {
            if (snapshot->IsRespawn())
                SendZoneChangeFailed(sessionID);
            return;
        }
        result.chatID = committed->chatID;
        result.ownershipEpoch = committed->ownershipEpoch;
        if (!snapshot->IsRespawn()) {
            zone.Skill(sessionID, snapshot->skillSlot, snapshot->aimAngle);
            zone.DirtyCheck(sessionID);
        }
        SendZoneChange(sessionID, result);
    }

    void ZoneEventHandler::ApplyProfileVersion(ZoneState& zone, const ProfileVersionUpdate& update) {
        auto route = sessionManager->RouteProfileVersion(update);
        if (route && *route == zone.GetZoneID()) {
            zone.UpdateProfileVersion(update.sessionID, update.profileID, update.version);
            return;
        }
        if (route && *route == 0)
            route = lobbyZone->ApplyProfileVersion(update);
        if (route && !zoneRequests->TryEnqueueProfileVersion(*route, update))
            errorLogger->LogWarn("zone handler", "could not route profile version", {{"sessionID", update.sessionID}, {"zoneID", *route}});
    }

    void ZoneEventHandler::ProcessDisconnect(ZoneState& zone, uint16_t zoneID, const ZoneDisconnectEvent& request) {
        std::shared_ptr<ZoneTransfer> pending;
        if (!sessionManager->CompleteZoneDisconnect({request.sessionID, zoneID, request.ownershipEpoch}, pending))
            return;

        if (pending) {
            cacheCommands->EnqueueCharacterStateUpdate(request.sessionID, pending->character);
            return;
        }
        CharacterState character;
        if (zone.EmigrateChar(request.sessionID, character))
            cacheCommands->EnqueueCharacterStateUpdate(request.sessionID, character);
        else
            gameLogger->LogWarn("zone handler", "disconnect character not found", {{"sessionID", request.sessionID}, {"zoneID", zoneID}});
    }

    void ZoneEventHandler::ProcessBootstrap(ZoneState& zone, const ZoneBootstrapEvent& request) {
        const auto info = sessionManager->PrepareZoneBootstrap({request.sessionID, zone.GetZoneID(), request.ownershipEpoch}, request.resendZoneChange);
        if (!info)
            return;
        if (request.resendZoneChange) {
            ZoneChangeResult transition;
            if (zone.DescribeZoneChange(request.sessionID, info->chatID, request.ownershipEpoch, transition))
                SendZoneChange(request.sessionID, transition);
            return;
        }
        zone.EnqueueBootstrap(request.sessionID, request.ownershipEpoch, info->bootstrapID);
    }

    void ZoneEventHandler::SendZoneChangeFailed(uint64_t sessionID) {
        responses->Enqueue(sessionID, writer->WriteZoneChangeFailed());
    }

    void ZoneEventHandler::SendZoneChange(uint64_t sessionID, const ZoneChangeResult& transition) {
        responses->EnqueueZoneChange(sessionID, writer->WriteZoneChangeSucess(transition), transition.zoneID, transition.ownershipEpoch);
    }

    void ZoneEventHandler::SaveRemainingCharacters(ZoneState& zone) {
        CharacterState character;
        while (zone.TakeCharacter(character))
            cacheCommands->EnqueueCharacterStateUpdate(character.sessionID, character);
    }
}
