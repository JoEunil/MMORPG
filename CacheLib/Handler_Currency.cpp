#include "Handler.h"

#include <CoreLib/Message.h>
#include <CoreLib/MessageTypes.h>
#include <CoreLib/IMessageQueue.h>
#include <CoreLib/LoggerGlobal.h>

#include "DBWorker.h"
#include "MessagePool.h"

namespace Cache {
    void Handler::CurrencyRequest(Core::Message*& msg, uint64_t sessionID, Core::MsgCurrencyReqBody* body) {
        Result7 res = cache_currency->Getter(body->characterID);
        if (res.status == CACHE_STATUS::AVAILABLE) {
            auto st = reinterpret_cast<Core::MsgStruct<Core::MsgCurrencyResBody>*>(msg->GetBuffer());
            st->header.messageType = Core::MSG_CURRENCY_RES;
            st->header.sessionID = sessionID;
            st->body.resStatus = 1;
            st->body.gold = res.data.gold;

            msg->SetLength(sizeof(Core::MsgStruct<Core::MsgCurrencyResBody>));
            messageQ->EnqueueMessage(msg);
        } else {
            auto st = reinterpret_cast<Core::MsgStruct<Core::MsgCurrencyResBody>*>(msg->GetBuffer());
            st->header.messageType = Core::MSG_CURRENCY_RES;
            st->header.sessionID = sessionID;
            st->body.resStatus = 0;
            msg->SetLength(sizeof(Core::MsgStruct<Core::MsgCurrencyResBody>));
            messageQ->EnqueueMessage(msg);
        }
    }

    void Handler::CurrencyDeposit(Core::Message*& msg, uint64_t sessionID, Core::MsgCurrencyDepositBody* body) {
        auto charID = body->characterID;
        
        auto [status, gold] = cache_currency->DepositCurrency(body->characterID, body->gold);

        auto st = reinterpret_cast<Core::MsgStruct<Core::MsgCurrencyDepositResBody>*>(msg->GetBuffer());
        st->header.messageType = Core::MSG_CURRENCY_DEPOSIT_RES;
        st->header.sessionID = sessionID;
        st->body.resStatus = (status == CACHE_STATUS::AVAILABLE ? 1 : 0);
        if (status != CACHE_STATUS::AVAILABLE) {
            Core::gameLogger->LogWarn("cache handler", "currency deposit failed", "sessionID", sessionID, "status", status, "gold", gold);
        }
        st->body.gold = gold;

        msg->SetLength(sizeof(Core::MsgStruct<Core::MsgCurrencyDepositResBody>));
        messageQ->EnqueueMessage(msg);
    }
}