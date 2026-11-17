#include "pch.h"
#include "ZoneHandler.h"

#include "IPacketView.h"
#include "ISessionAuth.h"
#include "INetworkSender.h"
#include "IMessageQueue.h"
#include "PacketWriter.h"
#include "ZoneState.h"
#include "Message.h"
#include "MessagePool.h"
#include "LoggerGlobal.h"
#include <cmath>
#include <limits>

namespace Core {
    void ZoneHandler::ProcessAction(ZoneState& zone, const ActionRequestBody& action, uint64_t sessionID, uint16_t zoneID) {
        if (!std::isfinite(action.aimAngle) || action.aimAngle < 0 || action.aimAngle >= 360)
            return;
        auto& shard = stateManager->m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock sessionLock(shard.smutex);
        const auto session = shard.sessionMap.find(sessionID);
        if (session == shard.sessionMap.end() || !session->second.authenticated || !session->second.authCompleted
            || session->second.zoneID == 0 || session->second.ownershipEpoch == 0 || session->second.pendingFieldTransfer)
            return;
        auto& data = session->second;
        if (data.zoneID != zoneID || data.ownershipEpoch != action.ownershipEpoch) {
            ZoneChangeResult transition;
            const bool current = stateManager->DescribeZoneChangeLocked(sessionID, data, transition);
            sessionLock.unlock();
            if (current)
                SendZoneChange(sessionID, transition);
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

                std::shared_ptr<FieldZoneTransfer> transfer;
                try {
                    transfer = std::make_shared<FieldZoneTransfer>();
                    transfer->character = character;
                    zone.m_pendingFieldTransfers.push_back(transfer);
                }
                catch (const std::bad_alloc&) {
                    return;
                }
                transfer->sourceZoneID = zoneID;
                transfer->ownershipEpoch = action.ownershipEpoch;
                transfer->x = x;
                transfer->y = y;
                transfer->moveBudget = moveBudget;
                transfer->dir = dir;
                transfer->skillSlot = action.skillSlot;
                transfer->aimAngle = action.aimAngle;
                if (!zoneTransfer->TryBeginTransfer(destination, transfer)) {
                    zone.m_pendingFieldTransfers.pop_back();
                    return;
                }
                data.pendingFieldTransfer = std::move(transfer);
                zone.RemoveCharacterLocked(sessionID);
                zone.UpdateSessionSnapshot();
                return;
            }
        }

        zone.Skill(sessionID, action.skillSlot, action.aimAngle);
        zone.DirtyCheck(sessionID);
    }

    void ZoneHandler::ProcessFieldTransfer(ZoneState& zone, const std::shared_ptr<FieldZoneTransfer>& transfer) {
        ZoneChangeResult transition;
        if (transfer && stateManager->CompleteFieldTransfer(zone, transfer, transition))
            SendZoneChange(transfer->character.sessionID, transition);
    }

    void ZoneHandler::SendZoneChange(uint64_t sessionID, const ZoneChangeResult& transition) {
        auto response = writer->WriteZoneChangeSucess(transition);
        if (response)
            networkSender->SendDataUnique(sessionID, std::move(response));
        stateManager->SendBootstrap(sessionID, transition.ownershipEpoch);
    }

    void ZoneHandler::Process(ZoneState& zone, IPacketView* p, uint16_t zoneID) {
        PacketHeader* h = parseHeader(p->GetPtr());
        switch (p->GetOpcode())
        {
            case OP::PARRY: {
                auto response = writer->WriteParryResponse(stateManager->Parry(p->GetSessionID(), zoneID, *parseBody<ParryRequestBody>(p->GetPtr())));
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
