#include "pch.h"
#include "NonSimulationHandler.h"

#include "CacheCommandSender.h"
#include "PacketTypes.h"
#include "IPacketView.h"
#include "ISessionAuth.h"
#include "INetworkSender.h"
#include "PacketWriter.h"
#include "ZoneState.h"
#include "MessageTypes.h"
#include "SessionManager.h"
#include "LobbyZone.h"
#include "ChatThreadPool.h"
#include "IProfileCache.h"
#include "LoggerGlobal.h"
#include "PacketConfig.h"

namespace Core {
    void NonSimulationHandler::Initialize(INetworkSender* sender, ISessionAuth* s, PacketWriter* p, const CacheCommandSender* commands, SessionManager* manager, LobbyZone* lobby, ChatThreadPool* c, IProfileCache* pc) {
        networkSender = sender;
        auth = s;
        writer = p;
        cacheCommands = commands;
        sessionManager = manager;
        lobbyZone = lobby;
        chat = c;
        profileCache = pc;
    }

    bool NonSimulationHandler::IsReady() {
        if (networkSender == nullptr) return false;
        if (auth == nullptr) return false;
        if (writer == nullptr) return false;
        if (cacheCommands == nullptr) return false;
        if (sessionManager == nullptr) return false;
        if (lobbyZone == nullptr) return false;
        
        return true;
    }

    void NonSimulationHandler::Process(IPacketView* p) {
        PacketHeader* h = parseHeader(p->GetPtr());
        uint16_t zoneID = 0;
        if (p->GetOpcode() != OP::AUTH ) {
            zoneID = sessionManager->GetZoneID(p->GetSessionID());
        }
        
        switch (p->GetOpcode())
        {
            case OP::AUTH:
                CheckSession(p);
                break;
            case OP::CHARACTER_LIST:
                GetCharacterList(p);
                break;
            case OP::ENTER_WORLD:
                GetCharacterState(p);
                break;
            case OP::CHAT:
                Chat(p);
                break;
            case OP::ZONE_CHANGE:
                ZoneChange(p);
                break;
            case OP::ZONE_RESYNC:
                if (p->GetLength() == sizeof(PacketStruct<ZoneResyncBody>)) {
                    const auto* body = parseBody<ZoneResyncBody>(p->GetPtr());
                    sessionManager->QueueBootstrap(p->GetSessionID(), body->ownershipEpoch);
                }
                break;
            case OP::RESPAWN_TO_TOWN:
                RespawnToTown(p);
                break;
            case OP::ITEM_USE:
                UseItem(p);
                break;
            case OP::EQUIPMENT_GET:
                GetEquipment(p);
                break;
            case OP::EQUIPMENT_SET:
                SetEquipment(p);
                break;
            case OP::INVENTORY_REQ:
                GetInventory(p);
                break;
            case OP::BAZAAR_BUY:
                BazaarBuy(p);
                break;
            case OP::BAZAAR_OUTBOX_LIST:
                BazaarOutboxList(p);
                break;
            case OP::BAZAAR_CHECK_OUTBOX:
                BazaarCheckOutbox(p);
                break;
            case OP::BAZAAR_CANCEL_LISTING:
                BazaarCancelListing(p);
                break;
            case OP::BAZAAR_CLAIM:
                BazaarClaim(p);
                break;
            case OP::BAZAAR_MY_LIST:
                BazaarMyList(p);
                break;
            case OP::BAZAAR_SEARCH:
                BazaarSearch(p);
                break;
            case OP::BAZAAR_REGISTER:
                BazaarRegister(p);
                break;
            case OP::WALLET_REQUEST:
                WalletRequest(p);
                break;
            case OP::PROFILE_BATCH:
                GetProfileBatch(p);
                break;
            default:
                errorLogger->LogInfo("non zone handler",  "UnDefined OPCODE", "opcode", p->GetOpcode(),"header opcode", h->opcode,"sessionId", p->GetSessionID());
                break;
        }
        
    }

    void NonSimulationHandler::Disconnect(uint64_t sessionID) {
        sessionManager->Disconnect(sessionID);
    }

    void NonSimulationHandler::RequestProfileRename(uint64_t sessionID, uint32_t profileId, const char* name, uint16_t nameLen) {
        if (profileId == INVALID_PROFILE_ID || name == nullptr || nameLen == 0 || nameLen >= MAX_CHARNAME_LEN) {
            errorLogger->LogError("non zone handler", "profile rename invalid", "session", sessionID, "profile_id", profileId, "nameLen", nameLen);
            return;
        }
        cacheCommands->EnqueueProfileRename(sessionID, profileId, {name, nameLen});
    }

    void NonSimulationHandler::UseItem(IPacketView* p) {
        const auto request = *parseBody<ItemUseRequestBody>(p->GetPtr());
        const auto result = sessionManager->UseItem(p->GetSessionID(), request);
        auto response = writer->WriteItemUseResponse(result);
        if (response)
            networkSender->SendDataUnique(p->GetSessionID(), std::move(response));
    }

    void NonSimulationHandler::GetEquipment(IPacketView* p) {
        const auto request = *parseBody<EquipmentGetRequestBody>(p->GetPtr());
        const auto result = sessionManager->Equipment(p->GetSessionID(), request.requestID, request.ownershipEpoch, false);
        auto response = writer->WriteEquipmentResponse(result);
        if (response)
            networkSender->SendDataUnique(p->GetSessionID(), std::move(response));
    }

    void NonSimulationHandler::SetEquipment(IPacketView* p) {
        const auto request = *parseBody<EquipmentSetRequestBody>(p->GetPtr());
        if (request.slot > 2)
            return;

        const auto result = sessionManager->Equipment(p->GetSessionID(), request.requestID, request.ownershipEpoch, true, request.slot, request.itemID);
        auto response = writer->WriteEquipmentResponse(result);
        if (response)
            networkSender->SendDataUnique(p->GetSessionID(), std::move(response));
    }

    void NonSimulationHandler::GetInventory(IPacketView* p) {
        const auto sessionID = p->GetSessionID();
        const auto characterID = sessionManager->GetCharacterID(sessionID);
        if (characterID == 0)
            return;

        cacheCommands->EnqueueInventory(sessionID, characterID);
    }

    void NonSimulationHandler::WalletRequest(IPacketView* p) {
        if (p->GetLength() != sizeof(PacketStruct<WalletRequestBody>)) return;
        const auto* header = parseHeader(p->GetPtr());
        const auto request = *parseBody<WalletRequestBody>(p->GetPtr());
        if (header->magic != MAGIC || header->opcode != OP::WALLET_REQUEST || header->length != p->GetLength()
            || IsSimulationFlag(header->flags) || request.requestID == 0) return;
        const uint64_t characterID = sessionManager->GetFieldCharacterID(p->GetSessionID());
        if (characterID == 0) return;

        if (!cacheCommands->EnqueueGoldBalance(p->GetSessionID(), characterID, request.requestID)) {
            MsgCurrencyResBody failed{};
            failed.requestID = request.requestID;
            failed.characterID = characterID;
            auto response = writer->WriteWalletGoldResponse(&failed);
            if (response && sessionManager->GetFieldCharacterID(p->GetSessionID()) == characterID)
                networkSender->SendDataUnique(p->GetSessionID(), std::move(response));
        }

        if (!cacheCommands->EnqueueDiamondBalance(p->GetSessionID(), characterID, request.requestID)) {
            MsgDiamondResBody failed{};
            failed.requestID = request.requestID;
            failed.characterID = characterID;
            auto response = writer->WriteWalletDiamondResponse(&failed);
            if (response && sessionManager->GetFieldCharacterID(p->GetSessionID()) == characterID)
                networkSender->SendDataUnique(p->GetSessionID(), std::move(response));
        }
    }

    void NonSimulationHandler::Chat(IPacketView* p) {
        auto body = parseBody<ChatRequestBody>(p->GetPtr());
        uint8_t* startPtr = p->GetPtr() + sizeof(PacketStruct<ChatRequestBody>);
        auto session = p->GetSessionID();
        auto zoneID = sessionManager->GetZoneID(session);
        ChatEvent event;
        event.type = ChatEventType::CHAT;
        event.senderSessionID = session;
        event.key.scope = body->scope;
        switch (body->scope) {
        case CHAT_SCOPE::Whisper:
            event.key.id = body->targetChatID;
            break;
        case CHAT_SCOPE::Zone:
            event.key.id = static_cast<uint64_t>(zoneID);
        }
        if (p->GetLength() < sizeof(PacketStruct<ChatRequestBody>) + body->messageLength) {
            errorLogger->LogError("non zone handler", "message length invalid", "session", session, "messageLength", body->messageLength);
            Disconnect(session);
            return;
        }
        event.message = std::string(reinterpret_cast<char*>(startPtr), body->messageLength);
        chat->EnqueueChat(event);
    }

    // 클라이언트가 모르는 profile_id 목록을 보내면 캐시에 있는 것만 돌려준다.
    // NonSimulation 스레드에서 처리하므로 zone 틱과 무관하고, write-through 캐시라
    // DB를 기다릴 일이 없어 동기 조회로 충분하다.
    void NonSimulationHandler::GetProfileBatch(IPacketView* p) {
        auto session = p->GetSessionID();
        if (profileCache == nullptr) {
            errorLogger->LogError("non zone handler", "profileCache not initialized", "session", session);
            return;
        }
        auto body = parseBody<ProfileBatchReqBody>(p->GetPtr());

        // count는 클라이언트가 보낸 값이라 그대로 믿고 인덱싱하면 버퍼를 넘는다.
        if (body->count == 0 || body->count > MAX_PROFILE_BATCH) {
            errorLogger->LogError("non zone handler", "profile batch count invalid", "session", session, "count", body->count);
            Disconnect(session);
            return;
        }
        const size_t needed = sizeof(PacketHeader) + sizeof(body->count) + body->count * sizeof(uint32_t);
        if (p->GetLength() < needed) {
            errorLogger->LogError("non zone handler", "profile batch length invalid", "session", session, "count", body->count, "length", p->GetLength());
            Disconnect(session);
            return;
        }

        ProfileEntry entries[MAX_PROFILE_BATCH];
        uint16_t found = profileCache->GetBatch(body->profileIds, body->count, entries);

        auto res = writer->WriteProfileBatchRes(entries, found);
        if (!res)
            return;
        networkSender->SendDataUnique(session, std::move(res));
    }
}
