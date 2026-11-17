#pragma once
#include "Config.h"
#include "WorldConfig.h"
#include "ZoneState.h"
#include "SessionManager.h"
#include "LoggerGlobal.h"
#include "ChatThreadPool.h"

namespace Core {
    class LobbyZone {
        // 월드 진입 전 db 비동기 결과 보관.
        std::unordered_map<uint64_t, CharacterState> m_chars;
        std::mutex m_mutex;
        
        SessionManager* sessionManager = nullptr;
        void Initialize(SessionManager* s) {
            m_chars.reserve(MAX_USER_CAPACITY);
            sessionManager = s;
        }
        bool IsReady() {
            if (sessionManager == nullptr) {
                sysLogger->LogError("lobby zone", "sessionManager not initialized");
                return false;
            }
            return true;
        }
        static uint16_t EntryZone(const CharacterState& character) {
            return character.lastZone == 0 ? TOWN_ZONE_ID : character.lastZone;
        }

        friend class Initializer;
        friend class SessionManager;
    public:
        uint16_t FindEntryZone(uint64_t sessionID, uint64_t characterID) {
            std::lock_guard lock(m_mutex);
            const auto loaded = m_chars.find(sessionID);
            if (loaded == m_chars.end() || loaded->second.characterID != characterID)
                return 0;
            return EntryZone(loaded->second);
        }

        template<class Admit>
        bool TryEnterField(uint64_t sessionID, uint64_t characterID, uint16_t zoneID, Admit admit) {
            std::lock_guard lock(m_mutex);
            const auto loaded = m_chars.find(sessionID);
            if (loaded == m_chars.end() || loaded->second.characterID != characterID
                || EntryZone(loaded->second) != zoneID || !admit(loaded->second))
                return false;
            m_chars.erase(loaded);
            return true;
        }

        void Disconnect(uint64_t sessionID) {
            // db에서 load 후 변경되지 않은 상태
            std::lock_guard lock(m_mutex);
            if (m_chars.erase(sessionID) == 0)
                gameLogger->LogWarn("lobby zone", "Character State not exist", "sessionID", sessionID);
        }

        bool ImmigrateChar(uint64_t sessionID, const CharacterState& character) {
            if (character.characterID == INVALID_CHARACTER_ID)
                return false;
            // Same order as SessionManager::Disconnect/CleanUp: shard -> lobby.
            auto& shard = sessionManager->m_shards[sessionID & SHARD_SIZE_MASK];
            std::unique_lock<std::shared_mutex> sessionLock(shard.smutex);
            const auto session = shard.sessionMap.find(sessionID);
            if (session == shard.sessionMap.end())
                return false;
            auto& data = session->second;
            if (!data.authenticated || data.zoneID != 0
                || (data.characterID != INVALID_CHARACTER_ID && data.characterID != character.characterID))
                return false;

            std::lock_guard lock(m_mutex);
            if (!m_chars.try_emplace(sessionID, character).second) {
                gameLogger->LogError("lobby zone", "ImmigrageChar Failed sessionID already exists", "sessionID", sessionID);
                return false;
            }
            if (data.characterID == INVALID_CHARACTER_ID)
                data.characterID = character.characterID;
            return true;
        }

        uint64_t ReturnFieldTransfer(const std::shared_ptr<FieldZoneTransfer>& transfer, ChatThreadPool& chat) {
            const auto sessionID = transfer->character.sessionID;
            auto& shard = sessionManager->m_shards[sessionID & SHARD_SIZE_MASK];
            std::unique_lock sessionLock(shard.smutex);
            const auto session = shard.sessionMap.find(sessionID);
            if (session == shard.sessionMap.end() || session->second.pendingFieldTransfer != transfer
                || session->second.ownershipEpoch != transfer->ownershipEpoch
                || session->second.zoneID != transfer->sourceZoneID
                || transfer->IsRespawn())
                return 0;

            auto& data = session->second;
            {
                std::lock_guard lobbyLock(m_mutex);
                if (!m_chars.try_emplace(sessionID, std::move(transfer->character)).second)
                    return 0;
            }
            SessionManager::UpdateZoneOwnershipLocked(data, 0);
            data.pendingFieldTransfer.reset();
            chat.EnqueueZoneLeave(sessionID, transfer->sourceZoneID);
            return data.ownershipEpoch;
        }

        void UpdateProfileVersion(uint64_t sessionID, uint32_t profileID, uint32_t version) {
            std::lock_guard lock(m_mutex);
            const auto it = m_chars.find(sessionID);
            if (it != m_chars.end())
                it->second.UpdateProfileVersion(profileID, version);
        }

        bool EmigrateChar(uint64_t sessionID, CharacterState& out) {
            std::lock_guard lock(m_mutex);
            const auto it = m_chars.find(sessionID);
            if (it == m_chars.end()) {
                gameLogger->LogError("lobby zone", "EmigrageChar Failed sessionID not exists", "sessionID", sessionID);
                return false;
            }
            out = it->second;
            m_chars.erase(it);
            return true;
        }

    };

}
