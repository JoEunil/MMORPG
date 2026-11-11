#pragma once

#include <cstdint>
#include <optional>
#include <set>
#include <unordered_map>
#include <vector>

namespace Core {
    struct ChatUserInfo {
        uint64_t chatID;
        uint32_t profileId;
        uint16_t zoneID = 0;
    };

    class ChatSessionManager {
        std::unordered_map<uint64_t, ChatUserInfo> m_sessionChatIdMap;
        std::unordered_map<uint64_t, uint64_t> m_chatIdSessionMap;
        std::vector<std::set<uint64_t>> m_zoneMembers;

    public:
        void Initialize();
        void AddSession(uint64_t sessionID, uint64_t chatID, uint32_t profileId);
        void DeleteSession(uint64_t sessionID);
        void JoinZone(uint64_t sessionID, uint16_t zoneID);
        void LeaveZone(uint64_t sessionID);

        const ChatUserInfo* FindSession(uint64_t sessionID) const;
        std::optional<uint64_t> FindSessionID(uint64_t chatID) const;
        const std::set<uint64_t>* FindZoneMembers(uint64_t zoneID) const;

        const std::unordered_map<uint64_t, ChatUserInfo>& GetSessions() const {
            return m_sessionChatIdMap;
        }
    };
}
