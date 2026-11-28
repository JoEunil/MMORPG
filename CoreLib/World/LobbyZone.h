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
        static uint16_t EntryZone(const CharacterState& character) { return character.lastZone == 0 ? TOWN_ZONE_ID : character.lastZone; }

        friend class Initializer;

      public:
        uint16_t FindEntryZone(uint64_t sessionID, uint64_t characterID) {
            std::lock_guard lock(m_mutex);
            const auto loaded = m_chars.find(sessionID);
            if (loaded == m_chars.end() || loaded->second.characterID != characterID)
                return 0;
            return EntryZone(loaded->second);
        }

        ZoneOperationResult EnterField(uint64_t sessionID, uint64_t ownershipEpoch, ZoneState& zone, ZoneChangeResult& result) {
            // Lobby operations acquire Lobby before Session; SessionManager never locks Lobby.
            std::lock_guard lock(m_mutex);
            const auto entry = sessionManager->GetLobbyEntry(sessionID, ZONE_CHANGE::ENTER);
            if (!entry || entry->ownershipEpoch != ownershipEpoch)
                return ZoneOperationResult::NoResponse;
            const auto loaded = m_chars.find(sessionID);
            if (loaded == m_chars.end() || loaded->second.characterID != entry->characterID || EntryZone(loaded->second) != zone.GetZoneID() ||
                !zone.AdmitLobbyCharacter(sessionID, loaded->second, result))
                return ZoneOperationResult::Rejected;
            if (!sessionManager->CompleteLobbyEntry({sessionID, zone.GetZoneID(), ownershipEpoch}, loaded->second.profileId, result)) {
                CharacterState discarded;
                if (!zone.EmigrateChar(sessionID, discarded))
                    errorLogger->LogError("lobby zone", "could not roll back field entry", {{"sessionID", sessionID}});
                return ZoneOperationResult::NoResponse;
            }
            m_chars.erase(loaded);
            return ZoneOperationResult::Completed;
        }

        void Disconnect(uint64_t sessionID) {
            // db에서 load 후 변경되지 않은 상태
            std::lock_guard lock(m_mutex);
            if (m_chars.erase(sessionID) == 0)
                gameLogger->LogWarn("lobby zone", "Character State not exist", {{"sessionID", sessionID}});
        }

        bool ImmigrateChar(uint64_t sessionID, const CharacterState& character) {
            std::lock_guard lock(m_mutex);
            const auto [entry, inserted] = m_chars.try_emplace(sessionID, character);
            if (!inserted) {
                gameLogger->LogError("lobby zone", "ImmigrageChar Failed sessionID already exists", {{"sessionID", sessionID}});
                return false;
            }
            if (!sessionManager->CompleteLobbyLoad(sessionID, character.characterID)) {
                m_chars.erase(entry);
                return false;
            }
            return true;
        }

        std::optional<uint64_t> ReturnZoneTransfer(const std::shared_ptr<ZoneTransfer>& transfer) {
            std::lock_guard lock(m_mutex);
            const auto snapshot = sessionManager->ReadZoneTransfer(transfer, 0, false);
            if (!snapshot)
                return std::nullopt;
            const auto sessionID = snapshot->character.sessionID;
            const auto [entry, inserted] = m_chars.try_emplace(sessionID, snapshot->character);
            if (!inserted)
                return std::nullopt;
            const auto committed = sessionManager->CompleteZoneTransfer(transfer, 0, false);
            if (!committed) {
                m_chars.erase(entry);
                return std::nullopt;
            }
            entry->second.UpdateProfileVersion(committed->profileID, committed->profileVersion);
            return committed->ownershipEpoch;
        }

        std::optional<uint16_t> ApplyProfileVersion(const ProfileVersionUpdate& update) {
            std::lock_guard lock(m_mutex);
            const auto route = sessionManager->RouteProfileVersion(update);
            if (!route || *route != 0)
                return route;
            const auto found = m_chars.find(update.sessionID);
            if (found != m_chars.end())
                found->second.UpdateProfileVersion(update.profileID, update.version);
            return std::nullopt;
        }

        bool TakeCharacter(CharacterState& out) {
            std::lock_guard lock(m_mutex);
            if (m_chars.empty())
                return false;
            auto entry = m_chars.begin();
            out = std::move(entry->second);
            m_chars.erase(entry);
            return true;
        }

        bool EmigrateChar(uint64_t sessionID, CharacterState& out) {
            std::lock_guard lock(m_mutex);
            const auto it = m_chars.find(sessionID);
            if (it == m_chars.end()) {
                gameLogger->LogError("lobby zone", "EmigrageChar Failed sessionID not exists", {{"sessionID", sessionID}});
                return false;
            }
            out = it->second;
            m_chars.erase(it);
            return true;
        }
    };

}
