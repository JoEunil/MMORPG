#include "pch.h"
#include "PlayerDataRequestHandler.h"
#include "../Cache/CacheCommandSender.h"
#include "../Packet/PacketTypes.h"
#include "../Cache/MessageTypes.h"
#include "../Response/Interface/IServerResponseQueue.h"
#include "../Packet/PacketWriter.h"
#include "../World/SessionManager.h"
#include "../Interface/IProfileCache.h"

namespace Core {
    void PlayerDataRequestHandler::RequestProfileRename(uint64_t sessionID, uint32_t profileId, const char* name, uint16_t nameLen) {
        if (profileId == INVALID_PROFILE_ID || name == nullptr || nameLen == 0 || nameLen >= MAX_CHARNAME_LEN) {
            errorLogger->LogError("non zone handler", "profile rename invalid", {{"session", sessionID}, {"profile_id", profileId}, {"nameLen", nameLen}});
            return;
        }
        cacheCommands->EnqueueProfileRename(sessionID, profileId, {name, nameLen});
    }

    void PlayerDataRequestHandler::GetInventory(IPacketView* p) {
        const auto sessionID = p->GetSessionID();
        const auto characterID = sessionManager->GetCharacterID(sessionID);
        if (characterID == 0)
            return;

        cacheCommands->EnqueueInventory(sessionID, characterID);
    }

    void PlayerDataRequestHandler::WalletRequest(IPacketView* p) {
        if (p->GetLength() != sizeof(PacketStruct<WalletRequestBody>))
            return;
        const auto* header = parseHeader(p->GetPtr());
        const auto request = *parseBody<WalletRequestBody>(p->GetPtr());
        if (header->magic != MAGIC || header->opcode != OP::WALLET_REQUEST || header->length != p->GetLength() || IsSimulationFlag(header->flags) ||
            request.requestID == 0)
            return;
        const uint64_t characterID = sessionManager->GetFieldCharacterID(p->GetSessionID());
        if (characterID == 0)
            return;

        if (!cacheCommands->EnqueueGoldBalance({p->GetSessionID(), characterID, request.requestID})) {
            MsgCurrencyResBody failed{};
            failed.requestID = request.requestID;
            failed.characterID = characterID;
            if (sessionManager->GetFieldCharacterID(p->GetSessionID()) == characterID)
                responses->Enqueue(p->GetSessionID(), writer->WriteWalletGoldResponse(&failed));
        }

        if (!cacheCommands->EnqueueDiamondBalance({p->GetSessionID(), characterID, request.requestID})) {
            MsgDiamondResBody failed{};
            failed.requestID = request.requestID;
            failed.characterID = characterID;
            if (sessionManager->GetFieldCharacterID(p->GetSessionID()) == characterID)
                responses->Enqueue(p->GetSessionID(), writer->WriteWalletDiamondResponse(&failed));
        }
    }

    bool PlayerDataRequestHandler::GetProfileBatch(IPacketView* p) {
        auto session = p->GetSessionID();
        if (profileCache == nullptr) {
            errorLogger->LogError("non zone handler", "profileCache not initialized", {{"session", session}});
            return true;
        }
        auto body = parseBody<ProfileBatchReqBody>(p->GetPtr());

        // count는 클라이언트가 보낸 값이라 그대로 믿고 인덱싱하면 버퍼를 넘는다.
        if (body->count == 0 || body->count > MAX_PROFILE_BATCH) {
            errorLogger->LogError("non zone handler", "profile batch count invalid", {{"session", session}, {"count", body->count}});
            return false;
        }
        const size_t needed = sizeof(PacketHeader) + sizeof(body->count) + body->count * sizeof(uint32_t);
        if (p->GetLength() < needed) {
            errorLogger->LogError("non zone handler", "profile batch length invalid", {{"session", session}, {"count", body->count}, {"length", p->GetLength()}});
            return false;
        }

        ProfileEntry entries[MAX_PROFILE_BATCH];
        uint16_t found = profileCache->GetBatch(body->profileIds, body->count, entries);

        responses->Enqueue(session, writer->WriteProfileBatchRes(entries, found));
            return true;
    }
}
