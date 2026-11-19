#include "pch.h"
#include "NonSimulationHandler.h"

#include "../Cache/CacheCommandSender.h"
#include "NonSimulationConfig.h"
#include "../Interface/INetworkSender.h"
#include "../Interface/IPacketView.h"
#include "../Packet/PacketTypes.h"
#include "../Packet/PacketWriter.h"
#include "../World/SessionManager.h"
#include "../Interface/ISessionAuth.h"

#include <cstring>
#include <memory>

namespace Core {
    void NonSimulationHandler::ResponseSession(uint64_t sessionID, uint8_t resStatus, uint64_t userID) {
        if (!sessionManager->CompleteAuth(sessionID, userID, resStatus == RES_STATUS::SUCCESS))
            return;
        auto res = writer->WriteAuthResponse(resStatus);
        if (res == nullptr)
            return;
        networkSender->SendDataUnique(sessionID, std::move(res));
    }

    void NonSimulationHandler::CheckSession(IPacketView* p) {
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

    void NonSimulationHandler::GetCharacterList(IPacketView* p) {
        const auto sessionID = p->GetSessionID();
        cacheCommands->EnqueueCharacterList(sessionID, CHANNEL_ID, sessionManager->GetUserID(sessionID));
    }

    void NonSimulationHandler::GetCharacterState(IPacketView* p) {
        const auto sessionID = p->GetSessionID();
        const auto* packetBody = parseBody<EnterWorldRequestBody>(p->GetPtr());
        cacheCommands->EnqueueCharacterState(sessionID, CHANNEL_ID, sessionManager->GetUserID(sessionID), packetBody->characterID);
    }
}
