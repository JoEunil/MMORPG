#include "pch.h"
#include "Handler.h"
#include "CacheResponseSender.h"

#include <CoreLib/Interface/IMessage.h>
#include <CoreLib/Cache/MessageTypes.h>
#include <CoreLib/LoggerGlobal.h>

#include "DB/DBWorker.h"

namespace Cache {
    void Handler::DiamondRequest(ScopedMessage& msg, uint64_t sessionID, Core::MsgDiamondReqBody* body) {
        if (!CheckMessageLength<Core::MsgDiamondReqBody>(msg.get()))
            return;

        const uint64_t characterID = body->characterID;
        const uint32_t requestID = body->requestID;
        msg.reset();
        dbWorkerBazaar->Enqueue([this, sessionID, characterID, requestID](DBConnectionBazaar* conn) {
            auto res = conn->ExecuteSelect(StatementId::SelectDiamond, characterID);
            const Core::MsgHeader header{ sessionID, Core::MSG_DIAMOND_RES };
            Core::MsgDiamondResBody response{};
            response.requestID = requestID;
            response.characterID = characterID;

            if (!res || !res->next()) {
                responses->Send(header, response);
                Core::gameLogger->LogInfo("cache handler diamond", "diamond read failed", {{"sessionID", sessionID}, {"char_id", characterID}});
                return;
            }

            response.resStatus = 1;
            response.diamond = res->getUInt64("diamond");
            response.totalEarned = res->getUInt64("total_earned");
            response.totalSpent = res->getUInt64("total_spent");
            responses->Send(header, response);
            });
    }

    void Handler::DiamondDeposit(ScopedMessage& msg, uint64_t sessionID, Core::MsgDiamondDepositBody* body) {
        if (!CheckMessageLength<Core::MsgDiamondDepositBody>(msg.get()))
            return;

        const auto request = *body;
        msg.reset();
        dbWorkerBazaar->Enqueue([this, sessionID, request](DBConnectionBazaar* conn) {
            int affected = conn->ExecuteUpdate(StatementId::DepositDiamond, request.diamond, request.diamond, request.characterID);
            const Core::MsgHeader header{ sessionID, Core::MSG_DIAMOND_DEPOSIT_RES };
            Core::MsgDiamondDepositResBody response{};

            if (affected <= 0) {
                responses->Send(header, response);
                Core::gameLogger->LogInfo("cache handler diamond", "diamond deposit failed", {{"sessionID", sessionID}, {"char_id", request.characterID}});
                return;
            }

            auto res = conn->ExecuteSelect(StatementId::SelectDiamond, request.characterID);
            if (!res || !res->next()) {
                response.resStatus = 2;
            } else {
                response.resStatus = 1;
                response.diamond = res->getUInt64("diamond");
            }
            responses->Send(header, response);
            });
    }
}
