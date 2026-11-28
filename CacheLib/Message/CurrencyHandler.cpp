#include "pch.h"
#include "CurrencyHandler.h"
#include "CacheResponseSender.h"

#include <CoreLib/Interface/IMessage.h>
#include <CoreLib/Cache/MessageTypes.h>
#include <CoreLib/LoggerGlobal.h>

#include "../Storage/CacheStorageCurrency.h"
#include <atomic>

namespace Cache {
    void CurrencyHandler::Request(uint64_t sessionID, Core::MsgCurrencyReqBody request) {

        const uint64_t characterID = request.characterID;
        const uint32_t requestID = request.requestID;
        const CurrencyCacheItem res = cache_currency->Getter(characterID);
        const bool available = res.status == CACHE_STATUS::AVAILABLE;
        const Core::MsgCurrencyResBody response{
            .resStatus = static_cast<uint8_t>(available),
            .gold = available ? res.data.gold : 0,
            .requestID = requestID,
            .characterID = characterID
        };
        responses->Send({ sessionID, Core::MSG_CURRENCY_RES }, response);
    }

    void CurrencyHandler::Deposit(uint64_t sessionID, Core::MsgCurrencyDepositBody request) {

        const auto completed = std::make_shared<std::atomic_bool>(false);
        cache_currency->GrantCurrency(request.characterID, request.gold,
            [this, completed, request, sessionID](CACHE_STATUS status, uint64_t gold) {
                if (completed->exchange(true))
                    return;
                const Core::MsgCurrencyDepositResBody response{
                    static_cast<uint8_t>(status == CACHE_STATUS::AVAILABLE), gold, request.characterID
                };
                if (status != CACHE_STATUS::AVAILABLE)
                    Core::errorLogger->LogWarn("cache handler", "currency deposit failed", {{"characterID", request.characterID}, {"status", status}});
                if (const auto result = responses->Send({ sessionID, Core::MSG_CURRENCY_DEPOSIT_RES }, response);
                result == CacheResponseSender::Result::PoolExhausted || result == CacheResponseSender::Result::Rejected)
                    Core::errorLogger->LogError("cache handler", "currency deposit response rejected", {{"characterID", request.characterID}, {"status", status}});
            });
    }
}
