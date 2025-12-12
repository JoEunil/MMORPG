#include "Handler.h"

#include <CoreLib/Message.h>
#include <CoreLib/MessageTypes.h>
#include <CoreLib/IMessageQueue.h>
#include <CoreLib/ILogger.h>

#include "DBConnectionPool.h"
#include "MessagePool.h"
#include "DBConnection.h"

namespace Cache {
    void Handler::Process(Core::Message* msg) {
        Core::MsgHeader* header = Core::parseMsgHeader(msg->GetBuffer());
        switch (header->messageType) {
        case Core::MSG_CHARACTER_LIST_REQ :
                CharacterListRequest(msg, header->sessionID, Core::parseMsgBody<Core::MsgCharacterListReqBody>(msg->GetBuffer()));
            break;
        case Core::MSG_CHARACTER_STATE_REQ:
            CharacterStateRequest(msg, header->sessionID, Core::parseMsgBody<Core::MsgCharacterStateReqBody>(msg->GetBuffer()));
            break;
        case Core::MSG_CHARACTER_STATE_UPDATE:
            CharacterStateUpdate(msg, header->sessionID, Core::parseMsgBody<Core::MsgCharacterStateUpdateBody>(msg->GetBuffer()));
            break;
        case Core::MSG_INVENTORY_REQ:
            InventoryRequest(msg, header->sessionID, Core::parseMsgBody<Core::MsgInventoryReqBody>(msg->GetBuffer()));
            break;
        case Core::MSG_INVENTORY_UPDATE:
            InventoryUpdate(msg, header->sessionID, Core::parseMsgBody<Core::MsgInventoryUpdateBody>(msg->GetBuffer()));
            break;
        }
        if (msg != nullptr)
            messagePool->Return(msg);
    }
    void Handler::CharacterListRequest(Core::Message* msg, uint64_t sesionID, Core::MsgCharacterListReqBody* body) {
        DBConnection* conn = connectionPool->Acquire();
        auto res = conn->ExecuteSelect(1, body->userID, body->channelID);
        connectionPool->Return(conn);
        Core::MsgStruct<Core::MsgCharacterListResBody>* st = reinterpret_cast<Core::MsgStruct<Core::MsgCharacterListResBody>*>(msg->GetBuffer());
        
        st->header.sessionID = sesionID;
        st->header.messageType = Core::MSG_CHARACTER_LIST_RES;
        
        if (!res || !res->next()) {
            st->body.resStatus = 0;
            st->body.count = 0;
            messageQ->EnqueueMessage(msg);
            connectionPool->Return(conn);
            return;
        }

        st->body.resStatus = 1;
        st->body.count = 0;

        do {
            if (st->body.count >= MAX_CHARACTER_CNT) break;

            auto& info = st->body.characters[st->body.count];
            info.characterID = res->getUInt64("char_id");
            std::string name = res->getString("name");
            std::memset(info.name, 0, sizeof(info.name));
            std::memcpy(info.name, name.c_str(), std::min(name.size(), sizeof(info.name) - 1));
            info.level = res->getUInt("level");

            st->body.count++;
        } while (res->next());
        
        msg->SetLength(sizeof(Core::MsgStruct<Core::MsgCharacterListResBody>));
        messageQ->EnqueueMessage(msg);
    }

    void Handler::CharacterStateRequest(Core::Message* msg, uint64_t sesionID, Core::MsgCharacterStateReqBody* body) {
        uint64_t characterID = body->characterID;
        DBConnection* conn = connectionPool->Acquire();
        auto res = conn->ExecuteSelect(3, body->characterID);
        connectionPool->Return(conn);

        Core::MsgStruct<Core::MsgCharacterStateResBody>* st = reinterpret_cast<Core::MsgStruct<Core::MsgCharacterStateResBody>*>(msg->GetBuffer());
        
        st->header.sessionID = sesionID;
        st->header.messageType = Core::MSG_CHARACTER_STATE_RES;
        
        if (!res || !res->next()) {
            st->body.resStatus = 0;
            messageQ->EnqueueMessage(msg);
            connectionPool->Return(conn);
            return;
        }
        st->body.charID = res->getUInt64("char_id");
        st->body.resStatus = 1;
        std::string name = res->getString("name");
        std::memset(st->body.name, 0, name.size());
        std::memcpy(st->body.name, name.c_str(), std::min(name.size(), sizeof(st->body.name) - 1));
        st->body.level = res->getUInt("level");
        st->body.exp = res->getInt64("exp");
        st->body.hp = res->getInt64("hp");
        st->body.mp = res->getInt64("mp");
        st->body.dir = res->getInt("dir");
        st->body.currentZone = res->getUInt("zone_id");
        st->body.startX = res->getDouble("last_pos_x");
        st->body.startY = res->getDouble("last_pos_y");
        
        cache_5->Setter(std::move(res));

        msg->SetLength(sizeof(Core::MsgStruct<Core::MsgCharacterStateResBody>));
        messageQ->EnqueueMessage(msg);
    }

    void Handler::CharacterStateUpdate(Core::Message* msg, uint64_t sessionID, Core::MsgCharacterStateUpdateBody* body) {
        DBConnection* conn = connectionPool->Acquire();
        logger->LogInfo(std::format("state update {} {}", body->x, body->y));
        auto res = conn->ExecuteUpdate(4, body->level, body->exp, body->hp, body->mp, body->dir, body->x, body->y, body->lastZone, body->charID);
        connectionPool->Return(conn);
    }

    void Handler::InventoryRequest(Core::Message* msg, uint64_t sesionID, Core::MsgInventoryReqBody* body) {    
        Core::MsgStruct<Core::MsgInventoryResBody>* st = reinterpret_cast<Core::MsgStruct<Core::MsgInventoryResBody>*>(msg->GetBuffer());
        
        if (!cache_5->Getter(msg)) {
            auto conn = connectionPool->Acquire();
            auto res = conn->ExecuteSelect(5, body->characterID);
            connectionPool->Return(conn);

            if (!res) {
                st->body.resStatus = 0;
                messageQ->EnqueueMessage(msg);
                return;
            }
            cache_5->Setter(std::move(res));
            cache_5->Getter(msg);
        }

        msg->SetLength(sizeof(Core::MsgStruct<Core::MsgInventoryResBody>));
        messageQ->EnqueueMessage(msg);
    }

    void Handler::InventoryUpdate(Core::Message* msg, uint64_t sesionID, Core::MsgInventoryUpdateBody* body) {
        auto [resStatus, itemID, slot, quantity] = cache_5->PartialUpdate(msg);

        auto charID = body->characterID;
        Core::MsgStruct<Core::MsgInventoryUpdateResBody>* st = reinterpret_cast<Core::MsgStruct<Core::MsgInventoryUpdateResBody>*>(msg->GetBuffer());

        st->header.sessionID = sesionID;
        st->header.messageType = Core::MSG_INVENTORY_UPDATE_RES;

        st->body.resStatus = resStatus;
        st->body.itemID = itemID;
        st->body.slot = slot;
        st->body.itemQuantity = quantity;
        st->body.characterID = charID;
        msg->SetLength(sizeof(Core::MsgStruct<Core::MsgInventoryUpdateResBody>));
        messageQ->EnqueueMessage(msg);
    }
}
