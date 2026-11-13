#include "Handler.h"

#include <CoreLib/Message.h>
#include <CoreLib/MessageTypes.h>
#include <CoreLib/IMessageQueue.h>
#include <CoreLib/LoggerGlobal.h>

#include "DBWorker.h"

namespace Cache {
    void Handler::CurrencyRequest(Core::Message*& msg, uint64_t sessionID, Core::MsgCurrencyReqBody* body) {
        if (!CheckMessageLength<Core::MsgCurrencyReqBody>(msg))
            return;

        const uint64_t characterID = body->characterID;
        const uint32_t requestID = body->requestID;
        Result7 res = cache_currency->Getter(characterID);
        auto* st = reinterpret_cast<Core::MsgStruct<Core::MsgCurrencyResBody>*>(msg->GetBuffer());
        st->header.messageType = Core::MSG_CURRENCY_RES;
        st->header.sessionID = sessionID;
        st->body = {};
        st->body.resStatus = (res.status == CACHE_STATUS::AVAILABLE ? 1 : 0);
        st->body.gold = st->body.resStatus == 1 ? res.data.gold : 0;
        st->body.requestID = requestID;
        st->body.characterID = characterID;
        msg->SetLength(sizeof(Core::MsgStruct<Core::MsgCurrencyResBody>));
        messageQ->EnqueueMessage(msg);
    }

    void Handler::CurrencyDeposit(Core::Message*& msg, uint64_t sessionID, Core::MsgCurrencyDepositBody* body) {
        if (!CheckMessageLength<Core::MsgCurrencyDepositBody>(msg))
            return;

        const auto request = *body;
        auto* owned = msg;
        msg = nullptr; // Cache loading owns this message until its one completion callback.
        cache_currency->GrantCurrency(request.characterID, request.gold,
            [this, owned, request, sessionID](CACHE_STATUS status, uint64_t gold) {
                auto* st = reinterpret_cast<Core::MsgStruct<Core::MsgCurrencyDepositResBody>*>(owned->GetBuffer());
                st->header = {sessionID, Core::MSG_CURRENCY_DEPOSIT_RES};
                st->body = {static_cast<uint8_t>(status == CACHE_STATUS::AVAILABLE ? 1 : 0), gold, request.characterID};
                if (status != CACHE_STATUS::AVAILABLE)
                    Core::errorLogger->LogWarn("cache handler", "currency deposit failed", "characterID", request.characterID, "status", status);
                owned->SetLength(sizeof(*st));
                if (!messageQ->EnqueueMessage(owned))
                    Core::errorLogger->LogError("cache handler", "currency deposit response rejected", "characterID", request.characterID, "status", status);
                messagePool->Return(owned);
            });
    }
}
