#include "pch.h"
#include "Handler.h"
#include "CacheResponseSender.h"

#include <CoreLib/Interface/IMessage.h>
#include <CoreLib/Cache/MessageTypes.h>
#include <CoreLib/LoggerGlobal.h>

#include "DB/DBWorker.h"
#include <atomic>

namespace Cache {
    void Handler::CurrencyRequest(ScopedMessage& msg, uint64_t sessionID, Core::MsgCurrencyReqBody* body) {
        if (!CheckMessageLength<Core::MsgCurrencyReqBody>(msg.get()))
            return;

        const uint64_t characterID = body->characterID;
        const uint32_t requestID = body->requestID;
        msg.reset();
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

    void Handler::CurrencyDeposit(ScopedMessage& msg, uint64_t sessionID, Core::MsgCurrencyDepositBody* body) {
        if (!CheckMessageLength<Core::MsgCurrencyDepositBody>(msg.get()))
            return;

        const auto request = *body;
        msg.reset();
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
                if (responses->Send({ sessionID, Core::MSG_CURRENCY_DEPOSIT_RES }, response) != CacheResponseSender::Result::Queued)
                    Core::errorLogger->LogError("cache handler", "currency deposit response rejected", {{"characterID", request.characterID}, {"status", status}});
            });
    }
}
