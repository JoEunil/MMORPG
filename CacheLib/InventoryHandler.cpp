#include "pch.h"
#include "InventoryHandler.h"
#include "CacheResponseSender.h"

#include <CoreLib/Interface/IMessage.h>
#include <CoreLib/Cache/MessageTypes.h>
#include <CoreLib/LoggerGlobal.h>

#include "CacheStorageInventory.h"

namespace Cache {
    void InventoryHandler::Request(uint64_t sessionID, Core::MsgInventoryReqBody request) {

        const auto characterID = request.characterID;
        InventoryCacheItem res = cache_inventory->Getter(characterID);
        Core::MsgInventoryResBody response{};
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
        responses->Send({ sessionID, Core::MSG_INVENTORY_RES }, response);
    }

    void InventoryHandler::Update(uint64_t sessionID, Core::MsgInventoryUpdateBody request) {

        const auto characterID = request.characterID;
        const auto [status, itemID, slot, quantity] = cache_inventory->PartialUpdate(characterID, request.itemID, request.op, request.change);
        const Core::MsgInventoryUpdateResBody response{
            .resStatus = static_cast<uint8_t>(status == CACHE_STATUS::AVAILABLE),
            .characterID = characterID,
            .itemID = itemID,
            .itemQuantity = quantity,
            .slot = slot
        };
        responses->Send({ sessionID, Core::MSG_INVENTORY_UPDATE_RES }, response);
    }
}
