#include "pch.h"
#include "NonZoneHandler.h"

#include "PacketTypes.h"
#include "IPacketView.h"
#include "ISessionAuth.h"
#include "INetworkSender.h"
#include "PacketWriter.h"
#include "ZoneState.h"
#include "Message.h"
#include "MessagePool.h"
#include "IMessageQueue.h"
#include "StateManager.h"
#include "LobbyZone.h"
#include "ChatThreadPool.h"
#include "IProfileCache.h"
#include "LoggerGlobal.h"
#include "Config.h"

namespace Core {
    INetworkSender* NonZoneHandler::networkSender = nullptr;
    PacketWriter* NonZoneHandler::writer = nullptr;
    StateManager* NonZoneHandler::stateManager = nullptr;

    void NonZoneHandler::Initialize(INetworkSender* sender, ISessionAuth* s, PacketWriter* p, MessagePool* m, IMessageQueue* mq, StateManager* manager, LobbyZone* lobby, ChatThreadPool* c) {
        networkSender = sender;
        auth = s;
        writer = p;
        messagePool = m;
        messageQueue = mq;
        stateManager = manager;
        lobbyZone = lobby;
        chat = c;
    }

    void NonZoneHandler::InitializeProfileCache(IProfileCache* pc) {
        profileCache = pc;
    }

    bool NonZoneHandler::IsReady() {
        if (networkSender == nullptr) return false;
        if (auth == nullptr) return false;
        if (writer == nullptr) return false;
        if (messagePool == nullptr) return false;
        if (messageQueue == nullptr) return false;
        if (stateManager == nullptr) return false;
        if (lobbyZone == nullptr) return false;
        
        return true;
    }

    void NonZoneHandler::Process(IPacketView* p) {
        PacketHeader* h = parseHeader(p->GetPtr());
        uint16_t zoneID = 0;
        if (p->GetOpcode() != OP::AUTH ) {
            zoneID = stateManager->GetZoneID(p->GetSessionID());
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
                    stateManager->SendBootstrap(p->GetSessionID(), body->ownershipEpoch);
                }
                break;
            case OP::RESPAWN_TO_TOWN:
                RespawnToTown(p);
                break;
            case OP::ITEM_USE: {
                    const auto request = *parseBody<ItemUseRequestBody>(p->GetPtr());
                    const auto result = stateManager->UseItem(p->GetSessionID(), request);
                    auto response = writer->WriteItemUseResponse(result);
                    if (response) networkSender->SendDataUnique(p->GetSessionID(), std::move(response));
                }
                break;
            case OP::EQUIPMENT_GET:  {
                    const auto request = *parseBody<EquipmentGetRequestBody>(p->GetPtr());
                    const auto result = stateManager->Equipment(p->GetSessionID(), request.requestID, request.ownershipEpoch, false);
                    auto response = writer->WriteEquipmentResponse(result);
                    if (response) networkSender->SendDataUnique(p->GetSessionID(), std::move(response));
                }
                break;
            case OP::EQUIPMENT_SET: {
                    const auto request = *parseBody<EquipmentSetRequestBody>(p->GetPtr());
                    if (request.slot > 2) break;
                    const auto result = stateManager->Equipment(p->GetSessionID(), request.requestID, request.ownershipEpoch, true, request.slot, request.itemID);
                    auto response = writer->WriteEquipmentResponse(result);
                    if (response) networkSender->SendDataUnique(p->GetSessionID(), std::move(response));
                }
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

    void NonZoneHandler::Disconnect(uint64_t sessionID) {
        stateManager->Disconnect(sessionID);
    }

    void NonZoneHandler::RequestProfileRename(uint64_t sessionID, uint32_t profileId, const char* name, uint16_t nameLen) {
        if (profileId == INVALID_PROFILE_ID || name == nullptr || nameLen == 0 || nameLen >= MAX_CHARNAME_LEN) {
            errorLogger->LogError("non zone handler", "profile rename invalid", "session", sessionID, "profile_id", profileId, "nameLen", nameLen);
            return;
        }
        Message* msg = messagePool->Acquire();
        if (msg == nullptr) {
            return;
        }
        auto st = reinterpret_cast<MsgStruct<MsgProfileRenameBody>*>(msg->GetBuffer());
        st->header.sessionID = sessionID;
        st->header.messageType = MSG_PROFILE_RENAME;
        st->body.profileId = profileId;
        std::memset(st->body.name, 0, sizeof(st->body.name));
        std::memcpy(st->body.name, name, nameLen);
        msg->SetLength(sizeof(MsgStruct<MsgProfileRenameBody>));
        messageQueue->EnqueueMessage(msg);
        messagePool->Return(msg);
    }

    void NonZoneHandler::GetInventory(IPacketView* p) {
        auto charID = stateManager->GetCharacterID(p->GetSessionID());
        if (charID <= 0)
            return ;
        
        Message* msg = messagePool->Acquire();
        if (msg == nullptr) {
            return;
        }
        MsgStruct<MsgInventoryReqBody>* st = reinterpret_cast<MsgStruct<MsgInventoryReqBody>*>(msg->GetBuffer());
        
        st->header.sessionID = p->GetSessionID();
        st->header.messageType = MSG_INVENTORY_REQ;
        st->body.characterID = charID;
        msg->SetLength(sizeof(MsgStruct<MsgInventoryReqBody>));
        messageQueue->EnqueueMessage(msg);
        messagePool->Return(msg);
    }

    void NonZoneHandler::WalletRequest(IPacketView* p) {
        if (p->GetLength() != sizeof(PacketStruct<WalletRequestBody>)) return;
        const auto* header = parseHeader(p->GetPtr());
        const auto request = *parseBody<WalletRequestBody>(p->GetPtr());
        if (header->magic != MAGIC || header->opcode != OP::WALLET_REQUEST || header->length != p->GetLength()
            || IsSimulationFlag(header->flags) || request.requestID == 0) return;
        const uint64_t characterID = stateManager->GetFieldCharacterID(p->GetSessionID());
        if (characterID == 0) return;

        Message* gold = messagePool->Acquire();
        if (gold != nullptr) {
            auto* st = reinterpret_cast<MsgStruct<MsgCurrencyReqBody>*>(gold->GetBuffer());
            st->header = { p->GetSessionID(), MSG_CURRENCY_REQ };
            st->body = { characterID, request.requestID };
            gold->SetLength(sizeof(*st));
            const bool queued = messageQueue->EnqueueMessage(gold);
            messagePool->Return(gold);
            if (!queued) gold = nullptr;
        }
        if (gold == nullptr) {
            MsgCurrencyResBody failed{};
            failed.requestID = request.requestID;
            failed.characterID = characterID;
            auto response = writer->WriteWalletGoldResponse(&failed);
            if (response && stateManager->GetFieldCharacterID(p->GetSessionID()) == characterID)
                networkSender->SendDataUnique(p->GetSessionID(), std::move(response));
        }

        Message* diamond = messagePool->Acquire();
        if (diamond != nullptr) {
            auto* st = reinterpret_cast<MsgStruct<MsgDiamondReqBody>*>(diamond->GetBuffer());
            st->header = { p->GetSessionID(), MSG_DIAMOND_REQ };
            st->body = { characterID, request.requestID };
            diamond->SetLength(sizeof(*st));
            const bool queued = messageQueue->EnqueueMessage(diamond);
            messagePool->Return(diamond);
            if (!queued) diamond = nullptr;
        }
        if (diamond == nullptr) {
            MsgDiamondResBody failed{};
            failed.requestID = request.requestID;
            failed.characterID = characterID;
            auto response = writer->WriteWalletDiamondResponse(&failed);
            if (response && stateManager->GetFieldCharacterID(p->GetSessionID()) == characterID)
                networkSender->SendDataUnique(p->GetSessionID(), std::move(response));
        }
    }

    void NonZoneHandler::Chat(IPacketView* p) {
        auto body = parseBody<ChatRequestBody>(p->GetPtr());
        uint8_t* startPtr = p->GetPtr() + sizeof(PacketStruct<ChatRequestBody>);
        auto session = p->GetSessionID();
        auto zoneID = stateManager->GetZoneID(session);
        auto zone = stateManager->GetZone(zoneID);
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
    // NonZone 스레드에서 처리하므로 zone 틱과 무관하고, write-through 캐시라
    // DB를 기다릴 일이 없어 동기 조회로 충분하다.
    void NonZoneHandler::GetProfileBatch(IPacketView* p) {
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

    void NonZoneHandler::ZoneChange(IPacketView* p) {
        ZoneChangeResult result;
        const auto session = p->GetSessionID();
        const bool success = p->GetLength() == sizeof(PacketStruct<ZoneChangeBody>)
            && stateManager->ChangeZone(session, parseBody<ZoneChangeBody>(p->GetPtr())->op, result);
        // ChangeZone releases the shard and all Zone locks before network transmission.
        auto response = success
            ? writer->WriteZoneChangeSucess(result.zoneID, result.chatID, result.internalID,
                result.x, result.y, result.generation, result.ownershipEpoch)
            : writer->WriteZoneChangeFailed();
        if (response)
            networkSender->SendDataUnique(session, std::move(response));
        if (success)
            stateManager->SendBootstrap(session, result.ownershipEpoch);
    }

    void NonZoneHandler::RespawnToTown(IPacketView* p) {
        ZoneChangeResult result;
        const auto session = p->GetSessionID();
        const bool validHeader = p->GetLength() == sizeof(PacketStruct<RespawnToTownBody>)
            && parseHeader(p->GetPtr())->magic == MAGIC
            && parseHeader(p->GetPtr())->opcode == OP::RESPAWN_TO_TOWN
            && parseHeader(p->GetPtr())->length == p->GetLength()
            && !IsSimulationFlag(parseHeader(p->GetPtr())->flags);
        const bool success = validHeader
            && stateManager->RespawnToTown(session, *parseBody<RespawnToTownBody>(p->GetPtr()), result);
        auto response = success
            ? writer->WriteZoneChangeSucess(result.zoneID, result.chatID, result.internalID,
                result.x, result.y, result.generation, result.ownershipEpoch)
            : writer->WriteZoneChangeFailed();
        if (response)
            networkSender->SendDataUnique(session, std::move(response));
        if (success)
            stateManager->SendBootstrap(session, result.ownershipEpoch);
    }
}
