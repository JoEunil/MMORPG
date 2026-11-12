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
        auto msg = messagePool->AcquireScoped();
        if (msg == nullptr) {
            return ;
		}
        auto* st = InitializeMsg<MsgCharacterListReqBody>(*msg, { p->GetSessionID(), MSG_CHARACTER_LIST_REQ });
        st->body.channelID = CHANNEL_ID;
        st->body.userID = stateManager->GetUserID(p->GetSessionID());
        messageQueue->EnqueueMessage(msg.get());
    }

    void NonZoneHandler::GetCharacterState(IPacketView* p) {
        auto msg = messagePool->AcquireScoped();
        if (msg == nullptr) {
            return;
        }
        auto packetBody = parseBody<EnterWorldRequestBody>(p->GetPtr());
        
        auto* st = InitializeMsg<MsgCharacterStateReqBody>(*msg, { p->GetSessionID(), MSG_CHARACTER_STATE_REQ });
        st->body.channelID = CHANNEL_ID;
        st->body.userID = stateManager->GetUserID(p->GetSessionID());
        st->body.characterID = packetBody->characterID;

        messageQueue->EnqueueMessage(msg.get());
    }
}
