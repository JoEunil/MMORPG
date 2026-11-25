#pragma once

#include <cstdint>
#include <string>
#include <variant>

#include "../Packet/PacketTypes.h"

namespace Core {
    struct ChatDestKey {
        CHAT_SCOPE scope;
        uint64_t id; // zone, party, guild, targetChatID
        bool operator==(const ChatDestKey& other) const noexcept { return scope == other.scope && id == other.id; }
    };

    struct ChatSessionAddEvent {
        uint64_t senderSessionID;
        uint64_t senderChatID;
        uint32_t senderProfileId;
    };

    struct ChatSessionDeleteEvent {
        uint64_t senderSessionID;
    };

    struct ChatMessageEvent {
        uint64_t senderSessionID;
        ChatDestKey key;
        std::string message;
    };

    struct ChatZoneJoinEvent {
        uint64_t senderSessionID;
        uint16_t zoneID;
        uint64_t zoneSequence = 0;
    };

    struct ChatZoneLeaveEvent {
        uint64_t senderSessionID;
        uint16_t zoneID;
        uint64_t zoneSequence = 0;
    };

    using ChatEvent = std::variant<ChatSessionAddEvent, ChatSessionDeleteEvent, ChatMessageEvent,
        ChatZoneJoinEvent, ChatZoneLeaveEvent>;

}
