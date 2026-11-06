#pragma once
#include <limits>
#include <initializer_list>
#include <stdexcept>
#include <CoreLib/MessageTypes.h>
#include "CacheStorage.h"
#include "CacheStorageInventory.h"

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

    // Read-only snapshot: READY may already be applied to the inventory but not flushed to CLAIMED.
    template<typename ResultSet>
    void ParseBazaarOutboxListResult(ResultSet* result, const Result5& inventory, Core::MsgBazaarOutboxListResBody& body) {
        body = {};
        if (result == nullptr) return;
        if (inventory.status != CACHE_STATUS::AVAILABLE) {
            body.resStatus = 2;
            return;
        }
        Core::MsgBazaarOutboxListResBody snapshot{};
        uint64_t seen[30]{};
        uint16_t seenCount = 0;
        try {
            while (result->next()) {
                if (seenCount == 30) throw std::runtime_error("outbox list row limit");
                for (const auto* column : { "event_id", "listing_id", "item_id", "quantity" })
                    if (result->isNull(column)) throw std::runtime_error("outbox list null result");
                const uint64_t eventID = result->getUInt64("event_id");
                const uint64_t listingID = result->getUInt64("listing_id");
                const uint64_t itemID = result->getUInt64("item_id");
                const uint64_t quantity = result->getUInt64("quantity");
                if (eventID == 0 || listingID == 0 || itemID == 0 || itemID > UINT32_MAX || quantity == 0 || quantity > UINT16_MAX)
                    throw std::runtime_error("outbox list value range");
                for (uint16_t i = 0; i < seenCount; ++i)
                    if (seen[i] == eventID) throw std::runtime_error("outbox list duplicate event");
                seen[seenCount++] = eventID;
                bool applied = false;
                for (const uint64_t recent : inventory.data.recentEventIds)
                    if (recent == eventID) { applied = true; break; }
                if (applied) ++snapshot.processingCount;
                else snapshot.rows[snapshot.count++] = { eventID, listingID, static_cast<uint32_t>(itemID), static_cast<uint32_t>(quantity) };
            }
            snapshot.resStatus = 1;
            body = snapshot;
        }
        catch (...) { body = {}; } // A partial list must never be presented as a successful empty snapshot.
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
