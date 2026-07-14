#include "Handler.h"

#include <CoreLib/Message.h>
#include <CoreLib/MessageTypes.h>
#include <CoreLib/IMessageQueue.h>
#include <CoreLib/LoggerGlobal.h>

#include "DBWorker.h"
#include "MessagePool.h"

namespace Cache {
    void Handler::DiamondRequest(Core::Message*& msg, uint64_t sessionID, Core::MsgDiamondReqBody* body) {
        dbWorkerBazaar->Enqueue([=](DBConnectionBazaar* conn) {
            auto res = conn->ExecuteSelect(9, body->characterID);
            Core::MsgStruct<Core::MsgDiamondResBody>* st = reinterpret_cast<Core::MsgStruct<Core::MsgDiamondResBody>*>(msg->GetBuffer());

            st->header.sessionID = sessionID;
            st->header.messageType = Core::MSG_DIAMOND_RES;

            if (!res || !res->next()) {
                st->body.resStatus = 0;
                msg->SetLength(sizeof(Core::MsgStruct<Core::MsgDiamondResBody>));
                messageQ->EnqueueMessage(msg);
                messagePool->Return(msg);
                Core::gameLogger->LogInfo("cache handler diamond", "diamond read failed", "sessionID", sessionID, "char_id", body->characterID);
                return;
            }

            st->body.resStatus = 1;

            st->body.diamond = res->getUInt64("diamond");
            st->body.totalEarned = res->getUInt64("total_earned");
            st->body.totalSpent = res->getUInt64("total_spent");
            msg->SetLength(sizeof(Core::MsgStruct<Core::MsgDiamondResBody>));
            messageQ->EnqueueMessage(msg);
            messagePool->Return(msg);
            });
        msg = nullptr;
    }

    void Handler::DiamondDeposit(Core::Message*& msg, uint64_t sessionID, Core::MsgDiamondDepositBody* body) {
        dbWorkerBazaar->Enqueue([=](DBConnectionBazaar* conn) {
            int affected = conn->ExecuteUpdate(10, body->diamond, body->diamond, body->characterID);
            Core::MsgStruct<Core::MsgDiamondDepositResBody>* st = reinterpret_cast<Core::MsgStruct<Core::MsgDiamondDepositResBody>*>(msg->GetBuffer());

            st->header.sessionID = sessionID;
            st->header.messageType = Core::MSG_DIAMOND_DEPOSIT_RES;

            if (affected <= 0) {
                st->body.resStatus = 0;
                msg->SetLength(sizeof(Core::MsgStruct<Core::MsgDiamondDepositResBody>));
                messageQ->EnqueueMessage(msg);
                messagePool->Return(msg);
                Core::gameLogger->LogInfo("cache handler diamond", "diamond deposit failed", "sessionID", sessionID, "char_id", body->characterID);
                return;
            }

            auto res = conn->ExecuteSelect(9, body->characterID);
            if (!res || !res->next()) {
                st->body.resStatus = 2;
                msg->SetLength(sizeof(Core::MsgStruct<Core::MsgDiamondDepositResBody>));
                messageQ->EnqueueMessage(msg);
                messagePool->Return(msg);
                return;
            }

            st->body.resStatus = 1;

            st->body.diamond = res->getUInt64("diamond");

            msg->SetLength(sizeof(Core::MsgStruct<Core::MsgDiamondDepositResBody>));
            messageQ->EnqueueMessage(msg);
            messagePool->Return(msg);
            });
        msg = nullptr;
    }
}