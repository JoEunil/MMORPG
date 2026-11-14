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
        Core::MsgInventoryResBody response{};
        response.resStatus = Core::RES_STATUS::FAILED;
        if (res.status == CACHE_STATUS::AVAILABLE) {
            response.resStatus = 1;
            uint16_t n = 0;
            for (int i = 0; i < MAX_INVENTORY && n < MAX_INVENTORY; i++) {
                if (res.data.items[i].itemID == 0) continue;
                response.items[n].itemID = res.data.items[i].itemID;
                response.items[n].quantity = res.data.items[i].quantity;
                response.items[n].slot = res.data.items[i].slot;
                n++;
            }
            response.itemCount = n;
        }
        TryEnqueueResponse(*msg, { sessionID, Core::MSG_INVENTORY_RES }, response);
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
