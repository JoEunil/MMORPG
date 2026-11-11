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
        SessionCallbackData* privdata = new SessionCallbackData;
        privdata->sessionID = p->GetSessionID();
        privdata->userID = body->userID;
        std::memcpy(privdata->sessionToken.data(), body->sessionToken, privdata->sessionToken.size());
        privdata->callbackFunc = &ResponseSession;
        stateManager->BeginAuth(privdata->sessionID);
        auth->CheckSession(privdata);
    }

    void NonZoneHandler::GetCharacterList(IPacketView* p) {
        Message* msg = messagePool->Acquire();
        if (msg == nullptr) {
            p = nullptr;
            return ;
		}
        MsgStruct<MsgCharacterListReqBody>* st = reinterpret_cast<MsgStruct<MsgCharacterListReqBody>*>(msg->GetBuffer());
        
        st->header.sessionID = p->GetSessionID();
        st->header.messageType = MSG_CHARACTER_LIST_REQ;
        st->body.channelID = CHANNEL_ID;
        st->body.userID = stateManager->GetUserID(p->GetSessionID());
        msg->SetLength(sizeof(MsgStruct<MsgCharacterListReqBody>));
        messageQueue->EnqueueMessage(msg);
        messagePool->Return(msg);
    }

    void NonZoneHandler::GetCharacterState(IPacketView* p) {
        Message* msg = messagePool->Acquire();
        if (msg == nullptr) {
            return;
        }
        MsgStruct<MsgCharacterStateReqBody>* st = reinterpret_cast<MsgStruct<MsgCharacterStateReqBody>*>(msg->GetBuffer());
        
        auto packetBody = parseBody<EnterWorldRequestBody>(p->GetPtr());
        
        st->header.sessionID = p->GetSessionID();
        st->header.messageType = MSG_CHARACTER_STATE_REQ;
        st->body.channelID = CHANNEL_ID;
        st->body.userID = stateManager->GetUserID(p->GetSessionID());
        st->body.characterID = packetBody->characterID;

        msg->SetLength(sizeof(MsgStruct<MsgCharacterStateReqBody>));
        messageQueue->EnqueueMessage(msg);
        messagePool->Return(msg);
    }
}
