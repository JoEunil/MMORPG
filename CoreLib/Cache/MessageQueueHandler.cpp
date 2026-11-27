#include "pch.h"
#include "MessageQueueHandler.h"
#include "../Interface/IMessage.h"
#include "MessageTypes.h"
#include "../Response/Interface/IServerResponseQueue.h"
#include "../Packet/PacketWriter.h"
#include "../World/LobbyZone.h"
#include "../World/SessionManager.h"
#include "../LoggerGlobal.h"
#include "../World/Interface/IZoneRequestQueue.h"

namespace Core {
    namespace {
        CharacterState BuildCharacterState(uint64_t sessionID, const MsgCharacterStateResBody& body) {
            CharacterState character;
            character.profileId = body.profileId;
            character.profileVersion = body.profileVersion;
            character.characterID = body.charID;
            character.dir = body.dir;
            character.attack = body.attack;
            character.equipment = Equipment(body.equippedWeaponItemID, body.equippedShieldItemID, body.equippedBodyItemID);
            character.level = body.level;
            character.exp = body.exp;
            character.hp = body.hp;
            character.mp = body.mp;
            character.maxHp = body.maxHp;
            character.maxMp = body.maxMp;
            character.lastZone = body.currentZone;
            character.sessionID = sessionID;
            character.x = body.startX;
            character.y = body.startY;
            return character;
        }

        template<typename T>
        const T* ReadResponse(IMessage& msg) {
            if (msg.GetLength() != sizeof(MsgStruct<T>))
                return nullptr;
            return parseMsgBody<T>(msg.GetBuffer());
        }
    }

    bool MessageQueueHandler::IsCurrentFieldCharacter(uint64_t sessionID, uint64_t characterID) {
        return sessionManager && characterID != 0 && sessionManager->GetFieldCharacterID(sessionID) == characterID;
    }

    template<MessageQueueHandler::ResponseScope Scope, typename T>
    void MessageQueueHandler::DispatchResponse(IMessage& msg, uint64_t sessionID, ResponseWriter<T> write) {
        const auto* body = ReadResponse<T>(msg);
        if (!body)
            return;

        if constexpr (Scope == ResponseScope::FieldCharacter) {
            if (!IsCurrentFieldCharacter(sessionID, body->characterID))
                return;
        } else if constexpr (Scope == ResponseScope::FieldSession) {
            if (!sessionManager || !sessionManager->IsFieldSession(sessionID))
                return;
        }

        responses->Enqueue(sessionID, (writer->*write)(body));
    }

    void MessageQueueHandler::Process(IMessage* msg) {
        if (msg == nullptr) {
            errorLogger->LogError("mq handler", "Invlid IMessage");
            return;
        }
        if (msg->GetLength() < sizeof(MsgHeader)) {
            errorLogger->LogError("mq handler", "invalid message length");
            return;
        }

        const auto* header = parseMsgHeader(msg->GetBuffer());
        const auto sessionID = header->sessionID;
        switch (header->messageType) {
        case MSG_CHARACTER_LIST_RES:
            DispatchResponse(*msg, sessionID, &PacketWriter::WriteCharacterListResponse);
            break;
        case MSG_CHARACTER_STATE_RES:
            if (const auto* body = ReadResponse<MsgCharacterStateResBody>(*msg))
                CharacterStateResponse(sessionID, body);
            break;
        case MSG_INVENTORY_RES:
            DispatchResponse(*msg, sessionID, &PacketWriter::WriteInventoryResponse);
            break;
        case MSG_INVENTORY_UPDATE_RES:
            DispatchResponse(*msg, sessionID, &PacketWriter::WriteInventoryUpdateResponse);
            break;
        case MSG_BAZAAR_BUY_RES:
            DispatchResponse<ResponseScope::FieldCharacter>(*msg, sessionID, &PacketWriter::WriteBazaarBuyResponse);
            break;
        case MSG_BAZAAR_OUTBOX_LIST_RES:
            DispatchResponse<ResponseScope::FieldCharacter>(*msg, sessionID, &PacketWriter::WriteBazaarOutboxListResponse);
            break;
        case MSG_BAZAAR_CHECK_OUTBOX_RES:
            DispatchResponse<ResponseScope::FieldCharacter>(*msg, sessionID, &PacketWriter::WriteBazaarCheckOutboxResponse);
            break;
        case MSG_BAZAAR_CANCEL_LISTING_RES:
            DispatchResponse<ResponseScope::FieldCharacter>(*msg, sessionID, &PacketWriter::WriteBazaarCancelListingResponse);
            break;
        case MSG_BAZAAR_CLAIM_RES:
            DispatchResponse<ResponseScope::FieldCharacter>(*msg, sessionID, &PacketWriter::WriteBazaarClaimResponse);
            break;
        case MSG_BAZAAR_MY_LIST_RES:
            DispatchResponse<ResponseScope::FieldCharacter>(*msg, sessionID, &PacketWriter::WriteBazaarMyListResponse);
            break;
        case MSG_BAZAAR_SEARCH_RES:
            DispatchResponse<ResponseScope::FieldSession>(*msg, sessionID, &PacketWriter::WriteBazaarSearchResponse);
            break;
        case MSG_BAZAAR_REGISTER_RES:
            DispatchResponse<ResponseScope::FieldCharacter>(*msg, sessionID, &PacketWriter::WriteBazaarRegisterResponse);
            break;
        case MSG_CURRENCY_DEPOSIT_RES:
            if (const auto* body = ReadResponse<MsgCurrencyDepositResBody>(*msg))
                GoldDepositResponse(sessionID, body);
            break;
        case MSG_CURRENCY_RES:
            DispatchResponse<ResponseScope::FieldCharacter>(*msg, sessionID, &PacketWriter::WriteWalletGoldResponse);
            break;
        case MSG_DIAMOND_RES:
            DispatchResponse<ResponseScope::FieldCharacter>(*msg, sessionID, &PacketWriter::WriteWalletDiamondResponse);
            break;
        case MSG_PROFILE_RENAME_RES:
            if (const auto* body = ReadResponse<MsgProfileRenameResBody>(*msg))
                ProfileRenameResponse(sessionID, body);
            break;
        default:
            errorLogger->LogError("mq handler", "invalid message type");
        }
    }

    void MessageQueueHandler::CharacterStateResponse(uint64_t sessionID, const MsgCharacterStateResBody* body) {
        //client에서는 full snapshot 받을 때 까지 로딩 화면
        if (body->resStatus != 0) {
            auto character = BuildCharacterState(sessionID, *body);
            if (!lobbyZone->ImmigrateChar(sessionID, character)) {
                errorLogger->LogError("mq handler", "Failed to enter lobby");
                return;
            }
        }

        responses->Enqueue(sessionID, writer->WriteEnterWorldResponse(body));
    }

    void MessageQueueHandler::GoldDepositResponse(uint64_t sessionID, const MsgCurrencyDepositResBody* body) {
        if (!IsCurrentFieldCharacter(sessionID, body->characterID))
            return;

        responses->Enqueue(sessionID, writer->WriteGoldReward(body->resStatus));
    }

    void MessageQueueHandler::ProfileRenameResponse(uint64_t sessionID, const MsgProfileRenameResBody* body) {
        if (body->resStatus != 1) {
            gameLogger->LogInfo("mq handler", "profile rename failed", {{"sessionID", sessionID}, {"profile_id", body->profileId}});
            return;
        }
        if (sessionManager == nullptr) {
            errorLogger->LogError("mq handler", "sessionManager not initialized", {{"sessionID", sessionID}});
            return;
        }
        const ProfileVersionUpdate update{sessionID, body->profileId, body->version};
        const auto route = lobbyZone->ApplyProfileVersion(update);
        if (route && !zoneRequests->TryEnqueueProfileVersion(*route, update))
            errorLogger->LogWarn("mq handler", "could not route profile version", {{"sessionID", sessionID}, {"zoneID", *route}});
    }
}
