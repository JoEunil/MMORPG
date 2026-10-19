#pragma once
#include <cstdint>
#include <limits>
#include <initializer_list>
#include <stdexcept>
#include <CoreLib/MessageTypes.h>
#include "Config.h"

namespace Cache {
    static_assert(sizeof(Core::MsgStruct<Core::MsgBazaarMyListResBody>) <= MESSAGE_LEN);

    // SQL and fixtures share this external read boundary.
    template<typename ResultSet>
    void ParseBazaarMyListResult(ResultSet* result, uint32_t requestID, uint64_t characterID,
        Core::MsgBazaarMyListResBody& body) {
        body = {};
        body.requestID = requestID;
        body.characterID = characterID;
        if (result == nullptr || characterID == 0)
            return;
        try {
            while (result->next()) {
                if (body.count >= Core::MAX_BAZAAR_MY_LIST)
                    throw std::runtime_error("my list row limit");
                for (const auto* column : { "listing_id", "seller_id", "item_id", "quantity", "price", "listed_at", "status" })
                    if (result->isNull(column))
                        throw std::runtime_error("my list null column");
                const uint64_t itemID = result->getUInt64("item_id");
                const int64_t quantity = result->getInt64("quantity");
                const int64_t status = result->getInt64("status");
                if (itemID == 0 || itemID > (std::numeric_limits<uint32_t>::max)()
                    || quantity <= 0 || quantity > (std::numeric_limits<uint16_t>::max)()
                    || status < 0 || status > 2)
                    throw std::runtime_error("my list value range");
                auto& row = body.listings[body.count];
                row.listingID = result->getUInt64("listing_id");
                row.sellerCharacterID = result->getUInt64("seller_id");
                if (row.listingID == 0 || row.sellerCharacterID != characterID)
                    throw std::runtime_error("my list owner or listing");
                row.itemID = static_cast<uint32_t>(itemID);
                row.quantity = static_cast<uint16_t>(quantity);
                row.status = static_cast<uint8_t>(status);
                row.price = result->getUInt64("price");
                row.registeredAt = result->getUInt64("listed_at");
                ++body.count;
            }
            body.resStatus = 1;
        }
        catch (...) {
            body = {};
            body.requestID = requestID;
            body.characterID = characterID;
        }
    }
}
