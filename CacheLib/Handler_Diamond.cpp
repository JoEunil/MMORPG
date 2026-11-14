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
            auto res = conn->ExecuteSelect(StatementId::SelectDiamond, characterID);
            const Core::MsgHeader header{ sessionID, Core::MSG_DIAMOND_RES };
            Core::MsgDiamondResBody response{};
            response.requestID = requestID;
            response.characterID = characterID;

            if (!res || !res->next()) {
                TryEnqueueResponse(*msg, header, response);
                Core::gameLogger->LogInfo("cache handler diamond", "diamond read failed", "sessionID", sessionID, "char_id", characterID);
                return;
            }

            response.resStatus = 1;
            response.diamond = res->getUInt64("diamond");
            response.totalEarned = res->getUInt64("total_earned");
            response.totalSpent = res->getUInt64("total_spent");
            TryEnqueueResponse(*msg, header, response);
            });
    }

    void Handler::DiamondDeposit(ScopedMessage& msg, uint64_t sessionID, Core::MsgDiamondDepositBody* body) {
        if (!CheckMessageLength<Core::MsgDiamondDepositBody>(msg.get()))
            return;

        const auto request = *body;
        EnqueueMessageWork(dbWorkerBazaar, msg, [this, sessionID, request](DBConnectionBazaar* conn, Core::Message* msg) {
            int affected = conn->ExecuteUpdate(StatementId::DepositDiamond, request.diamond, request.diamond, request.characterID);
            const Core::MsgHeader header{ sessionID, Core::MSG_DIAMOND_DEPOSIT_RES };
            Core::MsgDiamondDepositResBody response{};

            if (affected <= 0) {
                TryEnqueueResponse(*msg, header, response);
                Core::gameLogger->LogInfo("cache handler diamond", "diamond deposit failed", "sessionID", sessionID, "char_id", request.characterID);
                return;
            }

            auto res = conn->ExecuteSelect(StatementId::SelectDiamond, request.characterID);
            if (!res || !res->next()) {
                response.resStatus = 2;
            } else {
                response.resStatus = 1;
                response.diamond = res->getUInt64("diamond");
            }
            TryEnqueueResponse(*msg, header, response);
            });
    }
}
