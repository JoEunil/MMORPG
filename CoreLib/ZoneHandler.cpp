#include "pch.h"
#include "ZoneHandler.h"

#include "IPacketView.h"
#include "ISessionAuth.h"
#include "INetworkSender.h"
#include "IMessageQueue.h"
#include "PacketWriter.h"
#include "ZoneState.h"
#include "ChatThreadPool.h"
#include "Message.h"
#include "MessagePool.h"
#include "LoggerGlobal.h"
#include <cmath>
#include <limits>

namespace Core {
    void ZoneHandler::ProcessAction(ZoneState& zone, const ActionRequestBody& action, uint64_t sessionID, uint16_t zoneID) {
        if (!std::isfinite(action.aimAngle) || action.aimAngle < 0 || action.aimAngle >= 360)
            return;
        auto& shard = sessionManager->m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock sessionLock(shard.smutex);
        const auto session = shard.sessionMap.find(sessionID);
        if (session == shard.sessionMap.end() || !session->second.authenticated || !session->second.authCompleted
            || session->second.zoneID == 0 || session->second.ownershipEpoch == 0 || session->second.pendingFieldTransfer)
            return;
        auto& data = session->second;
        if (data.zoneID != zoneID || data.ownershipEpoch != action.ownershipEpoch) {
            sessionManager->QueueBootstrapLocked(sessionID, data, true);
            return;
        }
        data.lastRespawnRequestEpoch = 0;

        float previousX, previousY, previousBudget;
        uint8_t previousDir;
        uint32_t previousDirty;
        {
            std::lock_guard zoneLock(zone.m_mutex);
            const auto member = zone.m_sessionToIndex.find(sessionID);
            if (member == zone.m_sessionToIndex.end())
                return;
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
                return;
            const auto& character = zone.m_chars[member->second];
            const auto destination = action.ownershipEpoch == (std::numeric_limits<uint64_t>::max)()
                ? 0 : zoneTransfer->FindDestination(zoneID, character.x, character.y);
            if (destination != 0) {
                const float x = character.x, y = character.y, moveBudget = character.moveBudget;
                const uint8_t dir = character.dir;
                zone.RestoreMovementLocked(sessionID, previousX, previousY, previousDir, previousBudget, previousDirty);

                auto transfer = std::make_shared<FieldZoneTransfer>();
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
                    return;
                data.pendingFieldTransfer = std::move(transfer);
                zone.RemoveCharacterLocked(sessionID);
                return;
            }
        }

        zone.Skill(sessionID, action.skillSlot, action.aimAngle);
        zone.DirtyCheck(sessionID);
    }

    void ZoneHandler::ProcessFieldTransfer(ZoneState& zone, const std::shared_ptr<FieldZoneTransfer>& transfer) {
        if (!transfer)
            return;

        const auto sessionID = transfer->character.sessionID;
        auto& shard = sessionManager->m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock sessionLock(shard.smutex);
        const auto session = shard.sessionMap.find(sessionID);
        if (session == shard.sessionMap.end() || session->second.pendingFieldTransfer != transfer
            || session->second.zoneID != transfer->sourceZoneID || session->second.ownershipEpoch != transfer->ownershipEpoch)
            return;

        const bool returning = transfer->status == FieldZoneTransfer::Status::Rejected;
        if (returning != (zone.m_zoneID == transfer->sourceZoneID))
            return;

        std::unique_lock zoneLock(zone.m_mutex);
        const bool admitted = returning ? zone.RestoreFieldTransferLocked(transfer->character)
                                        : zone.AdmitFieldTransferLocked(*transfer);
        if (!admitted) {
            transfer->status = FieldZoneTransfer::Status::Rejected;
            zoneLock.unlock();
            sessionLock.unlock();
            if (returning || !zoneTransfer->TryEnqueueTransfer(transfer->sourceZoneID, transfer))
                zoneTransfer->EnqueueLobbyReturn(transfer);
            return;
        }

        auto& data = session->second;
        data.pendingFieldTransfer.reset();
        if (returning)
            return;

        SessionManager::UpdateZoneOwnershipLocked(data, zone.m_zoneID);
        const auto& entered = zone.m_chars.back();
        const ZoneChangeResult result{zone.m_zoneID, data.chatID, entered.zoneInternalID, entered.x, entered.y,
            zone.m_generation, data.ownershipEpoch};
        chat->EnqueueZoneJoin(sessionID, zone.m_zoneID);
        zoneLock.unlock();
        zone.Skill(sessionID, transfer->skillSlot, transfer->aimAngle);
        zone.DirtyCheck(sessionID);
        sessionLock.unlock();
        SendZoneChange(zone, sessionID, result);
    }

    void ZoneHandler::ApplyProfileVersion(ZoneState& zone, const ProfileVersionUpdate& update) {
        auto& shard = sessionManager->m_shards[update.sessionID & SHARD_SIZE_MASK];
        std::unique_lock sessionLock(shard.smutex);
        const auto session = shard.sessionMap.find(update.sessionID);
        if (session == shard.sessionMap.end())
            return;

        const auto& data = session->second;
        if (data.zoneID != zone.m_zoneID || data.pendingFieldTransfer) {
            sessionManager->RouteProfileVersionLocked(data, update);
            return;
        }

        zone.UpdateProfileVersion(update.sessionID, update.profileID, update.version);
    }

    void ZoneHandler::ProcessBootstrap(ZoneState& zone, const BootstrapRequest& request) {
        auto& shard = sessionManager->m_shards[request.sessionID & SHARD_SIZE_MASK];
        std::unique_lock sessionLock(shard.smutex);
        const auto found = shard.sessionMap.find(request.sessionID);
        if (found == shard.sessionMap.end() || found->second.zoneID != zone.m_zoneID
            || found->second.ownershipEpoch != request.ownershipEpoch || found->second.pendingFieldTransfer)
            return;

        auto& session = found->second;
        if (request.resendZoneChange) {
            ZoneChangeResult transition;
            if (!zone.DescribeZoneChange(request.sessionID, session.chatID, request.ownershipEpoch, transition))
                return;
            sessionLock.unlock();
            SendZoneChange(zone, request.sessionID, transition);
            return;
        }

        const auto bootstrapID = SessionManager::NextBootstrapIDLocked(session);
        if (bootstrapID != 0)
            zone.EnqueueBootstrap(request.sessionID, request.ownershipEpoch, bootstrapID);
    }

    void ZoneHandler::SendZoneChange(ZoneState& zone, uint64_t sessionID, const ZoneChangeResult& transition) {
        auto response = writer->WriteZoneChangeSucess(transition);
        if (response)
            networkSender->SendDataUnique(sessionID, std::move(response));
        ProcessBootstrap(zone, {sessionID, transition.ownershipEpoch});
    }

    ParryResponseBody ZoneHandler::ProcessParry(ZoneState& zone, uint64_t sessionID, uint16_t zoneID,
        const ParryRequestBody& request) {
        ParryResponseBody result{request.requestID, request.ownershipEpoch, static_cast<uint8_t>(ParryStatus::Invalid), 0, 0};
        if (request.requestID == 0 || request.ownershipEpoch == 0)
            return result;

        auto& shard = sessionManager->m_shards[sessionID & SHARD_SIZE_MASK];
        std::shared_lock sessionLock(shard.smutex);
        const auto session = shard.sessionMap.find(sessionID);
        if (session == shard.sessionMap.end() || !session->second.authenticated || !session->second.authCompleted) {
            result.status = static_cast<uint8_t>(ParryStatus::Unavailable);
            return result;
        }
        const auto& data = session->second;
        if (data.zoneID == 0 || data.zoneID != zoneID || data.ownershipEpoch != request.ownershipEpoch) {
            result.status = static_cast<uint8_t>(ParryStatus::StaleOwnership);
            return result;
        }
        return zone.Parry(sessionID, request);
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
