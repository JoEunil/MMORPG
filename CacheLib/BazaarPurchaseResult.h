#pragma once
#include <limits>
#include <initializer_list>
#include <stdexcept>
#include <CoreLib/MessageTypes.h>
#include "CacheStorage.h"

namespace Cache {
    // SQL/fixture boundary. A missing or unreadable result cannot prove that COMMIT did not happen.
    template<typename ResultSet>
    void ParseBazaarBuyResult(ResultSet* result, Core::MsgBazaarBuyResBody& body) {
        body = {};
        body.resStatus = 5;
        if (result == nullptr || !result->next()) return;
        for (const auto* column : { "result", "item_id", "quantity", "price" })
            if (result->isNull(column)) throw std::runtime_error("buy null result");
        const int64_t status = result->getInt64("result");
        const uint64_t itemID = result->getUInt64("item_id");
        const uint64_t quantity = result->getUInt64("quantity");
        const uint64_t price = result->getUInt64("price");
        if ((status != 0 && status != 1 && status != 3 && status != 4)
            || itemID > (std::numeric_limits<uint32_t>::max)()
            || quantity > (std::numeric_limits<uint32_t>::max)()
            || (status == 1 && (itemID == 0 || quantity == 0))
            || (status != 1 && (itemID != 0 || quantity != 0 || price != 0)))
            throw std::runtime_error("buy invalid result");
        body.resStatus = static_cast<uint8_t>(status);
        body.itemID = static_cast<uint32_t>(itemID);
        body.quantity = static_cast<uint32_t>(quantity);
        body.diamondSpent = price;
    }

    // One bounded SQL pass applies through the existing inventory method. Keep a completed prefix on read failure.
    template<typename ResultSet, typename Deliver>
    void DeliverBazaarOutboxResult(ResultSet* result, Deliver&& deliver, Core::MsgBazaarCheckOutboxResBody& body) {
        body = {};
        if (result == nullptr) return;
        body.resStatus = 1;
        try {
            while (result->next()) {
                if (body.deliveredCount + body.duplicatedCount + body.blockedCount >= 30)
                    throw std::runtime_error("outbox row limit");
                for (const auto* column : { "event_id", "item_id", "quantity" })
                    if (result->isNull(column)) throw std::runtime_error("outbox null result");
                const uint64_t eventID = result->getUInt64("event_id");
                const uint64_t itemID = result->getUInt64("item_id");
                const uint64_t quantity = result->getUInt64("quantity");
                if (itemID > (std::numeric_limits<uint32_t>::max)()
                    || quantity > (std::numeric_limits<uint32_t>::max)())
                    throw std::runtime_error("outbox value range");
                switch (deliver(eventID, static_cast<uint32_t>(itemID), static_cast<uint32_t>(quantity))) {
                case CACHE_STATUS::AVAILABLE: ++body.deliveredCount; break;
                case CACHE_STATUS::DUPLICATED: ++body.duplicatedCount; break;
                case CACHE_STATUS::BLOCKED: ++body.blockedCount; return;
                default: body.resStatus = 0; return;
                }
            }
        }
        catch (...) { body.resStatus = 0; } // Prior cache effects are not rolled back or presented as durable.
    }
}
