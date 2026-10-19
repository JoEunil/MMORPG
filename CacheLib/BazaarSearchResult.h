#pragma once
#include <cstdint>
#include <limits>
#include <CoreLib/MessageTypes.h>
#include "Config.h"

namespace Cache {
    static_assert(sizeof(Core::MsgStruct<Core::MsgBazaarSearchResBody>) <= MESSAGE_LEN);

    // ResultSet is either the SQL result or a fixture with the same read boundary.
    template<typename ResultSet>
    void ParseBazaarSearchResult(ResultSet* result, uint32_t requestID, Core::MsgBazaarSearchResBody& body) {
        body = {};
        body.requestID = requestID;
        if (result == nullptr)
            return;
        try {
            while (body.count < Core::MAX_BAZAAR_SEARCH_RESULT && result->next()) {
                const uint64_t itemID = result->getUInt64("item_id");
                const int64_t quantity = result->getInt64("quantity");
                if (itemID == 0 || itemID > (std::numeric_limits<uint32_t>::max)()
                    || quantity <= 0 || quantity > (std::numeric_limits<uint16_t>::max)()) {
                    body = {};
                    body.requestID = requestID;
                    return;
                }
                auto& row = body.listings[body.count];
                row.listingID = result->getUInt64("listing_id");
                row.sellerCharacterID = result->getUInt64("seller_id");
                if (row.listingID == 0 || row.sellerCharacterID == 0) {
                    body = {};
                    body.requestID = requestID;
                    return;
                }
                row.itemID = static_cast<uint32_t>(itemID);
                row.quantity = static_cast<uint16_t>(quantity);
                row.price = result->getUInt64("price");
                row.registeredAt = result->getUInt64("listed_at");
                row.status = 0; // query12 selects TRADING only.
                ++body.count;
            }
            body.resStatus = 1; // A valid result with no rows is still successful.
        }
        catch (...) {
            body = {}; // Never return partial rows or pooled-message residue.
            body.requestID = requestID;
        }
    }
}
