#include "Handler.h"

#include <CoreLib/Message.h>
#include <CoreLib/MessageTypes.h>
#include <CoreLib/IMessageQueue.h>
#include <CoreLib/LoggerGlobal.h>

#include "DBWorker.h"

namespace Cache {
    void Handler::DiamondRequest(ScopedMessage& msg, uint64_t sessionID, Core::MsgDiamondReqBody* body) {
        if (!CheckMessageLength<Core::MsgDiamondReqBody>(msg.get()))
            return;

        const uint64_t characterID = body->characterID;
        const uint32_t requestID = body->requestID;
        EnqueueMessageWork(dbWorkerBazaar, msg, [this, sessionID, characterID, requestID](DBConnectionBazaar* conn, Core::Message* msg) {
            auto res = conn->ExecuteSelect(9, characterID);
            Core::MsgStruct<Core::MsgDiamondResBody>* st = reinterpret_cast<Core::MsgStruct<Core::MsgDiamondResBody>*>(msg->GetBuffer());

            st->header.sessionID = sessionID;
            st->header.messageType = Core::MSG_DIAMOND_RES;
            st->body = {};
            st->body.requestID = requestID;
            st->body.characterID = characterID;

            if (!res || !res->next()) {
                st->body.resStatus = 0;
                msg->SetLength(sizeof(Core::MsgStruct<Core::MsgDiamondResBody>));
                messageQ->EnqueueMessage(msg);
                Core::gameLogger->LogInfo("cache handler diamond", "diamond read failed", "sessionID", sessionID, "char_id", characterID);
                return;
            }

            st->body.resStatus = 1;

            st->body.diamond = res->getUInt64("diamond");
            st->body.totalEarned = res->getUInt64("total_earned");
            st->body.totalSpent = res->getUInt64("total_spent");
            msg->SetLength(sizeof(Core::MsgStruct<Core::MsgDiamondResBody>));
            messageQ->EnqueueMessage(msg);
            });
    }

    void Handler::DiamondDeposit(ScopedMessage& msg, uint64_t sessionID, Core::MsgDiamondDepositBody* body) {
        if (!CheckMessageLength<Core::MsgDiamondDepositBody>(msg.get()))
            return;

        const auto request = *body;
        EnqueueMessageWork(dbWorkerBazaar, msg, [this, sessionID, request](DBConnectionBazaar* conn, Core::Message* msg) {
            int affected = conn->ExecuteUpdate(10, request.diamond, request.diamond, request.characterID);
            Core::MsgStruct<Core::MsgDiamondDepositResBody>* st = reinterpret_cast<Core::MsgStruct<Core::MsgDiamondDepositResBody>*>(msg->GetBuffer());

            st->header.sessionID = sessionID;
            st->header.messageType = Core::MSG_DIAMOND_DEPOSIT_RES;

            if (affected <= 0) {
                st->body.resStatus = 0;
                msg->SetLength(sizeof(Core::MsgStruct<Core::MsgDiamondDepositResBody>));
                messageQ->EnqueueMessage(msg);
                Core::gameLogger->LogInfo("cache handler diamond", "diamond deposit failed", "sessionID", sessionID, "char_id", request.characterID);
                return;
            }

            auto res = conn->ExecuteSelect(9, request.characterID);
            if (!res || !res->next()) {
                st->body.resStatus = 2;
                msg->SetLength(sizeof(Core::MsgStruct<Core::MsgDiamondDepositResBody>));
                messageQ->EnqueueMessage(msg);
                return;
            }

            st->body.resStatus = 1;

            st->body.diamond = res->getUInt64("diamond");

            msg->SetLength(sizeof(Core::MsgStruct<Core::MsgDiamondDepositResBody>));
            messageQ->EnqueueMessage(msg);
            });
    }
}
