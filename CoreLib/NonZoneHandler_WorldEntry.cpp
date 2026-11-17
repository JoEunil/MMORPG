#include "pch.h"
#include "NonZoneHandler.h"

#include "CacheCommandSender.h"
#include "NonZoneConfig.h"
#include "INetworkSender.h"
#include "IPacketView.h"
#include "PacketTypes.h"
#include "PacketWriter.h"
#include "SessionManager.h"
#include "ISessionAuth.h"

#include <cstring>
#include <memory>

namespace Core {
    void NonZoneHandler::ResponseSession(uint64_t sessionID, uint8_t resStatus, uint64_t userID) {
        if (!sessionManager->CompleteAuth(sessionID, userID, resStatus == RES_STATUS::SUCCESS))
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
        sessionManager->BeginAuth(privdata->sessionID);
        auth->CheckSession(privdata.release());
    }

    void NonZoneHandler::GetCharacterList(IPacketView* p) {
        const auto sessionID = p->GetSessionID();
        cacheCommands->EnqueueCharacterList(sessionID, CHANNEL_ID, sessionManager->GetUserID(sessionID));
    }

    void NonZoneHandler::GetCharacterState(IPacketView* p) {
        const auto sessionID = p->GetSessionID();
        const auto* packetBody = parseBody<EnterWorldRequestBody>(p->GetPtr());
        cacheCommands->EnqueueCharacterState(sessionID, CHANNEL_ID, sessionManager->GetUserID(sessionID), packetBody->characterID);
    }
}
