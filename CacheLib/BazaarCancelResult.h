#pragma once
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <CoreLib/MessageTypes.h>

namespace Cache {
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
}
