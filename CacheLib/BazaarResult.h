#pragma once

#include <cstdint>
#include <initializer_list>
#include <limits>
#include <stdexcept>

#include <CoreLib/Cache/MessageTypes.h>

#include "CacheStorage.h"
#include "CacheStorageInventory.h"
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
    void ParseBazaarOutboxListResult(ResultSet* result, const InventoryCacheItem& inventory, Core::MsgBazaarOutboxListResBody& body) {
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

    // The procedure commits cancellation and an outbox event together. Delivery is a separate cache pass.
    template<typename ResultSet, typename Deliver>
    void ParseBazaarCancelResult(ResultSet* result, Deliver&& deliver, Core::MsgBazaarCancelListingResBody& body) {
        body = {};
        body.resStatus = 5;
        if (result == nullptr || !result->next()) throw std::runtime_error("cancel result unavailable");
        for (const auto* column : { "result", "delivery_pending" })
            if (result->isNull(column)) throw std::runtime_error("cancel null result");
        const int64_t status = result->getInt64("result");
        const int64_t pending = result->getInt64("delivery_pending");
        if ((status != 0 && status != 1) || (pending != 0 && pending != 1))
            throw std::runtime_error("cancel invalid result");
        if (status == 1 && pending == 1) {
            for (const auto* column : { "event_id", "item_id", "quantity" })
                if (result->isNull(column)) throw std::runtime_error("cancel delivery null result");
            const uint64_t eventID = result->getUInt64("event_id");
            const uint64_t itemID = result->getUInt64("item_id");
            const uint64_t quantity = result->getUInt64("quantity");
            if (eventID == 0 || itemID == 0 || itemID > (std::numeric_limits<uint32_t>::max)()
                || quantity == 0 || quantity > (std::numeric_limits<uint32_t>::max)())
                throw std::runtime_error("cancel delivery value range");
            deliver(eventID, static_cast<uint32_t>(itemID), static_cast<uint32_t>(quantity));
        }
        body.resStatus = static_cast<uint8_t>(status);
    }

    // Shared SQL/fixture read boundary; DBWorker converts read exceptions to a correlated failure.
    template<typename ResultSet>
    void ParseBazaarClaimResult(ResultSet* result, Core::MsgBazaarClaimResBody& body) {
        body = {};
        if (result == nullptr || !result->next())
            return;
        if (result->isNull("result") || result->isNull("diamond"))
            throw std::runtime_error("claim null result");
        const auto status = result->getInt64("result");
        const auto amount = result->getUInt64("diamond");
        if (status < 0 || status > 4 || (status != 1 && amount != 0))
            throw std::runtime_error("claim invalid result");
        body.resStatus = static_cast<uint8_t>(status);
        body.diamondClaimed = amount;
    }

    // Only a returned zero proves that INSERT failed. An exception may follow COMMIT.
    // Restore is attempted once: a partial restore must never be retried automatically.
    template<typename Insert, typename Restore, typename ReportUnknown>
    uint8_t ResolveBazaarRegister(Insert&& insert, Restore&& restore, ReportUnknown&& reportUnknown) {
        int affected;
        try { affected = insert(); }
        catch (...) {
            reportUnknown();
            return 5;
        }
        if (affected == 1) return 1;
        if (affected == 0) {
            try { if (restore()) return 0; }
            catch (...) {} // A restore may have applied only one resource; do not repeat it.
        }
        reportUnknown();
        return 5;
    }
}
