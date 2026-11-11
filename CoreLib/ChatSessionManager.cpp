#include "pch.h"
#include "ChatSessionManager.h"
#include "Config.h"
#include "LoggerGlobal.h"

namespace Core {
    void ChatSessionManager::Initialize() {
        m_sessionChatIdMap.reserve(MAX_USER_CAPACITY);
        m_zoneMembers.resize(ZONE_COUNT);
    }

    void ChatSessionManager::AddSession(uint64_t sessionID, uint64_t chatID, uint32_t profileId) {
        auto it = m_sessionChatIdMap.find(sessionID);
        if (it != m_sessionChatIdMap.end()) {
            errorLogger->LogInfo("chat thread", "chatID already exist", "chatID", chatID, "session", sessionID);
            return;
        }
        auto& node = m_sessionChatIdMap[sessionID];
        node.chatID = chatID;
        node.profileId = profileId;
        node.zoneID = 0;
        m_chatIdSessionMap[chatID] = sessionID;
    }

    void ChatSessionManager::DeleteSession(uint64_t sessionID) {
        auto it = m_sessionChatIdMap.find(sessionID);
        if (it == m_sessionChatIdMap.end()) {
            errorLogger->LogInfo("chat thread", "chatID doesn't exist", "session", sessionID);
            return;
        }
        const uint16_t currentZoneID = it->second.zoneID;
        if (currentZoneID > 0 && currentZoneID <= ZONE_COUNT) {
            m_zoneMembers[currentZoneID - 1].erase(sessionID);
        }
        m_chatIdSessionMap.erase(it->second.chatID);
        m_sessionChatIdMap.erase(it);
    }

    void ChatSessionManager::JoinZone(uint64_t sessionID, uint16_t zoneID) {
        if (zoneID == 0 || zoneID > ZONE_COUNT)
            return;

        auto sessionIt = m_sessionChatIdMap.find(sessionID);
        if (sessionIt == m_sessionChatIdMap.end()) {
            errorLogger->LogInfo("chat thread", "chatID doesn't exist", "session", sessionID);
            return;
        }

        const uint16_t currentZoneID = sessionIt->second.zoneID;
        if (currentZoneID == zoneID)
            return;

        if (currentZoneID > 0 && currentZoneID <= ZONE_COUNT) {
            m_zoneMembers[currentZoneID - 1].erase(sessionID);
        }

        m_zoneMembers[zoneID - 1].insert(sessionID);
        sessionIt->second.zoneID = zoneID;
    }

    void ChatSessionManager::LeaveZone(uint64_t sessionID) {
        auto sessionIt = m_sessionChatIdMap.find(sessionID);
        if (sessionIt == m_sessionChatIdMap.end()) {
            errorLogger->LogInfo("chat thread", "chatID doesn't exist", "session", sessionID);
            return;
        }

        const uint16_t currentZoneID = sessionIt->second.zoneID;
        if (currentZoneID == 0)
            return;

        if (currentZoneID <= ZONE_COUNT) {
            m_zoneMembers[currentZoneID - 1].erase(sessionID);
        }
        sessionIt->second.zoneID = 0;
    }

    const ChatUserInfo* ChatSessionManager::FindSession(uint64_t sessionID) const {
        auto it = m_sessionChatIdMap.find(sessionID);
        return it == m_sessionChatIdMap.end() ? nullptr : &it->second;
    }

    std::optional<uint64_t> ChatSessionManager::FindSessionID(uint64_t chatID) const {
        auto it = m_chatIdSessionMap.find(chatID);
        if (it == m_chatIdSessionMap.end())
            return std::nullopt;
        return it->second;
    }

    const std::set<uint64_t>* ChatSessionManager::FindZoneMembers(uint64_t zoneID) const {
        if (zoneID == 0 || zoneID > m_zoneMembers.size())
            return nullptr;
        return &m_zoneMembers[zoneID - 1];
    }
}
