#include "pch.h"
#include "ZoneHandler.h"
#include "CacheCommandSender.h"

#include "IPacketView.h"
#include "ISessionAuth.h"
#include "INetworkSender.h"
#include "IMessageQueue.h"
#include "PacketWriter.h"
#include "ZoneState.h"
#include "LobbyZone.h"
#include "WorldMap.h"
#include "Message.h"
#include "MessagePool.h"
#include "LoggerGlobal.h"
#include <cmath>

namespace Core {
    void ZoneHandler::ProcessAction(ZoneState& zone, const ActionRequestBody& action, uint64_t sessionID, uint16_t zoneID) {
        if (!std::isfinite(action.aimAngle) || action.aimAngle < 0 || action.aimAngle >= 360)
            return;
        sessionManager->ApplyZoneAction(sessionID, zoneID, action.ownershipEpoch, [&] {
            return zone.ApplyAction(sessionID, action, *zoneTransfer);
        });
    }

    void ZoneHandler::ProcessLobbyEntry(ZoneState& zone, const ZoneRequest& request) {
        const auto sessionID = request.sessionID;
        ZoneChangeResult result;
        const auto outcome = sessionManager->EnterZoneFromLobby(sessionID, zone.m_zoneID, request.ownershipEpoch, result,
            [&](uint64_t characterID) -> std::optional<uint32_t> {
                uint32_t profileID = 0;
                if (!lobbyZone->TryEnterField(sessionID, characterID, zone.m_zoneID, [&](const CharacterState& loaded) {
                    profileID = loaded.profileId;
                    return zone.AdmitLobbyCharacter(sessionID, loaded, result);
                }))
                    return std::nullopt;
                return profileID;
            });
        if (outcome == ZoneOperationResult::Completed)
            SendZoneChange(zone, sessionID, result);
        else if (outcome == ZoneOperationResult::Rejected)
            SendZoneChangeFailed(sessionID);
    }

    void ZoneHandler::ProcessRespawn(ZoneState& zone, const ZoneRequest& request) {
        const auto sessionID = request.sessionID;
        const bool townSpawnWalkable = WorldMap::CanTraverse(TOWN_SPAWN_X, TOWN_SPAWN_Y, TOWN_SPAWN_X, TOWN_SPAWN_Y);
        ZoneChangeResult result;
        const auto outcome = sessionManager->RespawnInZone(sessionID, zone.m_zoneID, request.ownershipEpoch,
            townSpawnWalkable, result,
            [&](uint64_t characterID, uint64_t ownershipEpoch) {
                return zone.StartRespawnTransfer(sessionID, characterID, ownershipEpoch, *zoneTransfer);
            },
            [&](uint64_t characterID, bool replay) {
                return zone.DescribeOrRespawnInTown(sessionID, characterID, replay, result);
            });
        if (outcome == ZoneOperationResult::Completed)
            SendZoneChange(zone, sessionID, result);
        else if (outcome == ZoneOperationResult::Rejected)
            SendZoneChangeFailed(sessionID);
    }

    void ZoneHandler::ProcessZoneTransfer(ZoneState& zone, const ZoneEvent& event) {
        const auto& transfer = event.transfer;
        if (!transfer)
            return;

        const auto sessionID = transfer->character.sessionID;
        const bool returning = event.type == ZoneEventType::TransferRejected;
        const bool respawn = transfer->IsRespawn();
        ZoneChangeResult result;
        const auto outcome = sessionManager->ResolveZoneTransfer(transfer, zone.m_zoneID, returning, result,
            [&] { return zone.TryResolveZoneTransfer(*transfer, returning, result); },
            [&] {
                if (!respawn) {
                    zone.Skill(sessionID, transfer->skillSlot, transfer->aimAngle);
                    zone.DirtyCheck(sessionID);
                }
            });

        if (outcome == ZoneOperationResult::Rejected) {
            if (returning || !zoneTransfer->TryEnqueueTransferRejected(transfer))
                zoneTransfer->EnqueueLobbyReturn(transfer);
        } else if (outcome == ZoneOperationResult::Completed) {
            if (!returning)
                SendZoneChange(zone, sessionID, result);
            else if (respawn)
                SendZoneChangeFailed(sessionID);
        }
    }

    void ZoneHandler::ApplyProfileVersion(ZoneState& zone, const ProfileVersionUpdate& update) {
        sessionManager->ApplyZoneProfileVersion(zone.m_zoneID, update, [&] {
            zone.UpdateProfileVersion(update.sessionID, update.profileID, update.version);
        });
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
            gameLogger->LogWarn("zone handler", "disconnect character not found",
                "sessionID", request.sessionID, "zoneID", zoneID);
    }

    void ZoneHandler::ProcessBootstrap(ZoneState& zone, const ZoneRequest& request) {
        ZoneChangeResult transition;
        bool described = false;
        sessionManager->ProcessZoneBootstrap(request.sessionID, zone.m_zoneID, request.ownershipEpoch, request.resendZoneChange,
            [&](uint64_t chatID) {
                described = zone.DescribeZoneChange(request.sessionID, chatID, request.ownershipEpoch, transition);
            },
            [&](uint64_t bootstrapID) {
                zone.EnqueueBootstrap(request.sessionID, request.ownershipEpoch, bootstrapID);
            });
        if (described)
            SendZoneChange(zone, request.sessionID, transition);
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

    ParryResponseBody ZoneHandler::ProcessParry(ZoneState& zone, uint64_t sessionID, uint16_t zoneID,
        const ParryRequestBody& request) {
        return sessionManager->ApplyZoneParry(sessionID, zoneID, request, [&] { return zone.Parry(sessionID, request); });
    }

    void ZoneHandler::Process(ZoneState& zone, IPacketView* p, uint16_t zoneID) {
        PacketHeader* h = parseHeader(p->GetPtr());
        switch (p->GetOpcode())
        {
            case OP::PARRY: {
                const auto result = ProcessParry(zone, p->GetSessionID(), zoneID, *parseBody<ParryRequestBody>(p->GetPtr()));
                auto response = writer->WriteParryResponse(result);
                if (response) networkSender->SendDataUnique(p->GetSessionID(), std::move(response));
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
}
