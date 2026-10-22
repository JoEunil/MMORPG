#include "pch.h"
#include "NonZoneHandler.h"

#include "PacketTypes.h"
#include "IPacketView.h"
#include "ISessionAuth.h"
#include "IIOCP.h"
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
    static IIOCP* iocp;
    static PacketWriter* writer;\
    static StateManager* stateManager;
    // 콜백에서 쓰기 위함

    void NonZoneHandler::Initialize(IIOCP* i, ISessionAuth* s, PacketWriter* p, MessagePool* m, IMessageQueue* mq, StateManager* manager, LobbyZone* lobby, ChatThreadPool* c) {
        iocp = i;
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
        if (iocp == nullptr) return false;
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
            case OP::INVENTORY_REQ:
                GetInventory(p);
                break;
            case OP::BAZAAR_BUY:
                BazaarBuy(p);
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

    static void ResponseSession(uint64_t sessionID, uint8_t resStatus, uint64_t userID) {
        if (!stateManager->CompleteAuth(sessionID, userID, resStatus == RES_STATUS::SUCCESS))
            return;
        auto res = writer->WriteAuthResponse(resStatus);
        if (res == nullptr)
            return;
        iocp->SendDataUnique(sessionID, std::move(res));
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

    void NonZoneHandler::BazaarBuy(IPacketView* p) {
        if (p->GetLength() != sizeof(PacketStruct<BazaarBuyRequestBody>))
            return;
        const auto* header = parseHeader(p->GetPtr());
        const auto request = *parseBody<BazaarBuyRequestBody>(p->GetPtr());
        if (header->magic != MAGIC || header->opcode != OP::BAZAAR_BUY
            || header->length != p->GetLength() || header->flags != 0
            || request.requestID == 0 || request.listingID == 0)
            return;
        const uint64_t characterID = stateManager->GetFieldCharacterID(p->GetSessionID());
        if (characterID == 0)
            return;
        Message* msg = messagePool->Acquire();
        if (msg != nullptr) {
            auto* st = reinterpret_cast<MsgStruct<MsgBazaarBuyBody>*>(msg->GetBuffer());
            st->header = { p->GetSessionID(), MSG_BAZAAR_BUY };
            st->body = { characterID, request.listingID, request.requestID };
            msg->SetLength(sizeof(*st));
            const bool queued = messageQueue->EnqueueMessage(msg);
            messagePool->Return(msg);
            if (queued)
                return;
        }
        MsgBazaarBuyResBody failed{};
        failed.requestID = request.requestID;
        failed.listingID = request.listingID;
        failed.resStatus = 5;
        if (stateManager->GetFieldCharacterID(p->GetSessionID()) != characterID)
            return;
        auto response = writer->WriteBazaarBuyResponse(&failed);
        if (response)
            iocp->SendDataUnique(p->GetSessionID(), std::move(response));
    }

    void NonZoneHandler::BazaarCheckOutbox(IPacketView* p) {
        if (p->GetLength() != sizeof(PacketStruct<BazaarCheckOutboxRequestBody>))
            return;
        const auto* header = parseHeader(p->GetPtr());
        const auto request = *parseBody<BazaarCheckOutboxRequestBody>(p->GetPtr());
        if (header->magic != MAGIC || header->opcode != OP::BAZAAR_CHECK_OUTBOX
            || header->length != p->GetLength() || header->flags != 0 || request.requestID == 0)
            return;
        const uint64_t characterID = stateManager->GetFieldCharacterID(p->GetSessionID());
        if (characterID == 0)
            return;
        Message* msg = messagePool->Acquire();
        if (msg != nullptr) {
            auto* st = reinterpret_cast<MsgStruct<MsgBazaarCheckOutboxBody>*>(msg->GetBuffer());
            st->header = { p->GetSessionID(), MSG_BAZAAR_CHECK_OUTBOX };
            st->body = { characterID, request.requestID };
            msg->SetLength(sizeof(*st));
            const bool queued = messageQueue->EnqueueMessage(msg);
            messagePool->Return(msg);
            if (queued)
                return;
        }
        MsgBazaarCheckOutboxResBody failed{};
        failed.requestID = request.requestID;
        if (stateManager->GetFieldCharacterID(p->GetSessionID()) != characterID)
            return;
        auto response = writer->WriteBazaarCheckOutboxResponse(&failed);
        if (response)
            iocp->SendDataUnique(p->GetSessionID(), std::move(response));
    }

    void NonZoneHandler::BazaarCancelListing(IPacketView* p) {
        if (p->GetLength() != sizeof(PacketStruct<BazaarCancelListingRequestBody>)) return;
        const auto* header = parseHeader(p->GetPtr());
        const auto request = *parseBody<BazaarCancelListingRequestBody>(p->GetPtr());
        if (header->magic != MAGIC || header->opcode != OP::BAZAAR_CANCEL_LISTING
            || header->length != p->GetLength() || header->flags != 0
            || request.requestID == 0 || request.listingID == 0) return;
        const uint64_t characterID = stateManager->GetFieldCharacterID(p->GetSessionID());
        if (characterID == 0) return;
        Message* msg = messagePool->Acquire();
        if (msg != nullptr) {
            auto* st = reinterpret_cast<MsgStruct<MsgBazaarCancelListingBody>*>(msg->GetBuffer());
            st->header = { p->GetSessionID(), MSG_BAZAAR_CANCEL_LISTING };
            st->body = { characterID, request.listingID, request.requestID };
            msg->SetLength(sizeof(*st));
            const bool queued = messageQueue->EnqueueMessage(msg);
            messagePool->Return(msg);
            if (queued) return;
        }
        MsgBazaarCancelListingResBody failed{};
        failed.requestID = request.requestID;
        failed.listingID = request.listingID;
        failed.characterID = characterID;
        failed.resStatus = 5;
        if (stateManager->GetFieldCharacterID(p->GetSessionID()) != characterID) return;
        auto response = writer->WriteBazaarCancelListingResponse(&failed);
        if (response) iocp->SendDataUnique(p->GetSessionID(), std::move(response));
    }

    void NonZoneHandler::BazaarClaim(IPacketView* p) {
        if (p->GetLength() != sizeof(PacketStruct<BazaarClaimRequestBody>))
            return;
        const auto* header = parseHeader(p->GetPtr());
        const auto request = *parseBody<BazaarClaimRequestBody>(p->GetPtr());
        if (header->magic != MAGIC || header->opcode != OP::BAZAAR_CLAIM
            || header->length != p->GetLength() || header->flags != 0
            || request.requestID == 0 || request.listingID == 0)
            return;
        const uint64_t characterID = stateManager->GetFieldCharacterID(p->GetSessionID());
        if (characterID == 0)
            return;
        Message* msg = messagePool->Acquire();
        if (msg != nullptr) {
            auto* st = reinterpret_cast<MsgStruct<MsgBazaarClaimBody>*>(msg->GetBuffer());
            st->header = { p->GetSessionID(), MSG_BAZAAR_CLAIM };
            st->body = { characterID, request.listingID, request.requestID };
            msg->SetLength(sizeof(*st));
            const bool queued = messageQueue->EnqueueMessage(msg);
            messagePool->Return(msg);
            if (queued)
                return;
        }
        MsgBazaarClaimResBody failed{};
        failed.requestID = request.requestID;
        failed.listingID = request.listingID;
        if (stateManager->GetFieldCharacterID(p->GetSessionID()) != characterID)
            return;
        auto response = writer->WriteBazaarClaimResponse(&failed);
        if (response)
            iocp->SendDataUnique(p->GetSessionID(), std::move(response));
    }

    void NonZoneHandler::BazaarMyList(IPacketView* p) {
        if (p->GetLength() != sizeof(PacketStruct<BazaarMyListRequestBody>))
            return;
        const auto* header = parseHeader(p->GetPtr());
        const auto request = *parseBody<BazaarMyListRequestBody>(p->GetPtr());
        if (header->length != p->GetLength() || header->flags != 0 || request.requestID == 0)
            return;
        const uint64_t characterID = stateManager->GetFieldCharacterID(p->GetSessionID());
        if (characterID == 0)
            return;
        Message* msg = messagePool->Acquire();
        if (msg != nullptr) {
            auto* st = reinterpret_cast<MsgStruct<MsgBazaarMyListBody>*>(msg->GetBuffer());
            st->header = { p->GetSessionID(), MSG_BAZAAR_MY_LIST };
            st->body = { request.requestID, characterID };
            msg->SetLength(sizeof(*st));
            const bool queued = messageQueue->EnqueueMessage(msg);
            messagePool->Return(msg);
            if (queued)
                return;
        }
        MsgBazaarMyListResBody failed{};
        failed.requestID = request.requestID;
        failed.characterID = characterID;
        if (stateManager->GetFieldCharacterID(p->GetSessionID()) != characterID)
            return;
        auto response = writer->WriteBazaarMyListResponse(&failed);
        if (response)
            iocp->SendDataUnique(p->GetSessionID(), std::move(response));
    }

    void NonZoneHandler::BazaarSearch(IPacketView* p) {
        if (p->GetLength() != sizeof(PacketStruct<BazaarSearchRequestBody>))
            return;
        const auto* header = parseHeader(p->GetPtr());
        const auto request = *parseBody<BazaarSearchRequestBody>(p->GetPtr());
        if (header->length != p->GetLength() || header->flags != 0
            || request.requestID == 0 || request.itemType < 1 || request.itemType > 3
            || !stateManager->IsFieldSession(p->GetSessionID()))
            return;

        Message* msg = messagePool->Acquire();
        if (msg != nullptr) {
            auto* st = reinterpret_cast<MsgStruct<MsgBazaarSearchBody>*>(msg->GetBuffer());
            st->header = { p->GetSessionID(), MSG_BAZAAR_SEARCH };
            st->body = { request.requestID, request.itemType, request.page };
            msg->SetLength(sizeof(*st));
            const bool queued = messageQueue->EnqueueMessage(msg);
            messagePool->Return(msg);
            if (queued)
                return;
        }
        MsgBazaarSearchResBody failed{};
        failed.requestID = request.requestID;
        auto response = writer->WriteBazaarSearchResponse(&failed);
        if (response)
            iocp->SendDataUnique(p->GetSessionID(), std::move(response));
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
        iocp->SendDataUnique(session, std::move(res));
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
            iocp->SendDataUnique(session, std::move(response));
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
            && parseHeader(p->GetPtr())->flags == 0;
        const bool success = validHeader
            && stateManager->RespawnToTown(session, *parseBody<RespawnToTownBody>(p->GetPtr()), result);
        auto response = success
            ? writer->WriteZoneChangeSucess(result.zoneID, result.chatID, result.internalID,
                result.x, result.y, result.generation, result.ownershipEpoch)
            : writer->WriteZoneChangeFailed();
        if (response)
            iocp->SendDataUnique(session, std::move(response));
        if (success)
            stateManager->SendBootstrap(session, result.ownershipEpoch);
    }
}
