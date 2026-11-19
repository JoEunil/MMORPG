#include "pch.h"
#include "ZoneHandler.h"

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
#include <limits>

namespace Core {
    void ZoneHandler::ProcessAction(ZoneState& zone, const ActionRequestBody& action, uint64_t sessionID, uint16_t zoneID) {
        if (!std::isfinite(action.aimAngle) || action.aimAngle < 0 || action.aimAngle >= 360)
            return;
        sessionManager->ApplyZoneAction(sessionID, zoneID, action.ownershipEpoch, [&]() -> std::shared_ptr<ZoneTransfer> {
            float previousX, previousY, previousBudget;
            uint8_t previousDir;
            uint32_t previousDirty;
            {
                std::lock_guard zoneLock(zone.m_mutex);
                const auto member = zone.m_sessionToIndex.find(sessionID);
                if (member == zone.m_sessionToIndex.end())
                    return {};
                const auto& character = zone.m_chars[member->second];
                previousX = character.x;
                previousY = character.y;
                previousBudget = character.moveBudget;
                previousDir = character.dir;
                previousDirty = character.dirtyBit;
            }

            zone.Move(sessionID, action.dir, action.x, action.y);
            {
                std::lock_guard zoneLock(zone.m_mutex);
                const auto member = zone.m_sessionToIndex.find(sessionID);
                if (member == zone.m_sessionToIndex.end())
                    return {};
                const auto& character = zone.m_chars[member->second];
                const auto destination = action.ownershipEpoch == (std::numeric_limits<uint64_t>::max)()
                    ? 0 : zoneTransfer->FindDestination(zoneID, character.x, character.y);
                if (destination != 0) {
                    const float x = character.x, y = character.y, moveBudget = character.moveBudget;
                    const uint8_t dir = character.dir;
                    zone.RestoreMovementLocked(sessionID, previousX, previousY, previousDir, previousBudget, previousDirty);

                    auto transfer = std::make_shared<ZoneTransfer>();
                    transfer->character = character;
                    transfer->sourceZoneID = zoneID;
                    transfer->ownershipEpoch = action.ownershipEpoch;
                    transfer->x = x;
                    transfer->y = y;
                    transfer->moveBudget = moveBudget;
                    transfer->dir = dir;
                    transfer->skillSlot = action.skillSlot;
                    transfer->aimAngle = action.aimAngle;
                    if (!zoneTransfer->TryEnqueueTransfer(destination, transfer))
                        return {};
                    zone.RemoveCharacterLocked(sessionID);
                    return transfer;
                }
            }

            zone.Skill(sessionID, action.skillSlot, action.aimAngle);
            zone.DirtyCheck(sessionID);
            return {};
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
                return StartRespawnTransfer(zone, sessionID, characterID, ownershipEpoch);
            },
            [&](uint64_t characterID, bool replay) {
                return zone.DescribeOrRespawnInTown(sessionID, characterID, replay, result);
            });
        if (outcome == ZoneOperationResult::Completed)
            SendZoneChange(zone, sessionID, result);
        else if (outcome == ZoneOperationResult::Rejected)
            SendZoneChangeFailed(sessionID);
    }

    std::shared_ptr<ZoneTransfer> ZoneHandler::StartRespawnTransfer(ZoneState& source, uint64_t sessionID,
        uint64_t characterID, uint64_t ownershipEpoch) {
        std::lock_guard zoneLock(source.m_mutex);
        const auto member = source.m_sessionToIndex.find(sessionID);
        if (member == source.m_sessionToIndex.end())
            return {};
        const auto& character = source.m_chars[member->second];
        if (character.characterID != characterID || character.hp != 0
            || character.maxHp <= 0 || character.maxMp < 0)
            return {};

        auto transfer = std::make_shared<ZoneTransfer>();
        transfer->character = character;
        transfer->ownershipEpoch = ownershipEpoch;
        transfer->sourceZoneID = source.m_zoneID;
        transfer->kind = ZoneTransfer::Kind::Respawn;
        transfer->x = TOWN_SPAWN_X;
        transfer->y = TOWN_SPAWN_Y;
        if (!zoneTransfer->TryEnqueueTransfer(TOWN_ZONE_ID, transfer))
            return {};

        source.RemoveCharacterLocked(sessionID);
        return transfer;
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
            [&] {
                std::lock_guard zoneLock(zone.m_mutex);
                try {
                    if (returning)
                        return zone.RestoreZoneTransferLocked(transfer->character);
                    if (!zone.AdmitZoneTransferLocked(*transfer))
                        return false;
                }
                catch (const std::bad_alloc&) {
                    return false;
                }

                const auto& entered = zone.m_chars.back();
                result = {zone.m_zoneID, 0, entered.zoneInternalID, entered.x, entered.y, zone.m_generation};
                return true;
            },
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
