#include "pch.h"
#include "SessionManager.h"
#include "Interface/ISessionLifecycleListener.h"

namespace Core {
    std::optional<uint16_t> SessionManager::RouteProfileVersion(const ProfileVersionUpdate& update) {
        auto& shard = m_shards[update.sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, update.sessionID);
        if (!session)
            return std::nullopt;
        return session->zoneTransition.RouteProfileVersion(update.profileID, update.version);
    }

    std::optional<ZoneBootstrapInfo> SessionManager::PrepareZoneBootstrap(const ZoneSessionContext& context, bool resendZoneChange) {
        const auto& [sessionID, zoneID, ownershipEpoch] = context;
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (!session || session->disconnectQueued)
            return std::nullopt;
        const auto bootstrapID = session->zoneTransition.PrepareBootstrap(zoneID, ownershipEpoch, resendZoneChange);
        if (!bootstrapID)
            return std::nullopt;
        return ZoneBootstrapInfo{session->chatID, *bootstrapID};
    }

    std::optional<ParryStatus> SessionManager::GetParryRejection(uint64_t sessionID, uint16_t zoneID, const ParryRequestBody& request) const {
        if (request.requestID == 0 || request.ownershipEpoch == 0)
            return ParryStatus::Invalid;
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::shared_lock lock(shard.smutex);
        const auto found = shard.sessionMap.find(sessionID);
        if (found == shard.sessionMap.end() || found->second.disconnectQueued || !found->second.authenticated || !found->second.authCompleted)
            return ParryStatus::Unavailable;
        const auto& session = found->second;
        const auto ownership = session.zoneTransition.GetOwnership();
        if (ownership.zoneID == 0 || ownership.zoneID != zoneID || ownership.ownershipEpoch != request.ownershipEpoch)
            return ParryStatus::StaleOwnership;
        return std::nullopt;
    }

    std::optional<ZoneTransfer> SessionManager::ReadZoneTransfer(const std::shared_ptr<ZoneTransfer>& transfer, uint16_t zoneID, bool returning) const {
        if (!transfer)
            return std::nullopt;
        const auto sessionID = transfer->character.sessionID;
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::shared_lock lock(shard.smutex);
        const auto found = shard.sessionMap.find(sessionID);
        if (found == shard.sessionMap.end() || found->second.disconnectQueued)
            return std::nullopt;
        return found->second.zoneTransition.ReadTransfer(transfer, zoneID, returning);
    }

    std::optional<ZoneTransferCommit> SessionManager::CompleteZoneTransfer(const std::shared_ptr<ZoneTransfer>& transfer, uint16_t zoneID, bool returning) {
        if (!transfer)
            return std::nullopt;
        const auto sessionID = transfer->character.sessionID;
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (!session || session->disconnectQueued || !session->zoneTransition.CompleteTransfer(transfer, zoneID, returning))
            return std::nullopt;
        if (!returning)
            lifecycleListener->OnSessionZoneChanged(sessionID, transfer->sourceZoneID, zoneID);
        return ZoneTransferCommit{session->chatID, session->zoneTransition.GetOwnership().ownershipEpoch, transfer->character.profileId, transfer->character.profileVersion};
    }

    bool SessionManager::CompleteLobbyLoad(uint64_t sessionID, uint64_t characterID) {
        if (characterID == INVALID_CHARACTER_ID)
            return false;
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (!session || session->disconnectQueued || !session->authenticated || session->zoneTransition.GetOwnership().zoneID != 0 ||
            (session->characterID != INVALID_CHARACTER_ID && session->characterID != characterID))
            return false;
        session->characterID = characterID;
        return true;
    }

    bool SessionManager::CompleteLobbyEntry(const ZoneSessionContext& context, uint32_t profileID, ZoneChangeResult& result) {
        const auto& [sessionID, zoneID, ownershipEpoch] = context;
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (!session || session->disconnectQueued || !session->authenticated || !session->authCompleted ||
            !session->zoneTransition.CompleteLobbyEntry(zoneID, ownershipEpoch))
            return false;
        if (session->chatID == 0)
            session->chatID = lifecycleListener->OnSessionEnteredWorld(sessionID, zoneID, profileID);
        else
            lifecycleListener->OnSessionZoneChanged(sessionID, 0, zoneID);
        result.chatID = session->chatID;
        result.ownershipEpoch = session->zoneTransition.GetOwnership().ownershipEpoch;
        return true;
    }

    RespawnRequest SessionManager::CheckRespawnRequest(const ZoneSessionContext& context) const {
        const auto& [sessionID, zoneID, ownershipEpoch] = context;
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::shared_lock lock(shard.smutex);
        const auto found = shard.sessionMap.find(sessionID);
        if (found == shard.sessionMap.end())
            return {};
        const auto& session = found->second;
        if (session.disconnectQueued || !session.authenticated || !session.authCompleted)
            return {};
        const auto status = session.zoneTransition.CheckRespawn(zoneID, ownershipEpoch);
        if (status == RespawnRequest::Status::Ready || status == RespawnRequest::Status::Replay)
            return {status, session.characterID};
        return {status};
    }

    bool SessionManager::CompleteTownRespawn(uint64_t sessionID, uint64_t ownershipEpoch, ZoneChangeResult& result) {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        // Preserve the queued disconnect epoch if closing started during restoration.
        if (!session || session->disconnectQueued || !session->zoneTransition.CompleteTownRespawn(ownershipEpoch))
            return false;
        result.chatID = session->chatID;
        result.ownershipEpoch = session->zoneTransition.GetOwnership().ownershipEpoch;
        return true;
    }

    bool SessionManager::CheckZoneAction(const ZoneSessionContext& context, std::optional<ZoneRequestRoute>& resync) {
        const auto& [sessionID, zoneID, ownershipEpoch] = context;
        resync.reset();
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (!session || session->disconnectQueued || !session->authenticated || !session->authCompleted)
            return false;
        return session->zoneTransition.CheckAction(zoneID, ownershipEpoch, resync);
    }

    bool SessionManager::BeginZoneTransfer(const std::shared_ptr<ZoneTransfer>& transfer) {
        if (!transfer)
            return false;
        const auto sessionID = transfer->character.sessionID;
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (!session || session->disconnectQueued || !session->authenticated || !session->authCompleted)
            return false;
        return session->zoneTransition.BeginTransfer(transfer);
    }

    void SessionManager::CancelZoneTransfer(const std::shared_ptr<ZoneTransfer>& transfer) {
        if (!transfer)
            return;
        const auto sessionID = transfer->character.sessionID;
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (session)
            session->zoneTransition.CancelTransfer(transfer);
    }

    SessionRequestStatus SessionManager::CheckEquipment(uint64_t sessionID, uint16_t zoneID, uint64_t requestID, uint64_t ownershipEpoch, bool write,
        uint8_t slot, uint32_t itemID, EquipmentResponseBody& cachedResponse) const {
        if (requestID == 0 || ownershipEpoch == 0)
            return SessionRequestStatus::Invalid;
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::shared_lock sessionLock(shard.smutex);
        auto session = shard.sessionMap.find(sessionID);
        if (session == shard.sessionMap.end() || session->second.disconnectQueued || !session->second.authenticated || !session->second.authCompleted ||
            session->second.characterID == 0)
            return SessionRequestStatus::Invalid;
        const auto ownership = session->second.zoneTransition.GetOwnership();
        if (zoneID == 0 || ownership.zoneID != zoneID || ownership.ownershipEpoch != ownershipEpoch)
            return SessionRequestStatus::StaleOwnership;
        auto& previous = session->second.lastEquipmentSet;
        if (write && requestID <= previous.requestID) {
            if (requestID == previous.requestID && ownershipEpoch == previous.ownershipEpoch && slot == previous.slot &&
                itemID == session->second.lastEquipmentItemID) {
                cachedResponse = previous;
                return SessionRequestStatus::Replay;
            }
            return SessionRequestStatus::StaleRequest;
        }
        return SessionRequestStatus::Ready;
    }

    std::vector<std::shared_ptr<ZoneTransfer>> SessionManager::Shutdown() {
        std::vector<std::shared_ptr<ZoneTransfer>> pending;
        if (!m_running.exchange(false, std::memory_order_relaxed))
            return pending;
        for (auto& shard : m_shards) {
            std::unique_lock lock(shard.smutex);
            for (auto& [sessionID, data] : shard.sessionMap)
                if (auto transfer = data.zoneTransition.TakePendingTransfer())
                    pending.push_back(std::move(transfer));
            shard.sessionMap.clear();
        }
        return pending;
    }

    bool SessionManager::CompleteZoneDisconnect(const ZoneSessionContext& context, std::shared_ptr<ZoneTransfer>& pending) {
        const auto& [sessionID, zoneID, ownershipEpoch] = context;
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        const auto it = shard.sessionMap.find(sessionID);
        if (it == shard.sessionMap.end() || !it->second.disconnectQueued)
            return false;
        const auto ownership = it->second.zoneTransition.GetOwnership();
        if (ownership.zoneID != zoneID || ownership.ownershipEpoch != ownershipEpoch)
            return false;

        pending = it->second.zoneTransition.TakePendingTransfer();
        shard.sessionMap.erase(it);
        lifecycleListener->OnSessionClosed(sessionID, zoneID);
        return true;
    }

    void SessionManager::RememberEquipment(uint64_t sessionID, uint16_t zoneID, uint32_t itemID, const EquipmentResponseBody& result) {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (!session)
            return;
        const auto ownership = session->zoneTransition.GetOwnership();
        if (ownership.zoneID == zoneID && ownership.ownershipEpoch == result.ownershipEpoch &&
            result.requestID > session->lastEquipmentSet.requestID) {
            session->lastEquipmentSet = result;
            session->lastEquipmentItemID = itemID;
        }
    }

    std::optional<LobbyEntryRequest> SessionManager::GetLobbyEntry(uint64_t sessionID, uint8_t op) const {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::shared_lock lock(shard.smutex);
        const auto found = shard.sessionMap.find(sessionID);
        if (op != ZONE_CHANGE::ENTER || found == shard.sessionMap.end())
            return std::nullopt;
        const auto& data = found->second;
        if (data.disconnectQueued || !data.authenticated || !data.authCompleted ||
            data.characterID == INVALID_CHARACTER_ID)
            return std::nullopt;
        return data.zoneTransition.GetLobbyEntry(data.characterID);
    }

    std::optional<ZoneRequestRoute> SessionManager::GetRespawnRoute(uint64_t sessionID, uint64_t ownershipEpoch) const {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::shared_lock lock(shard.smutex);
        const auto found = shard.sessionMap.find(sessionID);
        if (found == shard.sessionMap.end())
            return std::nullopt;
        const auto& data = found->second;
        if (data.disconnectQueued || !data.authenticated || !data.authCompleted || data.userID == 0 || data.characterID == INVALID_CHARACTER_ID)
            return std::nullopt;
        return data.zoneTransition.GetRespawnRoute(ownershipEpoch);
    }

    std::optional<ZoneRequestRoute> SessionManager::GetBootstrapRoute(uint64_t sessionID, uint64_t ownershipEpoch) const {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::shared_lock lock(shard.smutex);
        const auto found = shard.sessionMap.find(sessionID);
        if (found == shard.sessionMap.end() || found->second.disconnectQueued)
            return std::nullopt;
        return found->second.zoneTransition.GetBootstrapRoute(ownershipEpoch);
    }

    std::optional<DisconnectRequest> SessionManager::BeginDisconnect(uint64_t sessionID) {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (!session || session->disconnectQueued)
            return std::nullopt;
        session->disconnectQueued = true;
        const auto ownership = session->zoneTransition.GetOwnership();
        return DisconnectRequest{ownership.zoneID, ownership.ownershipEpoch, session->chatID};
    }

    bool SessionManager::CompleteLobbyDisconnect(uint64_t sessionID, uint64_t ownershipEpoch) {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (!session || !session->disconnectQueued)
            return false;
        const auto ownership = session->zoneTransition.GetOwnership();
        if (ownership.zoneID != 0 || ownership.ownershipEpoch != ownershipEpoch)
            return false;
        const bool enteredWorld = session->chatID != 0;
        shard.sessionMap.erase(sessionID);
        if (enteredWorld)
            lifecycleListener->OnSessionClosed(sessionID, 0);
        return true;
    }
}
