#pragma once
#include <stdexcept>
#include <CoreLib/MessageTypes.h>

namespace Cache {
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
}
