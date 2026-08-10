#include "pch.h"
#include "MessageQueueHandler.h"
#include "Message.h"
#include "MessageTypes.h"
#include "MessagePool.h"
#include "IIOCP.h"
#include "PacketWriter.h"
#include "LobbyZone.h"
#include "StateManager.h"
#include "LoggerGlobal.h"
#include "Config.h"

namespace Core {
    void MessageQueueHandler::Process(Message* msg) {
        if (msg == nullptr) {
            errorLogger->LogError("mq handler", "Invlid Message");
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
        case MSG_PROFILE_RENAME_RES:
            ProfileRenameResponse(header->sessionID, parseMsgBody<MsgProfileRenameResBody>(msg->GetBuffer()));
            break;
        default:
            errorLogger->LogError("mq handler", "invalid message type");
        }
        messagePool->Return(msg);
    }

    void MessageQueueHandler::CharacterListResponse(uint64_t sessionID, MsgCharacterListResBody* body) {
        auto p = writer->WriteCharacterListResponse(body);
        if (!p) {
            return;
        }
        iocp->SendDataUnique(sessionID, std::move(p));
    }

    void MessageQueueHandler::CharacterStateResponse(uint64_t sessionID, MsgCharacterStateResBody* body) {
        //client에서는 full snapshot 받을 때 까지 로딩 화면
        CharacterState temp;
        if (body->resStatus != 0) {
            // 이름은 zone이 들고 있지 않는다. 자기 이름은 EnterWorld 응답으로만 내려간다.
            temp.profileId = body->profileId;
            temp.profileVersion = body->profileVersion;
            temp.characterID = body->charID;
            temp.dir = body->dir;
            temp.attack = body->attack;
            temp.level = body->level;
            temp.exp = body->exp;
            temp.hp = body ->hp;
            temp.mp = body ->mp;
            temp.maxHp = body->maxHp;
            temp.maxMp = body->maxMp;
            temp.lastZone = body->currentZone;
            temp.sessionID = sessionID;
            temp.x = body->startX;
            temp.y = body->startY;
            if (!lobbyZone->ImmigrateChar(sessionID, temp)) {
                errorLogger->LogError("mq handler", "Failed to enter lobby");
                return;
            }
        }


        auto p = writer->WriteEnterWorldResponse(body);
        if (!p) {
            return;
        }
        iocp->SendDataUnique(sessionID, std::move(p));
    }

    void MessageQueueHandler::InventoryResponse(uint64_t sessionID, MsgInventoryResBody* body) {
        auto p = writer->WriteInventoryResponse(body);
        if (!p) {
            return;
        }
        iocp->SendDataUnique(sessionID, std::move(p));
    }

    void MessageQueueHandler::InventoryUpdateResponse(uint64_t sessionID, MsgInventoryUpdateResBody* body) {
        auto p = writer->WriteInventoryUpdateResponse(body);
        if (!p) {
            return;
        }
        iocp->SendDataUnique(sessionID, std::move(p));
    }

    // rename이 DB에 확정된 뒤 도는 경로. zone이 들고 있는 profileVersion 사본을
    // 여기서 갱신해야 delta로 전파된다.
    void MessageQueueHandler::ProfileRenameResponse(uint64_t sessionID, MsgProfileRenameResBody* body) {
        if (body->resStatus == 0) {
            gameLogger->LogInfo("mq handler", "profile rename failed", "sessionID", sessionID, "profile_id", body->profileId);
            return;
        }
        if (stateManager == nullptr) {
            errorLogger->LogError("mq handler", "stateManager not initialized", "sessionID", sessionID);
            return;
        }
        stateManager->UpdateProfileVersion(sessionID, body->version);
    }
}
