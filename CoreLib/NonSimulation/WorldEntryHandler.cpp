#include "pch.h"
#include "WorldEntryHandler.h"
#include "../Cache/CacheCommandSender.h"
#include "NonSimulationConfig.h"
#include "../Packet/PacketTypes.h"
#include "../Response/Interface/IServerResponseQueue.h"
#include "../Packet/PacketWriter.h"
#include "../World/SessionManager.h"
#include "../Interface/ISessionAuth.h"
#include <cstring>
#include <memory>

namespace Core {
    void WorldEntryHandler::ResponseSession(uint64_t sessionID, uint8_t resStatus, uint64_t userID) {
        if (!sessionManager->CompleteAuth(sessionID, userID, resStatus == RES_STATUS::SUCCESS))
            return;
        responses->Enqueue(sessionID, writer->WriteAuthResponse(resStatus));
    }

    void WorldEntryHandler::CheckSession(IPacketView* p) {
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

    void WorldEntryHandler::GetCharacterList(IPacketView* p) {
        const auto sessionID = p->GetSessionID();
        cacheCommands->EnqueueCharacterList(sessionID, CHANNEL_ID, sessionManager->GetUserID(sessionID));
    }

    void WorldEntryHandler::GetCharacterState(IPacketView* p) {
        const auto sessionID = p->GetSessionID();
        const auto* packetBody = parseBody<EnterWorldRequestBody>(p->GetPtr());
        cacheCommands->EnqueueCharacterState(sessionID, CHANNEL_ID, sessionManager->GetUserID(sessionID), packetBody->characterID);
    }
}
