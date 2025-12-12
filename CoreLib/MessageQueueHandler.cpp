#include "pch.h"
#include "MessageQueueHandler.h"
#include "Message.h"
#include "MessageTypes.h"
#include "MessagePool.h"
#include "ILogger.h"
#include "IIOCP.h"
#include "PacketWriter.h"
#include "LobbyZone.h"
#include "Config.h"

namespace Core {
    void MessageQueueHandler::Process(Message* msg) {
        if (msg == nullptr) {
            logger->LogError("MessageQueueHandler: Invlid Message");
            return;
        }
        MsgHeader* header = parseMsgHeader(msg->GetBuffer());
        switch (header->messageType) {
        case MSG_CHARACTER_LIST_RES: 
            CharacterListResponse(header->sessionID, parseMsgBody<MsgCharacterListResBody>(msg->GetBuffer()));
            break;
        case MSG_CHARACTER_STATE_RES:
            CharacterStateResponse(header->sessionID, parseMsgBody<MsgCharacterStateResBody>(msg->GetBuffer()));
            break;
        case MSG_INVENTORY_RES:
            InventoryResponse(header->sessionID, parseMsgBody<MsgInventoryResBody>(msg->GetBuffer()));
            break;
        case MSG_INVENTORY_UPDATE_RES:
            InventoryUpdateResponse(header->sessionID, parseMsgBody<MsgInventoryUpdateResBody>(msg->GetBuffer()));
            break;
        default:
            logger->LogError(std::format("MessageQueueHandler: undefined message type {}", header->messageType));
        }
        messagePool->Return(msg);
    }

    void MessageQueueHandler::CharacterListResponse(uint64_t sessionID, MsgCharacterListResBody* body) {
        logger->LogError("CharacterList received");
        iocp->SendData(sessionID, writer->WriteCharacterListResponse(body));
    }

    void MessageQueueHandler::CharacterStateResponse(uint64_t sessionID, MsgCharacterStateResBody* body) {
        //client에서는 full snapshot 받을 때 까지 로딩 화면
        CharacterState temp;
        if (body->resStatus != 0) {
            temp.level = body->level;
            std::memcpy(temp.charName, body->name, MAX_CHARNAME_LEN);
            temp.characterID = body->charID;
            temp.dir = body->dir;
            temp.exp = body->exp;
            temp.hp = body ->hp;
            temp.mp = body ->mp;
            temp.lastZone = body->currentZone;
            temp.sessionID = sessionID;
            temp.x = body->startX;
            temp.y = body->startY;
            if (!lobbyZone->ImmigrateChar(sessionID, temp)) {
                logger->LogError("Immigrate to LobbyZone Failed");
            }
        }


        iocp->SendData(sessionID, writer->WriteEnterWorldResponse(body));
    }

    void MessageQueueHandler::InventoryResponse(uint64_t sessionID, MsgInventoryResBody* body) {
        iocp->SendData(sessionID, writer->WriteInventoryResponse(body));
    }

    void MessageQueueHandler::InventoryUpdateResponse(uint64_t sessionID, MsgInventoryUpdateResBody* body) {
        iocp->SendData(sessionID, writer->WriteInventoryUpdateResponse(body));
    }
}
