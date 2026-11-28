#include "pch.h"
#include "SessionManager.h"
#include "Interface/ISessionLifecycleListener.h"
#include <algorithm>
#include <limits>

namespace Core {
    std::optional<uint16_t> SessionManager::RouteProfileVersion(const ProfileVersionUpdate& update) {
        auto& shard = m_shards[update.sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, update.sessionID);
        if (!session)
            return std::nullopt;
        if (session->pendingZoneTransfer) {
            session->pendingZoneTransfer->character.UpdateProfileVersion(update.profileID, update.version);
            return std::nullopt;
        }
        return session->zoneID;
    }

    std::optional<ZoneBootstrapInfo> SessionManager::PrepareZoneBootstrap(const ZoneSessionContext& context, bool resendZoneChange) {
        const auto& [sessionID, zoneID, ownershipEpoch] = context;
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (!session || session->disconnectQueued || session->zoneID != zoneID || session->ownershipEpoch != ownershipEpoch || session->pendingZoneTransfer)
            return std::nullopt;
        ZoneBootstrapInfo info{session->chatID};
        if (!resendZoneChange) {
            info.bootstrapID = NextBootstrapIDLocked(*session);
            if (info.bootstrapID == 0)
                return std::nullopt;
        }
        return info;
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
        if (session.zoneID == 0 || session.zoneID != zoneID || session.ownershipEpoch != request.ownershipEpoch)
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
        const auto* session = found != shard.sessionMap.end() ? &found->second : nullptr;
        if (!MatchesZoneTransferLocked(session, transfer) || returning != (zoneID == transfer->sourceZoneID))
            return std::nullopt;
        // Profile updates can change the pending record; admission uses a private copy.
        return *transfer;
    }

    std::optional<ZoneTransferCommit> SessionManager::CompleteZoneTransfer(const std::shared_ptr<ZoneTransfer>& transfer, uint16_t zoneID, bool returning) {
        if (!transfer)
            return std::nullopt;
        const auto sessionID = transfer->character.sessionID;
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (!MatchesZoneTransferLocked(session, transfer) || returning != (zoneID == transfer->sourceZoneID))
            return std::nullopt;

        session->pendingZoneTransfer.reset();
        if (!returning) {
            if (zoneID != 0 && transfer->IsRespawn())
                session->lastRespawnRequestEpoch = transfer->ownershipEpoch;
            UpdateZoneOwnershipLocked(*session, zoneID);
            lifecycleListener->OnSessionZoneChanged(sessionID, transfer->sourceZoneID, zoneID);
        }
        return ZoneTransferCommit{session->chatID, session->ownershipEpoch, transfer->character.profileId, transfer->character.profileVersion};
    }

    bool SessionManager::CompleteLobbyLoad(uint64_t sessionID, uint64_t characterID) {
        if (characterID == INVALID_CHARACTER_ID)
            return false;
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (!session || session->disconnectQueued || !session->authenticated || session->zoneID != 0 ||
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
        if (!session || session->disconnectQueued || !session->authenticated || !session->authCompleted || session->zoneID != 0 ||
            session->ownershipEpoch != ownershipEpoch)
            return false;
        UpdateZoneOwnershipLocked(*session, zoneID);
        if (session->chatID == 0)
            session->chatID = lifecycleListener->OnSessionEnteredWorld(sessionID, zoneID, profileID);
        else
            lifecycleListener->OnSessionZoneChanged(sessionID, 0, zoneID);
        result.chatID = session->chatID;
        result.ownershipEpoch = session->ownershipEpoch;
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
        if (session.disconnectQueued || !session.authenticated || !session.authCompleted || session.zoneID != zoneID)
            return {};
        if (session.pendingZoneTransfer && session.pendingZoneTransfer->IsRespawn() && session.ownershipEpoch == ownershipEpoch)
            return {};
        if (session.pendingZoneTransfer || ownershipEpoch == (std::numeric_limits<uint64_t>::max)())
            return {RespawnRequest::Status::Rejected};
        if (session.ownershipEpoch == ownershipEpoch)
            return {RespawnRequest::Status::Ready, session.characterID};
        if (zoneID == TOWN_ZONE_ID && session.lastRespawnRequestEpoch == ownershipEpoch && session.ownershipEpoch == ownershipEpoch + 1)
            return {RespawnRequest::Status::Replay, session.characterID};
        return {RespawnRequest::Status::Rejected};
    }

    bool SessionManager::CompleteTownRespawn(uint64_t sessionID, uint64_t ownershipEpoch, ZoneChangeResult& result) {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        // Disconnect may have started while the Zone worker restored the character.
        // Keep its queued epoch valid and let that worker save the final state.
        if (!session || session->disconnectQueued || session->zoneID != TOWN_ZONE_ID || session->pendingZoneTransfer ||
            ownershipEpoch == (std::numeric_limits<uint64_t>::max)())
            return false;
        if (session->ownershipEpoch == ownershipEpoch) {
            session->lastRespawnRequestEpoch = ownershipEpoch;
            UpdateZoneOwnershipLocked(*session, TOWN_ZONE_ID);
        } else if (session->lastRespawnRequestEpoch != ownershipEpoch || session->ownershipEpoch != ownershipEpoch + 1) {
            return false;
        }
        result.chatID = session->chatID;
        result.ownershipEpoch = session->ownershipEpoch;
        return true;
    }

    bool SessionManager::CheckZoneAction(const ZoneSessionContext& context, std::optional<ZoneRequestRoute>& resync) {
        const auto& [sessionID, zoneID, ownershipEpoch] = context;
        resync.reset();
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (!session || session->disconnectQueued || !session->authenticated || !session->authCompleted || session->zoneID == 0 ||
            session->ownershipEpoch == 0 || session->pendingZoneTransfer)
            return false;
        if (session->zoneID != zoneID || session->ownershipEpoch != ownershipEpoch) {
            resync = ZoneRequestRoute{session->zoneID, session->ownershipEpoch};
            return false;
        }
        session->lastRespawnRequestEpoch = 0;
        return true;
    }

    bool SessionManager::BeginZoneTransfer(const std::shared_ptr<ZoneTransfer>& transfer) {
        if (!transfer)
            return false;
        const auto sessionID = transfer->character.sessionID;
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (!session || session->disconnectQueued || !session->authenticated || !session->authCompleted || session->zoneID == 0 ||
            session->zoneID != transfer->sourceZoneID || session->ownershipEpoch != transfer->ownershipEpoch || session->pendingZoneTransfer)
            return false;
        session->pendingZoneTransfer = transfer;
        return true;
    }

    void SessionManager::CancelZoneTransfer(const std::shared_ptr<ZoneTransfer>& transfer) {
        if (!transfer)
            return;
        const auto sessionID = transfer->character.sessionID;
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (session && session->pendingZoneTransfer == transfer)
            session->pendingZoneTransfer.reset();
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
        if (zoneID == 0 || session->second.zoneID != zoneID || ownershipEpoch != session->second.ownershipEpoch)
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

    void SessionManager::UpdateZoneOwnershipLocked(SessionData& session, uint16_t destinationID) {
        session.zoneID = destinationID;
        ++session.ownershipEpoch;
    }

    bool SessionManager::MatchesZoneTransferLocked(const SessionData* session, const std::shared_ptr<ZoneTransfer>& transfer) {
        return session && !session->disconnectQueued && session->pendingZoneTransfer == transfer && session->zoneID == transfer->sourceZoneID &&
               session->ownershipEpoch == transfer->ownershipEpoch;
    }

    uint64_t SessionManager::NextBootstrapIDLocked(SessionData& session) {
        if (session.bootstrapSequence == (std::numeric_limits<uint64_t>::max)())
            return 0;
        return ++session.bootstrapSequence;
    }

    std::vector<std::shared_ptr<ZoneTransfer>> SessionManager::Shutdown() {
        std::vector<std::shared_ptr<ZoneTransfer>> pending;
        if (!m_running.exchange(false, std::memory_order_relaxed))
            return pending;
        for (auto& shard : m_shards) {
            std::unique_lock lock(shard.smutex);
            for (auto& [sessionID, data] : shard.sessionMap)
                if (data.pendingZoneTransfer)
                    pending.push_back(std::move(data.pendingZoneTransfer));
            shard.sessionMap.clear();
        }
        return pending;
    }

    bool SessionManager::CompleteZoneDisconnect(const ZoneSessionContext& context, std::shared_ptr<ZoneTransfer>& pending) {
        const auto& [sessionID, zoneID, ownershipEpoch] = context;
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        const auto it = shard.sessionMap.find(sessionID);
        if (it == shard.sessionMap.end() || !it->second.disconnectQueued || it->second.zoneID != zoneID || it->second.ownershipEpoch != ownershipEpoch)
            return false;

        pending = std::move(it->second.pendingZoneTransfer);
        shard.sessionMap.erase(it);
        lifecycleListener->OnSessionClosed(sessionID, zoneID);
        return true;
    }

    void SessionManager::RememberEquipment(uint64_t sessionID, uint16_t zoneID, uint32_t itemID, const EquipmentResponseBody& result) {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (session && session->zoneID == zoneID && session->ownershipEpoch == result.ownershipEpoch &&
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
        if (data.disconnectQueued || !data.authenticated || !data.authCompleted || data.characterID == INVALID_CHARACTER_ID || data.zoneID != 0 ||
            data.ownershipEpoch == (std::numeric_limits<uint64_t>::max)())
            return std::nullopt;
        return LobbyEntryRequest{data.characterID, data.ownershipEpoch};
    }

    std::optional<ZoneRequestRoute> SessionManager::GetRespawnRoute(uint64_t sessionID, uint64_t ownershipEpoch) const {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::shared_lock lock(shard.smutex);
        const auto found = shard.sessionMap.find(sessionID);
        if (found == shard.sessionMap.end())
            return std::nullopt;
        const auto& data = found->second;
        if (data.disconnectQueued || !data.authenticated || !data.authCompleted || data.userID == 0 || data.characterID == INVALID_CHARACTER_ID ||
            data.zoneID == 0 || data.zoneID > ZONE_COUNT || ownershipEpoch == (std::numeric_limits<uint64_t>::max)())
            return std::nullopt;
        if (data.pendingZoneTransfer) {
            if (!data.pendingZoneTransfer->IsRespawn() || ownershipEpoch != data.ownershipEpoch)
                return std::nullopt;
            return ZoneRequestRoute{data.zoneID, ownershipEpoch, true};
        }
        const bool replay = data.zoneID == TOWN_ZONE_ID && data.lastRespawnRequestEpoch == ownershipEpoch && data.ownershipEpoch == ownershipEpoch + 1;
        if (data.ownershipEpoch != ownershipEpoch && !replay)
            return std::nullopt;
        return ZoneRequestRoute{data.zoneID, ownershipEpoch};
    }

    std::optional<ZoneRequestRoute> SessionManager::GetBootstrapRoute(uint64_t sessionID, uint64_t ownershipEpoch) const {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::shared_lock lock(shard.smutex);
        const auto found = shard.sessionMap.find(sessionID);
        if (found == shard.sessionMap.end())
            return std::nullopt;
        const auto& data = found->second;
        if (data.disconnectQueued || data.zoneID == 0 || data.ownershipEpoch == 0 || data.pendingZoneTransfer || data.ownershipEpoch != ownershipEpoch)
            return std::nullopt;
        return ZoneRequestRoute{data.zoneID, ownershipEpoch};
    }

    std::optional<DisconnectRequest> SessionManager::BeginDisconnect(uint64_t sessionID) {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (!session || session->disconnectQueued)
            return std::nullopt;
        session->disconnectQueued = true;
        return DisconnectRequest{session->zoneID, session->ownershipEpoch, session->chatID};
    }

    bool SessionManager::CompleteLobbyDisconnect(uint64_t sessionID, uint64_t ownershipEpoch) {
        auto& shard = m_shards[sessionID & SHARD_SIZE_MASK];
        std::unique_lock lock(shard.smutex);
        auto* session = FindSessionLocked(shard, sessionID);
        if (!session || !session->disconnectQueued || session->zoneID != 0 || session->ownershipEpoch != ownershipEpoch)
            return false;
        const bool enteredWorld = session->chatID != 0;
        shard.sessionMap.erase(sessionID);
        if (enteredWorld)
            lifecycleListener->OnSessionClosed(sessionID, 0);
        return true;
    }
}
