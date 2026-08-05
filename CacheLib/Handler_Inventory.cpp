#include "Handler.h"

#include <CoreLib/Message.h>
#include <CoreLib/MessageTypes.h>
#include <CoreLib/IMessageQueue.h>
#include <CoreLib/LoggerGlobal.h>

#include "DBWorker.h"
#include "MessagePool.h"

namespace Cache {
    void Handler::InventoryRequest(Core::Message*& msg, uint64_t sessionID, Core::MsgInventoryReqBody* body) {
        Result5 res = cache_inventory->Getter(body->characterID);
        if (res.status == CACHE_STATUS::AVAILABLE) {
            Core::MsgStruct<Core::MsgInventoryResBody>* st = reinterpret_cast<Core::MsgStruct<Core::MsgInventoryResBody>*>(msg->GetBuffer());
            st->header.messageType = Core::MSG_INVENTORY_RES;
            st->header.sessionID = sessionID;
            st->body.resStatus = 1;
            uint16_t n = 0;
            for (int i = 0; i < MAX_INVENTORY && n < MAX_INVENTORY; i++) {
                if (res.data.items[i].itemID == 0) continue;
                st->body.items[n].itemID = res.data.items[i].itemID;
                st->body.items[n].quantity = res.data.items[i].quantity;
                st->body.items[n].slot = res.data.items[i].slot;
                n++;
            }
            st->body.itemCount = n;
            msg->SetLength(sizeof(Core::MsgStruct<Core::MsgInventoryResBody>));
            messageQ->EnqueueMessage(msg);
        } else {
            Core::MsgStruct<Core::MsgInventoryResBody>* st = reinterpret_cast<Core::MsgStruct<Core::MsgInventoryResBody>*>(msg->GetBuffer());
            st->header.messageType = Core::MSG_INVENTORY_RES;            
            st->header.sessionID = sessionID;
            st->body.resStatus = 0;
            msg->SetLength(sizeof(Core::MsgStruct<Core::MsgInventoryResBody>));
			messageQ->EnqueueMessage(msg);
        }
    }

    void Handler::InventoryUpdate(Core::Message*& msg, uint64_t sessionID, Core::MsgInventoryUpdateBody* body) {
        auto charID = body->characterID;
        auto [status, itemID, slot, quantity] = cache_inventory->PartialUpdate(body->characterID, body->itemID, body->op, body->change);

        auto st = reinterpret_cast<Core::MsgStruct<Core::MsgInventoryUpdateResBody>*>(msg->GetBuffer());
        st->header.messageType = Core::MSG_INVENTORY_UPDATE_RES;
        st->header.sessionID = sessionID;
        st->body.characterID = charID;
        st->body.resStatus = (status == CACHE_STATUS::AVAILABLE ? 1 : 0);
        st->body.itemID = itemID;
        st->body.slot = slot;
        st->body.itemQuantity = quantity;

        msg->SetLength(sizeof(Core::MsgStruct<Core::MsgInventoryUpdateResBody>));
        messageQ->EnqueueMessage(msg);
    }
}