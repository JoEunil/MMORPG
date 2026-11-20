#include "pch.h"
#include "SessionManager.h"
#include "../Cache/CacheCommandSender.h"
#include "../NonSimulation/ChatThreadPool.h"
#include <algorithm>
#include <limits>

namespace Core {
    bool SessionManager::CheckZoneAction(uint64_t sessionID, uint16_t zoneID, uint64_t ownershipEpoch,
        std::optional<ZoneRequestRoute>& resync) {
        resync.reset();
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (!session || session->disconnectQueued || !session->authenticated || !session->authCompleted
            || session->zoneID == 0 || session->ownershipEpoch == 0 || session->pendingZoneTransfer)
            return false;
        if (session->zoneID != zoneID || session->ownershipEpoch != ownershipEpoch) {
            resync = ZoneRequestRoute{session->zoneID, session->ownershipEpoch};
            return false;
        }
        session->lastRespawnRequestEpoch = 0;
        return true;
    }

    bool SessionManager::BeginZoneTransfer(const std::shared_ptr<ZoneTransfer>& transfer) {
        if (!transfer) return false;
        const auto sessionID = transfer->character.sessionID;
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (!session || session->disconnectQueued || !session->authenticated || !session->authCompleted
            || session->zoneID == 0 || session->zoneID != transfer->sourceZoneID
            || session->ownershipEpoch != transfer->ownershipEpoch || session->pendingZoneTransfer)
            return false;
        session->pendingZoneTransfer = transfer;
        return true;
    }

    void SessionManager::CancelZoneTransfer(const std::shared_ptr<ZoneTransfer>& transfer) {
        if (!transfer) return;
        const auto sessionID = transfer->character.sessionID;
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (session && session->pendingZoneTransfer == transfer)
            session->pendingZoneTransfer.reset();
    }

    bool SessionManager::CheckItemUse(uint64_t sessionID, uint16_t zoneID, const ItemUseRequestBody& request, ItemUseResponseBody& result) const {
        result = ItemUseResponseBody{request.requestID, request.ownershipEpoch, request.itemID, 0, 0, 0};
        if (request.requestID == 0 || request.itemID == 0) return false;
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::shared_lock sessionLock(shard.smutex);
        const auto session = shard.sessionMap.find(sessionID);
        if (session == shard.sessionMap.end() || session->second.disconnectQueued || !session->second.authenticated
            || !session->second.authCompleted || session->second.characterID == 0) return false;
        auto& saved = session->second.lastItemUse;
        if (request.requestID == saved.requestID) {
            if (request.ownershipEpoch == saved.ownershipEpoch && request.itemID == saved.itemID) { result = saved; return false; }
            return false;
        }
        if (request.requestID < saved.requestID || request.ownershipEpoch != session->second.ownershipEpoch) {
            result.status = 7;
            return false;
        }
        if (zoneID == 0 || session->second.zoneID != zoneID) return false;
        return true;
    }

    bool SessionManager::CheckEquipment(uint64_t sessionID, uint16_t zoneID, uint64_t requestID, uint64_t ownershipEpoch, bool write, uint8_t slot, uint32_t itemID, EquipmentResponseBody& result) const {
        result = EquipmentResponseBody{ requestID, ownershipEpoch, static_cast<uint8_t>(write ? 1 : 0), static_cast<uint8_t>(EquipmentStatus::Invalid), slot, 0, 0, 0, 0, 0, 0, 0 };
        if (requestID == 0 || ownershipEpoch == 0) return false;
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::shared_lock sessionLock(shard.smutex);
        auto session = shard.sessionMap.find(sessionID);
        if (session == shard.sessionMap.end() || session->second.disconnectQueued || !session->second.authenticated || !session->second.authCompleted
            || session->second.characterID == 0) return false;
        if (zoneID == 0 || session->second.zoneID != zoneID || ownershipEpoch != session->second.ownershipEpoch) {
            result.status = static_cast<uint8_t>(EquipmentStatus::StaleOwnership);
            return false;
        }
        auto& previous = session->second.lastEquipmentSet;
        if (write && requestID <= previous.requestID) {
            if (requestID == previous.requestID && ownershipEpoch == previous.ownershipEpoch
                && slot == previous.slot && itemID == session->second.lastEquipmentItemID) { result = previous; return false; }
            result.status = static_cast<uint8_t>(EquipmentStatus::StaleRequest);
            return false;
        }
        return true;
    }

    void SessionManager::UpdateZoneOwnershipLocked(SessionData& session, uint16_t destinationID) {
        session.zoneID = destinationID;
        ++session.ownershipEpoch;
    }

    bool SessionManager::MatchesZoneTransfer(const SessionData* session, const std::shared_ptr<ZoneTransfer>& transfer) {
        return session && !session->disconnectQueued && session->pendingZoneTransfer == transfer
            && session->zoneID == transfer->sourceZoneID && session->ownershipEpoch == transfer->ownershipEpoch;
    }

    void SessionManager::JoinZoneChatLocked(uint64_t sessionID, uint16_t zoneID, SessionData& session,
        std::optional<uint32_t> lobbyProfile) {
        if (lobbyProfile && session.chatID == 0)
            session.chatID = chat->AddChatSession(sessionID, zoneID, *lobbyProfile);
        chat->EnqueueZoneJoin(sessionID, zoneID);
    }

    void SessionManager::LeaveZoneChatLocked(uint64_t sessionID, uint16_t zoneID) {
        chat->EnqueueZoneLeave(sessionID, zoneID);
    }

    uint64_t SessionManager::NextBootstrapIDLocked(SessionData& session) {
        if (session.bootstrapSequence == (std::numeric_limits<uint64_t>::max)())
            return 0;
        return ++session.bootstrapSequence;
    }

    void SessionManager::CleanUp() {
        if (!m_running.exchange(false, std::memory_order_relaxed)) return;
        for (auto& shard : m_shards) {
            std::unique_lock lock(shard.smutex);
            for (const auto& [sessionID, data] : shard.sessionMap)
                if (data.pendingZoneTransfer)
                    cacheCommands->EnqueueCharacterStateUpdate(sessionID, data.pendingZoneTransfer->character);
            shard.sessionMap.clear();
        }
    }

    bool SessionManager::CompleteZoneDisconnect(uint64_t sessionID, uint16_t zoneID, uint64_t ownershipEpoch,
        std::shared_ptr<ZoneTransfer>& pending) {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        const auto it = shard.sessionMap.find(sessionID);
        if (it == shard.sessionMap.end() || !it->second.disconnectQueued
            || it->second.zoneID != zoneID || it->second.ownershipEpoch != ownershipEpoch)
            return false;

        pending = std::move(it->second.pendingZoneTransfer);
        shard.sessionMap.erase(it);
        chat->EnqueueZoneLeave(sessionID, zoneID);
        chat->DeleteChatSession(sessionID, zoneID);
        return true;
    }

    void SessionManager::RememberItemUse(uint64_t sessionID, uint16_t zoneID, const ItemUseResponseBody& result) {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (session && session->zoneID == zoneID && session->ownershipEpoch == result.ownershipEpoch
            && result.requestID > session->lastItemUse.requestID) session->lastItemUse = result;
    }

    void SessionManager::RememberEquipment(uint64_t sessionID, uint16_t zoneID, uint32_t itemID, const EquipmentResponseBody& result) {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (session && session->zoneID == zoneID && session->ownershipEpoch == result.ownershipEpoch
            && result.requestID > session->lastEquipmentSet.requestID) {
            session->lastEquipmentSet = result;
            session->lastEquipmentItemID = itemID;
        }
    }

    std::optional<LobbyEntryRequest> SessionManager::GetLobbyEntry(uint64_t sessionID, uint8_t op) const {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::shared_lock lock(shard.smutex);
        const auto found = shard.sessionMap.find(sessionID);
        if (op != ZONE_CHANGE::ENTER || found == shard.sessionMap.end()) return std::nullopt;
        const auto& data = found->second;
        if (data.disconnectQueued || !data.authenticated || !data.authCompleted || data.characterID == INVALID_CHARACTER_ID
            || data.zoneID != 0 || data.ownershipEpoch == (std::numeric_limits<uint64_t>::max)()) return std::nullopt;
        return LobbyEntryRequest{data.characterID, data.ownershipEpoch};
    }

    std::optional<ZoneRequestRoute> SessionManager::GetRespawnRoute(uint64_t sessionID, uint64_t ownershipEpoch) const {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::shared_lock lock(shard.smutex);
        const auto found = shard.sessionMap.find(sessionID);
        if (found == shard.sessionMap.end()) return std::nullopt;
        const auto& data = found->second;
        if (data.disconnectQueued || !data.authenticated || !data.authCompleted || data.userID == 0
            || data.characterID == INVALID_CHARACTER_ID || data.zoneID == 0 || data.zoneID > ZONE_COUNT
            || ownershipEpoch == (std::numeric_limits<uint64_t>::max)()) return std::nullopt;
        if (data.pendingZoneTransfer) {
            if (!data.pendingZoneTransfer->IsRespawn() || ownershipEpoch != data.ownershipEpoch) return std::nullopt;
            return ZoneRequestRoute{data.zoneID, ownershipEpoch, true};
        }
        const bool replay = data.zoneID == TOWN_ZONE_ID && data.lastRespawnRequestEpoch == ownershipEpoch
            && data.ownershipEpoch == ownershipEpoch + 1;
        if (data.ownershipEpoch != ownershipEpoch && !replay) return std::nullopt;
        return ZoneRequestRoute{data.zoneID, ownershipEpoch};
    }

    std::optional<ZoneRequestRoute> SessionManager::GetBootstrapRoute(uint64_t sessionID, uint64_t ownershipEpoch) const {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::shared_lock lock(shard.smutex);
        const auto found = shard.sessionMap.find(sessionID);
        if (found == shard.sessionMap.end()) return std::nullopt;
        const auto& data = found->second;
        if (data.disconnectQueued || data.zoneID == 0 || data.ownershipEpoch == 0
            || data.pendingZoneTransfer || data.ownershipEpoch != ownershipEpoch) return std::nullopt;
        return ZoneRequestRoute{data.zoneID, ownershipEpoch};
    }

    std::optional<DisconnectRequest> SessionManager::BeginDisconnect(uint64_t sessionID) {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (!session || session->disconnectQueued) return std::nullopt;
        session->disconnectQueued = true;
        return DisconnectRequest{session->zoneID, session->ownershipEpoch, session->chatID};
    }

    bool SessionManager::CompleteLobbyDisconnect(uint64_t sessionID, uint64_t ownershipEpoch) {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (!session || !session->disconnectQueued || session->zoneID != 0 || session->ownershipEpoch != ownershipEpoch) return false;
        shard.sessionMap.erase(sessionID);
        return true;
    }
}
