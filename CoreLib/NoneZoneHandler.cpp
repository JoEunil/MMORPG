#include "pch.h"
#include "NoneZoneHandler.h"

#include "PacketTypes.h"
#include "IPacketView.h"
#include "ILogger.h"
#include "ISessionAuth.h"
#include "IIOCP.h"
#include "PacketWriter.h"
#include "ZoneState.h"
#include "Message.h"
#include "MessagePool.h"
#include "IMessageQueue.h"
#include "StateManager.h"
#include "LobbyZone.h"
#include "Config.h"

namespace Core {
    static IIOCP* iocp;
    static PacketWriter* writer;
    static ILogger* logger;
    static StateManager* stateManager;
    // 콜백에서 쓰기 위함

    void NoneZoneHandler::Initialize(IIOCP* i, ILogger* l, ISessionAuth* s, PacketWriter* p, MessagePool* m, IMessageQueue* mq, StateManager* manager, LobbyZone* lobby) {
        iocp = i;
        logger = l;
        auth = s;
        writer = p;
        messagePool = m;
        messageQueue = mq;
        stateManager = manager;
        lobbyZone = lobby;
    }

    bool NoneZoneHandler::IsReady() {
        if (iocp == nullptr) return false;
        if (logger == nullptr) return false;
        if (auth == nullptr) return false;
        if (writer == nullptr) return false;
        if (messagePool == nullptr) return false;
        if (messageQueue == nullptr) return false;
        if (stateManager == nullptr) return false;
        if (lobbyZone == nullptr) return false;
        
        return true;
    }

    void NoneZoneHandler::Process(IPacketView* p) {
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
                logger->LogInfo(std::format("Async handler UnDefined OPCODE {} {}, session {}, flag {}, magic {}", p->GetOpcode(), h->opcode, p->GetSessionID(), h->flags, h->magic));
                break;
        }
        
    }

    void NoneZoneHandler::Disconnect(uint64_t sessionID) {
        stateManager->Disconnect(sessionID);
    }

    static void ResponseSession(uint64_t sessionID, uint8_t resStatus, uint64_t userID) {
        stateManager->AddSession(sessionID, userID, resStatus == RES_STATUS::SUCCESS);
        iocp->SendData(sessionID, writer->WriteAuthResponse(resStatus));
    }

    void NoneZoneHandler::CheckSession(IPacketView* p) {
        AuthRequestBody* body = parseBody<AuthRequestBody>(p->GetPtr());
        SessionCallbackData* privdata = new SessionCallbackData;
        privdata->sessionID = p->GetSessionID();
        privdata->userID = body->userID;
        std::memcpy(privdata->sessionToken.data(), body->sessionToken, privdata->sessionToken.size());
        privdata->callbackFunc = &ResponseSession;
        auth->CheckSession(privdata);
    }

    void NoneZoneHandler::GetCharacterList(IPacketView* p) {
        Message* msg = messagePool->Acquire();
        MsgStruct<MsgCharacterListReqBody>* st = reinterpret_cast<MsgStruct<MsgCharacterListReqBody>*>(msg->GetBuffer());
        
        st->header.sessionID = p->GetSessionID();
        st->header.messageType = MSG_CHARACTER_LIST_REQ;
        st->body.channelID = CHANNEL_ID;
        st->body.userID = stateManager->GetUserID(p->GetSessionID());
        msg->SetLength(sizeof(MsgStruct<MsgCharacterListReqBody>));
        messageQueue->EnqueueMessage(msg);
        messagePool->Return(msg);
    }

    void NoneZoneHandler::GetCharacterState(IPacketView* p) {
        Message* msg = messagePool->Acquire();
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

    void NoneZoneHandler::GetInventory(IPacketView* p) {
        auto charID = stateManager->GetCharacterID(p->GetSessionID());
        if (charID <= 0)
            return ;
        
        Message* msg = messagePool->Acquire();
        MsgStruct<MsgInventoryReqBody>* st = reinterpret_cast<MsgStruct<MsgInventoryReqBody>*>(msg->GetBuffer());
        
        st->header.sessionID = p->GetSessionID();
        st->header.messageType = MSG_INVENTORY_REQ;
        st->body.characterID = charID;
        msg->SetLength(sizeof(MsgStruct<MsgInventoryReqBody>));
        messageQueue->EnqueueMessage(msg);
        messagePool->Return(msg);
    }

    void NoneZoneHandler::Chat(IPacketView* p) {
        auto body = parseBody<ChatRequestBody>(p->GetPtr());
        uint8_t* startPtr = p->GetPtr() + sizeof(PacketStruct<ChatRequestBody>);
        auto session = p->GetSessionID();
        auto zoneID = stateManager->GetZoneID(session);
        auto zone = stateManager->GetZone(zoneID);
        ChatMessage msg;
        msg.senderSessionID = session;
        msg.message = std::string(reinterpret_cast<char*>(startPtr), body->messageLength);
        zone->EnqueueChat(msg);
    }

    void NoneZoneHandler::ZoneChange(IPacketView* p) {
        auto body = parseBody<ZoneChangeBody>(p->GetPtr());
        auto session = p->GetSessionID();
        auto zoneID = stateManager->GetZoneID(session);
        int destZone = 0;
        uint64_t zoneInternalID = 0;
        switch (body->op)
        {
            case ZONE_CHANGE::ENTER : {
                if (zoneID != 0) {
                    logger->LogError("ZoneID != 0");
                    iocp->SendData(session, writer->WriteZoneChangeFailed());
                    return;
                }
                CharacterState temp;
                if (!lobbyZone->EmigrateChar(session, temp)) {
                    logger->LogError("EmigrateChar Failed");
                    iocp->SendData(session, writer->WriteZoneChangeFailed());
                    return;
                }
                if (temp.lastZone == 0)
                    temp.lastZone = 1;

                auto zone = stateManager->GetZone(temp.lastZone);
                zoneInternalID = zone->ImmigrateChar(session, temp);
                if (zoneInternalID == 0) {
                    if (!lobbyZone->ImmigrateChar(session, temp)) // 다시 Lobby Zone으로
                        Disconnect(session);
                    iocp->SendData(session, writer->WriteZoneChangeFailed());
                    logger->LogError("ImmigrateChar Failed");
                }
                else 
                    iocp->SendData(session, writer->WriteZoneChangeSucess(temp.lastZone, zoneInternalID, temp.x, temp.y));
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
            iocp->SendData(session, writer->WriteZoneChangeFailed());
            return;
        }
        auto SZone = stateManager->GetZone(zoneID);
        auto DZone = stateManager->GetZone(destZone);

        CharacterState temp;
        if (!SZone->EmigrateChar(session, temp)) {
            iocp->SendData(session, writer->WriteZoneChangeFailed());
            return;
        }
        zoneInternalID = DZone->ImmigrateChar(session, temp);
        if (zoneInternalID == 0) {
            if (SZone->ImmigrateChar(session, temp)) // 원래 Zone으로 복구
                Disconnect(session);
            iocp->SendData(session, writer->WriteZoneChangeFailed());
            return;
        }
        else
            iocp->SendData(session, writer->WriteZoneChangeSucess(destZone, zoneInternalID, temp.x, temp.y));
    }
}
