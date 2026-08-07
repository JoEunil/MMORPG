#include "pch.h"
#include "NonZoneHandler.h"

#include "PacketTypes.h"
#include "IPacketView.h"
#include "ISessionAuth.h"
#include "IIOCP.h"
#include "PacketWriter.h"
#include "ZoneState.h"
#include "Message.h"
#include "MessagePool.h"
#include "IMessageQueue.h"
#include "StateManager.h"
#include "LobbyZone.h"
#include "ChatThreadPool.h"
#include "LoggerGlobal.h"
#include "Config.h"

namespace Core {
    static IIOCP* iocp;
    static PacketWriter* writer;\
    static StateManager* stateManager;
    // 콜백에서 쓰기 위함

    void NonZoneHandler::Initialize(IIOCP* i, ISessionAuth* s, PacketWriter* p, MessagePool* m, IMessageQueue* mq, StateManager* manager, LobbyZone* lobby, ChatThreadPool* c) {
        iocp = i;
        auth = s;
        writer = p;
        messagePool = m;
        messageQueue = mq;
        stateManager = manager;
        lobbyZone = lobby;
        chat = c;
    }

    bool NonZoneHandler::IsReady() {
        if (iocp == nullptr) return false;
        if (auth == nullptr) return false;
        if (writer == nullptr) return false;
        if (messagePool == nullptr) return false;
        if (messageQueue == nullptr) return false;
        if (stateManager == nullptr) return false;
        if (lobbyZone == nullptr) return false;
        
        return true;
    }

    void NonZoneHandler::Process(IPacketView* p) {
        PacketHeader* h = parseHeader(p->GetPtr());
        uint16_t zoneID = 0;
        if (p->GetOpcode() != OP::AUTH ) {
            zoneID = stateManager->GetZoneID(p->GetSessionID());
        }
        
        switch (p->GetOpcode())
        {
            case OP::AUTH:
                CheckSession(p);
                break;
            case OP::CHARACTER_LIST:
                GetCharacterList(p);
                break;
            case OP::ENTER_WORLD:
                GetCharacterState(p);
                break;
            case OP::CHAT:
                Chat(p);
                break;
            case OP::ZONE_CHANGE:
                ZoneChange(p);
                break;
            default:
                errorLogger->LogInfo("non zone handler",  "UnDefined OPCODE", "opcode", p->GetOpcode(),"header opcode", h->opcode,"sessionId", p->GetSessionID());
                break;
        }
        
    }

    void NonZoneHandler::Disconnect(uint64_t sessionID) {
        stateManager->Disconnect(sessionID);
    }

    static void ResponseSession(uint64_t sessionID, uint8_t resStatus, uint64_t userID) {
        stateManager->AddSession(sessionID, userID, resStatus == RES_STATUS::SUCCESS);
        auto res = writer->WriteAuthResponse(resStatus);
        if (res == nullptr)
            return;
        iocp->SendDataUnique(sessionID, std::move(res));
    }

    void NonZoneHandler::CheckSession(IPacketView* p) {
        AuthRequestBody* body = parseBody<AuthRequestBody>(p->GetPtr());
        SessionCallbackData* privdata = new SessionCallbackData;
        privdata->sessionID = p->GetSessionID();
        privdata->userID = body->userID;
        std::memcpy(privdata->sessionToken.data(), body->sessionToken, privdata->sessionToken.size());
        privdata->callbackFunc = &ResponseSession;
        auth->CheckSession(privdata);
    }

    void NonZoneHandler::GetCharacterList(IPacketView* p) {
        Message* msg = messagePool->Acquire();
        if (msg == nullptr) {
            p = nullptr;
            return ;
		}
        MsgStruct<MsgCharacterListReqBody>* st = reinterpret_cast<MsgStruct<MsgCharacterListReqBody>*>(msg->GetBuffer());
        
        st->header.sessionID = p->GetSessionID();
        st->header.messageType = MSG_CHARACTER_LIST_REQ;
        st->body.channelID = CHANNEL_ID;
        st->body.userID = stateManager->GetUserID(p->GetSessionID());
        msg->SetLength(sizeof(MsgStruct<MsgCharacterListReqBody>));
        messageQueue->EnqueueMessage(msg);
        messagePool->Return(msg);
    }

    void NonZoneHandler::GetCharacterState(IPacketView* p) {
        Message* msg = messagePool->Acquire();
        if (msg == nullptr) {
            return;
        }
        MsgStruct<MsgCharacterStateReqBody>* st = reinterpret_cast<MsgStruct<MsgCharacterStateReqBody>*>(msg->GetBuffer());
        
        auto packetBody = parseBody<EnterWorldRequestBody>(p->GetPtr());
        
        st->header.sessionID = p->GetSessionID();
        st->header.messageType = MSG_CHARACTER_STATE_REQ;
        st->body.channelID = CHANNEL_ID;
        st->body.userID = stateManager->GetUserID(p->GetSessionID());
        st->body.characterID = packetBody->characterID;

        msg->SetLength(sizeof(MsgStruct<MsgCharacterStateReqBody>));
        messageQueue->EnqueueMessage(msg);
        messagePool->Return(msg);
    }

    void NonZoneHandler::GetInventory(IPacketView* p) {
        auto charID = stateManager->GetCharacterID(p->GetSessionID());
        if (charID <= 0)
            return ;
        
        Message* msg = messagePool->Acquire();
        if (msg == nullptr) {
            return;
        }
        MsgStruct<MsgInventoryReqBody>* st = reinterpret_cast<MsgStruct<MsgInventoryReqBody>*>(msg->GetBuffer());
        
        st->header.sessionID = p->GetSessionID();
        st->header.messageType = MSG_INVENTORY_REQ;
        st->body.characterID = charID;
        msg->SetLength(sizeof(MsgStruct<MsgInventoryReqBody>));
        messageQueue->EnqueueMessage(msg);
        messagePool->Return(msg);
    }

    void NonZoneHandler::Chat(IPacketView* p) {
        auto body = parseBody<ChatRequestBody>(p->GetPtr());
        uint8_t* startPtr = p->GetPtr() + sizeof(PacketStruct<ChatRequestBody>);
        auto session = p->GetSessionID();
        auto zoneID = stateManager->GetZoneID(session);
        auto zone = stateManager->GetZone(zoneID);
        ChatEvent event;
        event.type = ChatEventType::CHAT;
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
            errorLogger->LogError("non zone handler", "message length invalid", "session", session, "messageLength", body->messageLength);
            Disconnect(session);
            return;
        }
        event.message = std::string(reinterpret_cast<char*>(startPtr), body->messageLength);
        chat->EnqueueChat(event);
    }

    void NonZoneHandler::ZoneChange(IPacketView* p) {
        auto body = parseBody<ZoneChangeBody>(p->GetPtr());
        auto session = p->GetSessionID();
        auto zoneID = stateManager->GetZoneID(session);
        int destZone = 0;
        uint32_t zoneInternalID = INVALID_ZONE_INTERNAL_ID;
        switch (body->op)
        {
            case ZONE_CHANGE::ENTER : {
                if (zoneID != 0) {
                    errorLogger->LogError("non zone handler", "zone enter failed ZoneID != 0", "session", session, "zoneID", zoneID);
					auto p = writer->WriteZoneChangeFailed();
                    if (!p)
                        return;
                    iocp->SendDataUnique(session, std::move(p));
                    return;
                }
                CharacterState temp;
                if (!lobbyZone->EmigrateChar(session, temp)) {
                    errorLogger->LogError("non zone handler", "EmigrateChar from LobbyZone failed", "session", session);
                    auto p = writer->WriteZoneChangeFailed();
                    if (!p)
                        return;
                    iocp->SendDataUnique(session, std::move(p));
                    return;
                }
                if (temp.lastZone == 0)
                    temp.lastZone = 1;

                auto zone = stateManager->GetZone(temp.lastZone);
                zoneInternalID = zone->ImmigrateChar(session, temp);
                if (zoneInternalID == INVALID_ZONE_INTERNAL_ID) {
                    if (!lobbyZone->ImmigrateChar(session, temp)) // 다시 Lobby Zone으로
                        Disconnect(session);

                    auto p = writer->WriteZoneChangeFailed();
                    if (!p)
                        return;
                    iocp->SendDataUnique(session, std::move(p));
                    errorLogger->LogError("non zone handler", "ImmigrateChar Failed", "session", session, "zone", zoneID);
                    return;
                }
                else {
                    uint64_t chatID = chat->AddChatSession(session, temp.lastZone, std::string(temp.charName));
                    chat->EnqueueZoneJoin(session, temp.lastZone);
                    auto p = writer->WriteZoneChangeSucess(temp.lastZone, chatID, zoneInternalID, temp.x, temp.y);
                    if (!p)
                        return;
                    iocp->SendDataUnique(session, std::move(p));
                    return;
                }
                break;
            }
            case ZONE_CHANGE::UP : destZone = zoneID + ZONE_HORIZON; break;
            case ZONE_CHANGE::DOWN : destZone = zoneID - ZONE_HORIZON; break;
            case ZONE_CHANGE::LEFT : destZone = zoneID - 1; break;
            case ZONE_CHANGE::RIGHT : destZone = zoneID + 1; break;
            default: break;
        }
        
        if (destZone <= 0 || destZone > ZONE_COUNT)
        {
            auto p = writer->WriteZoneChangeFailed();
            if (!p)
                return;
            iocp->SendDataUnique(session, std::move(p));
            return;
        }
        auto SZone = stateManager->GetZone(zoneID);
        auto DZone = stateManager->GetZone(destZone);

        CharacterState temp;
        if (!SZone->EmigrateChar(session, temp)) {
            auto p = writer->WriteZoneChangeFailed();
            if (!p)
                return;
            iocp->SendDataUnique(session, std::move(p));
            return;
        }
        zoneInternalID = DZone->ImmigrateChar(session, temp);
        if (zoneInternalID == INVALID_ZONE_INTERNAL_ID) {
            if (!SZone->ImmigrateChar(session, temp)) // 원래 Zone으로 복구
                Disconnect(session);
            auto p = writer->WriteZoneChangeFailed();
            if (!p)
                return;
            iocp->SendDataUnique(session, std::move(p));
            return;
        }
        else {
            auto p = writer->WriteZoneChangeSucess(destZone, 0, zoneInternalID, temp.x, temp.y);
            if (!p)
                return;
            iocp->SendDataUnique(session, std::move(p));

            chat->EnqueueZoneLeave(session, zoneID);
            chat->EnqueueZoneJoin(session, destZone);
        }
    }
}
