#include "pch.h"
#include "MessageQueueHandler.h"
#include "Message.h"
#include "MessageTypes.h"
#include "MessagePool.h"
#include "INetworkSender.h"
#include "PacketWriter.h"
#include "LobbyZone.h"
#include "StateManager.h"
#include "LoggerGlobal.h"
#include "Config.h"

namespace Core {
    namespace {
        template<typename T>
        const T* ReadResponse(Message& msg) {
            if (msg.GetLength() != sizeof(MsgStruct<T>))
                return nullptr;
            return parseMsgBody<T>(msg.GetBuffer());
        }
    }

    bool MessageQueueHandler::IsCurrentFieldCharacter(uint64_t sessionID, uint64_t characterID) {
        return stateManager && characterID != 0 && stateManager->GetFieldCharacterID(sessionID) == characterID;
    }

    template<typename T>
    void MessageQueueHandler::SendResponse(uint64_t sessionID, const T* body, ResponseWriter<T> write) {
        auto packet = (writer->*write)(body);
        if (packet)
            networkSender->SendDataUnique(sessionID, std::move(packet));
    }

    template<MessageQueueHandler::ResponseScope Scope, typename T>
    void MessageQueueHandler::DispatchResponse(Message& msg, uint64_t sessionID, ResponseWriter<T> write) {
        const auto* body = ReadResponse<T>(msg);
        if (!body)
            return;

        if constexpr (Scope == ResponseScope::FieldCharacter) {
            if (!IsCurrentFieldCharacter(sessionID, body->characterID))
                return;
        } else if constexpr (Scope == ResponseScope::FieldSession) {
            if (!stateManager || !stateManager->IsFieldSession(sessionID))
                return;
        }

        SendResponse(sessionID, body, write);
    }

    void MessageQueueHandler::Process(Message* msg) {
        if (msg == nullptr) {
            errorLogger->LogError("mq handler", "Invlid Message");
            return;
        }
        if (msg->GetLength() < sizeof(MsgHeader)) {
            errorLogger->LogError("mq handler", "invalid message length");
            messagePool->Return(msg);
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
        messagePool->Return(msg);
    }

    void MessageQueueHandler::CharacterStateResponse(uint64_t sessionID, const MsgCharacterStateResBody* body) {
        //client에서는 full snapshot 받을 때 까지 로딩 화면
        CharacterState temp;
        if (body->resStatus != 0) {
            // 이름은 zone이 들고 있지 않는다. 자기 이름은 EnterWorld 응답으로만 내려간다.
            temp.profileId = body->profileId;
            temp.profileVersion = body->profileVersion;
            temp.characterID = body->charID;
            temp.dir = body->dir;
            temp.attack = body->attack;
            temp.equippedWeaponItemID = body->equippedWeaponItemID;
            temp.equippedShieldItemID = body->equippedShieldItemID;
            temp.equippedBodyItemID = body->equippedBodyItemID;
            temp.level = body->level;
            temp.exp = body->exp;
            temp.hp = body ->hp;
            temp.mp = body ->mp;
            temp.maxHp = body->maxHp;
            temp.maxMp = body->maxMp;
            temp.lastZone = body->currentZone;
            temp.sessionID = sessionID;
            temp.x = body->startX;
            temp.y = body->startY;
            if (!lobbyZone->ImmigrateChar(sessionID, temp)) {
                errorLogger->LogError("mq handler", "Failed to enter lobby");
                return;
            }
        }

        SendResponse(sessionID, body, &PacketWriter::WriteEnterWorldResponse);
    }

    void MessageQueueHandler::GoldDepositResponse(uint64_t sessionID, const MsgCurrencyDepositResBody* body) {
        if (!IsCurrentFieldCharacter(sessionID, body->characterID))
            return;

        auto packet = writer->WriteGoldReward(body->resStatus);
        if (packet)
            networkSender->SendDataUnique(sessionID, std::move(packet));
    }

    // rename이 DB에 확정된 뒤 도는 경로. zone이 들고 있는 profileVersion 사본을
    // 여기서 갱신해야 delta로 전파된다.
    void MessageQueueHandler::ProfileRenameResponse(uint64_t sessionID, const MsgProfileRenameResBody* body) {
        if (body->resStatus == 0) {
            gameLogger->LogInfo("mq handler", "profile rename failed", "sessionID", sessionID, "profile_id", body->profileId);
            return;
        }
        if (stateManager == nullptr) {
            errorLogger->LogError("mq handler", "stateManager not initialized", "sessionID", sessionID);
            return;
        }
        stateManager->UpdateProfileVersion(sessionID, body->version);
    }
}
