#pragma once
#include "../Config.h"
#include "WorldConfig.h"
#include "ZoneState.h"
#include "SessionManager.h"
#include "../LoggerGlobal.h"

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
        bool IsReady() const {
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
            return sessionManager->LoadLobbyCharacter(sessionID, character.characterID, [&] {
                std::lock_guard lock(m_mutex);
                if (m_chars.try_emplace(sessionID, character).second)
                    return true;
                gameLogger->LogError("lobby zone", "ImmigrageChar Failed sessionID already exists", "sessionID", sessionID);
                return false;
            });
        }

        uint64_t ReturnZoneTransfer(const std::shared_ptr<ZoneTransfer>& transfer) {
            const auto sessionID = transfer->character.sessionID;
            return sessionManager->ReturnTransferToLobby(transfer, [&] {
                std::lock_guard lock(m_mutex);
                return m_chars.try_emplace(sessionID, std::move(transfer->character)).second;
            });
        }

        std::optional<uint16_t> ApplyProfileVersion(const ProfileVersionUpdate& update) {
            return sessionManager->ApplyZoneProfileVersion(0, update, [&] {
                UpdateProfileVersion(update.sessionID, update.profileID, update.version);
            });
        }

        bool TakeCharacter(CharacterState& out) {
            std::lock_guard lock(m_mutex);
            if (m_chars.empty()) return false;
            auto entry = m_chars.begin();
            out = std::move(entry->second);
            m_chars.erase(entry);
            return true;
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
