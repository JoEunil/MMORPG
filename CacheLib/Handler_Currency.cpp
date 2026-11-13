#include "Handler.h"

#include <CoreLib/Message.h>
#include <CoreLib/MessageTypes.h>
#include <CoreLib/IMessageQueue.h>
#include <CoreLib/LoggerGlobal.h>

#include "DBWorker.h"

namespace Cache {
    void Handler::CurrencyRequest(ScopedMessage& msg, uint64_t sessionID, Core::MsgCurrencyReqBody* body) {
        if (!CheckMessageLength<Core::MsgCurrencyReqBody>(msg.get()))
            return;

        const uint64_t characterID = body->characterID;
        const uint32_t requestID = body->requestID;
        const Result7 res = cache_currency->Getter(characterID);
        const bool available = res.status == CACHE_STATUS::AVAILABLE;
        const Core::MsgCurrencyResBody response{
            .resStatus = static_cast<uint8_t>(available),
            .gold = available ? res.data.gold : 0,
            .requestID = requestID,
            .characterID = characterID
        };
        TryEnqueueResponse(*msg, { sessionID, Core::MSG_CURRENCY_RES }, response);
    }

    void Handler::CurrencyDeposit(ScopedMessage& msg, uint64_t sessionID, Core::MsgCurrencyDepositBody* body) {
        if (!CheckMessageLength<Core::MsgCurrencyDepositBody>(msg.get()))
            return;

        const auto request = *body;
        auto pending = std::make_shared<ScopedMessage>(std::move(msg));
        cache_currency->GrantCurrency(request.characterID, request.gold,
            [this, pending, request, sessionID](CACHE_STATUS status, uint64_t gold) {
                auto owned = std::move(*pending);
                auto* st = reinterpret_cast<Core::MsgStruct<Core::MsgCurrencyDepositResBody>*>(owned->GetBuffer());
                st->header = {sessionID, Core::MSG_CURRENCY_DEPOSIT_RES};
                st->body = {static_cast<uint8_t>(status == CACHE_STATUS::AVAILABLE ? 1 : 0), gold, request.characterID};
                if (status != CACHE_STATUS::AVAILABLE)
                    Core::errorLogger->LogWarn("cache handler", "currency deposit failed", "characterID", request.characterID, "status", status);
                owned->SetLength(sizeof(*st));
                if (!messageQ->EnqueueMessage(owned.get()))
                    Core::errorLogger->LogError("cache handler", "currency deposit response rejected", "characterID", request.characterID, "status", status);
            });
    }
}
