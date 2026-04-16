#include "Handler.h"

#include <CoreLib/Message.h>
#include <CoreLib/MessageTypes.h>
#include <CoreLib/IMessageQueue.h>
#include <CoreLib/LoggerGlobal.h>

#include "DBWorker.h"
#include "MessagePool.h"

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
    void Handler::CharacterListRequest(Core::Message* msg, uint64_t sessionID, Core::MsgCharacterListReqBody* body) {
        dbWorker->Enqueue([=](DBConnection* conn) {
            auto res = conn->ExecuteSelect(1, body->userID, body->channelID);
            Core::MsgStruct<Core::MsgCharacterListResBody>* st = reinterpret_cast<Core::MsgStruct<Core::MsgCharacterListResBody>*>(msg->GetBuffer());

            st->header.sessionID = sessionID;
            st->header.messageType = Core::MSG_CHARACTER_LIST_RES;

            if (!res || !res->next()) {
                Core::gameLogger->LogInfo("cache handler", "character list read failed", "sessionID", sessionID, "user_id", body->userID, "channel_id", body->channelID);
                st->body.resStatus = 0;
                st->body.count = 0;
                msg->SetLength(sizeof(Core::MsgStruct<Core::MsgCharacterListResBody>));
                messageQ->EnqueueMessage(msg);
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
            });
    }

    void Handler::CharacterStateRequest(Core::Message* msg, uint64_t sessionID, Core::MsgCharacterStateReqBody* body) {
        dbWorker->Enqueue([=](DBConnection* conn) {
            auto res = conn->ExecuteSelect(3, body->characterID);

            Core::MsgStruct<Core::MsgCharacterStateResBody>* st = reinterpret_cast<Core::MsgStruct<Core::MsgCharacterStateResBody>*>(msg->GetBuffer());

            st->header.sessionID = sessionID;
            st->header.messageType = Core::MSG_CHARACTER_STATE_RES;

            if (!res || !res->next()) {
                Core::gameLogger->LogInfo("cache handler", "character state read failed", "sessionID", sessionID, "char_id", body->characterID);
                st->body.resStatus = 0;
                msg->SetLength(sizeof(Core::MsgStruct<Core::MsgCharacterStateResBody>));
                messageQ->EnqueueMessage(msg);
                return;
            }
            st->body.charID = res->getUInt64("char_id");
            st->body.resStatus = 1;
            std::string name = res->getString("name");

            std::memset(st->body.name, 0, sizeof(st->body.name));
            std::memcpy(st->body.name, name.c_str(), std::min(name.size(), sizeof(st->body.name) - 1));
            st->body.name[sizeof(st->body.name) - 1] = '\0';

            st->body.attack = res->getUInt("attack");
            st->body.level = res->getUInt("level");
            st->body.exp = res->getInt64("exp");
            st->body.hp = res->getInt("hp");
            st->body.mp = res->getInt("mp");
            st->body.maxHp = res->getInt("max_hp");
            st->body.maxMp = res->getInt("max_mp");
            st->body.dir = res->getInt("dir");
            st->body.currentZone = res->getUInt("zone_id");
            st->body.startX = res->getDouble("last_pos_x");
            st->body.startY = res->getDouble("last_pos_y");

            msg->SetLength(sizeof(Core::MsgStruct<Core::MsgCharacterStateResBody>));
            messageQ->EnqueueMessage(msg);
        });
    }

    void Handler::CharacterStateUpdate(Core::Message* msg, uint64_t sessionID, Core::MsgCharacterStateUpdateBody* body) {
        dbWorker->Enqueue([=](DBConnection* conn) {
            auto res = conn->ExecuteUpdate(4, body->attack, body->level, body->exp, body->hp, body->mp, body->maxHp, body->maxMp, body->dir, body->x, body->y, body->lastZone, body->charID);

            if (!res) {
                Core::gameLogger->LogInfo("cache handler", "character state update failed", "sessionID", sessionID, "char_id", body->charID, "level", body->level,
                    "exp", body->exp, "hp", body->hp, "mp", body->mp, "max_hp", body->maxHp, "max_mp", body->maxMp, "dir", body->dir, "x", body->x, "y", body->y, "last_zone", body->lastZone);
            }
        });
    }

    void Handler::InventoryRequest(Core::Message* msg, uint64_t sessionID, Core::MsgInventoryReqBody* body) {    
        cache_5->Getter(msg);
        messageQ->EnqueueMessage(msg);
    }

    void Handler::InventoryUpdate(Core::Message* msg, uint64_t sessionID, Core::MsgInventoryUpdateBody* body) {
        auto charID = body->characterID;
        auto [status,itemID,  slot, quantity] = cache_5->PartialUpdate(msg);

        auto st = reinterpret_cast<Core::MsgStruct<Core::MsgInventoryUpdateResBody>*>(msg->GetBuffer());
        st->header.messageType = Core::MSG_INVENTORY_RES;
        st->body.characterID = charID;
        st->body.resStatus = (status == CACHE_STATUS::AVAILABLE ? 1 : 0);
        st->body.itemID = itemID;
        st->body.slot = slot;
        st->body.itemQuantity = quantity;
        
        messageQ->EnqueueMessage(msg);
    }
}
