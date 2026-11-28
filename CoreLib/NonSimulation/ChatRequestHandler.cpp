#include "pch.h"
#include "ChatRequestHandler.h"
#include "../Packet/PacketTypes.h"
#include "../World/SessionManager.h"
#include "ChatThreadPool.h"
#include <string>
#include <utility>

namespace Core {
    bool ChatRequestHandler::Chat(IPacketView* p) {
        auto body = parseBody<ChatRequestBody>(p->GetPtr());
        uint8_t* startPtr = p->GetPtr() + sizeof(PacketStruct<ChatRequestBody>);
        auto session = p->GetSessionID();
        auto zoneID = sessionManager->GetZoneID(session);
        ChatMessageEvent event{};
        event.senderSessionID = session;
        event.key.scope = body->scope;
        switch (body->scope) {
        case CHAT_SCOPE::Whisper:
            event.key.id = body->targetChatID;
            break;
        case CHAT_SCOPE::Zone:
            event.key.id = static_cast<uint64_t>(zoneID);
        }
        if (p->GetLength() < sizeof(PacketStruct<ChatRequestBody>) + body->messageLength) {
            errorLogger->LogError("non zone handler", "message length invalid", {{"session", session}, {"messageLength", body->messageLength}});
            return false;
        }
        event.message = std::string(reinterpret_cast<char*>(startPtr), body->messageLength);
        ChatEvent queued = std::move(event);
        chat->EnqueueChat(queued);
            return true;
    }
}
