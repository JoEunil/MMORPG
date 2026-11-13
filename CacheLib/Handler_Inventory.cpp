#include "Handler.h"

#include <CoreLib/Message.h>
#include <CoreLib/MessageTypes.h>
#include <CoreLib/IMessageQueue.h>
#include <CoreLib/LoggerGlobal.h>

#include "DBWorker.h"

namespace Cache {
    void Handler::InventoryRequest(ScopedMessage& msg, uint64_t sessionID, Core::MsgInventoryReqBody* body) {
        if (!CheckMessageLength<Core::MsgInventoryReqBody>(msg.get()))
            return;

        const auto characterID = body->characterID;
        Result5 res = cache_inventory->Getter(characterID);
        Core::MsgStruct<Core::MsgInventoryResBody>* st = reinterpret_cast<Core::MsgStruct<Core::MsgInventoryResBody>*>(msg->GetBuffer());
        st->body = {};
        st->header.messageType = Core::MSG_INVENTORY_RES;
        st->header.sessionID = sessionID;
        if (res.status == CACHE_STATUS::AVAILABLE) {
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
            messageQ->EnqueueMessage(msg.get());
        } else {
            st->body.resStatus = Core::RES_STATUS::FAILED;
            msg->SetLength(sizeof(Core::MsgStruct<Core::MsgInventoryResBody>));
			messageQ->EnqueueMessage(msg.get());
        }
    }

    void Handler::InventoryUpdate(ScopedMessage& msg, uint64_t sessionID, Core::MsgInventoryUpdateBody* body) {
        if (!CheckMessageLength<Core::MsgInventoryUpdateBody>(msg.get()))
            return;

        const auto characterID = body->characterID;
        const auto [status, itemID, slot, quantity] = cache_inventory->PartialUpdate(characterID, body->itemID, body->op, body->change);
        const Core::MsgInventoryUpdateResBody response{
            .resStatus = static_cast<uint8_t>(status == CACHE_STATUS::AVAILABLE),
            .characterID = characterID,
            .itemID = itemID,
            .itemQuantity = quantity,
            .slot = slot
        };
        TryEnqueueResponse(*msg, { sessionID, Core::MSG_INVENTORY_UPDATE_RES }, response);
    }
}
