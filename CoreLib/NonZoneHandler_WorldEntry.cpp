#include "pch.h"
#include "NonZoneHandler.h"

#include "Config.h"
#include "IMessageQueue.h"
#include "INetworkSender.h"
#include "IPacketView.h"
#include "Message.h"
#include "PacketTypes.h"
#include "PacketWriter.h"
#include "StateManager.h"
#include "ISessionAuth.h"

#include <cstring>
#include <memory>

namespace Core {
    void NonZoneHandler::ResponseSession(uint64_t sessionID, uint8_t resStatus, uint64_t userID) {
        if (!stateManager->CompleteAuth(sessionID, userID, resStatus == RES_STATUS::SUCCESS))
            return;
        auto res = writer->WriteAuthResponse(resStatus);
        if (res == nullptr)
            return;
        networkSender->SendDataUnique(sessionID, std::move(res));
    }

    void NonZoneHandler::CheckSession(IPacketView* p) {
        AuthRequestBody* body = parseBody<AuthRequestBody>(p->GetPtr());
        auto privdata = std::make_unique<SessionCallbackData>();
        privdata->sessionID = p->GetSessionID();
        privdata->userID = body->userID;
        std::memcpy(privdata->sessionToken.data(), body->sessionToken, privdata->sessionToken.size());
        privdata->callbackFunc = [this](uint64_t sessionID, uint8_t resStatus, uint64_t userID) {
            ResponseSession(sessionID, resStatus, userID);
        };
        stateManager->BeginAuth(privdata->sessionID);
        auth->CheckSession(privdata.release());
    }

    void NonZoneHandler::GetCharacterList(IPacketView* p) {
        const auto sessionID = p->GetSessionID();
        TryEnqueueMessage(
            { sessionID, MSG_CHARACTER_LIST_REQ },
            MsgCharacterListReqBody{ CHANNEL_ID, stateManager->GetUserID(sessionID) });
    }

    void NonZoneHandler::GetCharacterState(IPacketView* p) {
        const auto sessionID = p->GetSessionID();
        const auto* packetBody = parseBody<EnterWorldRequestBody>(p->GetPtr());
        TryEnqueueMessage(
            { sessionID, MSG_CHARACTER_STATE_REQ },
            MsgCharacterStateReqBody{ CHANNEL_ID, stateManager->GetUserID(sessionID), packetBody->characterID });
    }
}
