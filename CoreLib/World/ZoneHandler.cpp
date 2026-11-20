#include "pch.h"
#include "ZoneHandler.h"
#include "../Cache/CacheCommandSender.h"

#include "../Interface/IPacketView.h"
#include "../Interface/ISessionAuth.h"
#include "../Interface/INetworkSender.h"
#include "../Interface/IMessageQueue.h"
#include "../Packet/PacketWriter.h"
#include "ZoneState.h"
#include "LobbyZone.h"
#include "../Cache/Message.h"
#include "../Cache/MessagePool.h"
#include "../LoggerGlobal.h"
#include <cmath>

namespace Core {
    void ZoneHandler::ProcessAction(ZoneState& zone, const ActionRequestBody& action, uint64_t sessionID, uint16_t zoneID) {
        if (!std::isfinite(action.aimAngle) || action.aimAngle < 0 || action.aimAngle >= 360)
            return;
        std::optional<ZoneRequestRoute> resync;
        if (!sessionManager->CheckZoneAction(sessionID, zoneID, action.ownershipEpoch, resync)) {
            if (resync)
                zoneRequests->TryEnqueueBootstrap(resync->zoneID, sessionID, resync->ownershipEpoch, true);
            return;
        }
        zone.ApplyAction(sessionID, action, *zoneTransfer);
    }

    void ZoneHandler::ProcessLobbyEntry(ZoneState& zone, const ZoneRequest& request) {
        const auto sessionID = request.sessionID;
        ZoneChangeResult result;
        const auto outcome = lobbyZone->EnterField(sessionID, request.ownershipEpoch, zone, result);
        if (outcome == ZoneOperationResult::Completed)
            SendZoneChange(zone, sessionID, result);
        else if (outcome == ZoneOperationResult::Rejected)
            SendZoneChangeFailed(sessionID);
    }

    void ZoneHandler::ProcessRespawn(ZoneState& zone, const ZoneRequest& request) {
        const auto sessionID = request.sessionID;
        const auto respawn = sessionManager->CheckRespawnRequest(sessionID, zone.GetZoneID(), request.ownershipEpoch);
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
            SendZoneChange(zone, sessionID, result);
    }

    void ZoneHandler::ProcessZoneTransfer(ZoneState& zone, const ZoneEvent& event) {
        const auto& transfer = event.transfer;
        if (!transfer)
            return;

        const auto sessionID = transfer->character.sessionID;
        const bool returning = event.type == ZoneEventType::TransferRejected;
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
                errorLogger->LogError("zone handler", "could not roll back transfer admission", "sessionID", sessionID);
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
        SendZoneChange(zone, sessionID, result);
    }

    void ZoneHandler::ApplyProfileVersion(ZoneState& zone, const ProfileVersionUpdate& update) {
        auto route = sessionManager->RouteProfileVersion(update);
        if (route && *route == zone.GetZoneID()) {
            zone.UpdateProfileVersion(update.sessionID, update.profileID, update.version);
            return;
        }
        if (route && *route == 0)
            route = lobbyZone->ApplyProfileVersion(update);
        if (route && !zoneRequests->TryEnqueueProfileVersion(*route, update))
            errorLogger->LogWarn("zone handler", "could not route profile version", "sessionID", update.sessionID, "zoneID", *route);
    }

    void ZoneHandler::ProcessDisconnect(ZoneState& zone, uint16_t zoneID, const ZoneRequest& request) {
        std::shared_ptr<ZoneTransfer> pending;
        if (!sessionManager->CompleteZoneDisconnect(request.sessionID, zoneID, request.ownershipEpoch, pending))
            return;

        if (pending) {
            cacheCommands->EnqueueCharacterStateUpdate(request.sessionID, pending->character);
            return;
        }
        CharacterState character;
        if (zone.EmigrateChar(request.sessionID, character))
            cacheCommands->EnqueueCharacterStateUpdate(request.sessionID, character);
        else
            gameLogger->LogWarn("zone handler", "disconnect character not found", "sessionID", request.sessionID, "zoneID", zoneID);
    }

    void ZoneHandler::ProcessBootstrap(ZoneState& zone, const ZoneRequest& request) {
        const auto info = sessionManager->PrepareZoneBootstrap(request.sessionID, zone.GetZoneID(), request.ownershipEpoch, request.resendZoneChange);
        if (!info)
            return;
        if (request.resendZoneChange) {
            ZoneChangeResult transition;
            if (zone.DescribeZoneChange(request.sessionID, info->chatID, request.ownershipEpoch, transition))
                SendZoneChange(zone, request.sessionID, transition);
            return;
        }
        zone.EnqueueBootstrap(request.sessionID, request.ownershipEpoch, info->bootstrapID);
    }

    void ZoneHandler::SendZoneChangeFailed(uint64_t sessionID) {
        auto response = writer->WriteZoneChangeFailed();
        if (response)
            networkSender->SendDataUnique(sessionID, std::move(response));
    }

    void ZoneHandler::SendZoneChange(ZoneState& zone, uint64_t sessionID, const ZoneChangeResult& transition) {
        auto response = writer->WriteZoneChangeSucess(transition);
        if (response)
            networkSender->SendDataUnique(sessionID, std::move(response));
        ProcessBootstrap(zone, {sessionID, transition.ownershipEpoch});
    }

    ParryResponseBody ZoneHandler::ProcessParry(ZoneState& zone, uint64_t sessionID, uint16_t zoneID, const ParryRequestBody& request) {
        if (const auto rejection = sessionManager->GetParryRejection(sessionID, zoneID, request))
            return {request.requestID, request.ownershipEpoch, static_cast<uint8_t>(*rejection), 0, 0};
        return zone.Parry(sessionID, request);
    }

    void ZoneHandler::Process(ZoneState& zone, IPacketView* p, uint16_t zoneID) {
        PacketHeader* h = parseHeader(p->GetPtr());
        switch (p->GetOpcode()) {
        case OP::ITEM_USE: {
            const auto request = *parseBody<ItemUseRequestBody>(p->GetPtr());
            ItemUseResponseBody result{request.requestID, request.ownershipEpoch, request.itemID, 0, 0, 0};
            const auto check = sessionManager->CheckItemUse(p->GetSessionID(), zoneID, request, result);
            if (check == SessionRequestStatus::Ready) {
                if (const auto applied = zone.UseItem(p->GetSessionID(), request, inventory)) {
                    result = *applied;
                    sessionManager->RememberItemUse(p->GetSessionID(), zoneID, result);
                }
            } else if (check == SessionRequestStatus::StaleOwnership || check == SessionRequestStatus::StaleRequest)
                result.status = 7;
            auto response = writer->WriteItemUseResponse(result);
            if (response)
                networkSender->SendDataUnique(p->GetSessionID(), std::move(response));
            break;
        }
        case OP::EQUIPMENT_GET:
        case OP::EQUIPMENT_SET: {
            const bool write = p->GetOpcode() == OP::EQUIPMENT_SET;
            uint64_t requestID, epoch;
            uint8_t slot = 0;
            uint32_t itemID = 0;
            if (write) {
                const auto request = *parseBody<EquipmentSetRequestBody>(p->GetPtr());
                requestID = request.requestID;
                epoch = request.ownershipEpoch;
                slot = request.slot;
                itemID = request.itemID;
            } else {
                const auto request = *parseBody<EquipmentGetRequestBody>(p->GetPtr());
                requestID = request.requestID;
                epoch = request.ownershipEpoch;
            }
            EquipmentResponseBody result{requestID, epoch, static_cast<uint8_t>(write ? 1 : 0), static_cast<uint8_t>(EquipmentStatus::Invalid), slot};
            const auto check = sessionManager->CheckEquipment(p->GetSessionID(), zoneID, requestID, epoch, write, slot, itemID, result);
            if (check == SessionRequestStatus::Ready) {
                if (const auto applied = zone.Equipment(p->GetSessionID(), requestID, epoch, write, slot, itemID, inventory)) {
                    result = *applied;
                    if (write)
                        sessionManager->RememberEquipment(p->GetSessionID(), zoneID, itemID, result);
                }
            } else if (check == SessionRequestStatus::StaleOwnership)
                result.status = static_cast<uint8_t>(EquipmentStatus::StaleOwnership);
            else if (check == SessionRequestStatus::StaleRequest)
                result.status = static_cast<uint8_t>(EquipmentStatus::StaleRequest);
            auto response = writer->WriteEquipmentResponse(result);
            if (response)
                networkSender->SendDataUnique(p->GetSessionID(), std::move(response));
            break;
        }
        case OP::PARRY: {
            const auto result = ProcessParry(zone, p->GetSessionID(), zoneID, *parseBody<ParryRequestBody>(p->GetPtr()));
            auto response = writer->WriteParryResponse(result);
            if (response)
                networkSender->SendDataUnique(p->GetSessionID(), std::move(response));
            break;
        }
        case OP::ACTION:
            ProcessAction(zone, *parseBody<ActionRequestBody>(p->GetPtr()), p->GetSessionID(), zoneID);
            break;
        default:
            errorLogger->LogInfo("zone Handler", "UnDefined OPCODE", "zone", zoneID, "opcode", p->GetOpcode(), "session", p->GetSessionID());
            break;
        }
    }

    void ZoneHandler::SaveRemainingCharacters(ZoneState& zone) {
        CharacterState character;
        while (zone.TakeCharacter(character))
            cacheCommands->EnqueueCharacterStateUpdate(character.sessionID, character);
    }
}
