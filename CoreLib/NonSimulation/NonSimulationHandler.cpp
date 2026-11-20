#include "pch.h"
#include "NonSimulationHandler.h"

#include "../Cache/CacheCommandSender.h"
#include "../Packet/PacketTypes.h"
#include "../Interface/IPacketView.h"
#include "../Interface/ISessionAuth.h"
#include "../Interface/INetworkSender.h"
#include "../Packet/PacketWriter.h"
#include "../World/ZoneState.h"
#include "../World/Interface/IZonePacketQueue.h"
#include "../World/Interface/IZoneRequestQueue.h"
#include "../Cache/MessageTypes.h"
#include "../World/SessionManager.h"
#include "../World/LobbyZone.h"
#include "ChatThreadPool.h"
#include "../Interface/IProfileCache.h"
#include "../LoggerGlobal.h"
#include "../Packet/PacketConfig.h"

namespace Core {
    void NonSimulationHandler::Initialize(INetworkSender* sender, ISessionAuth* s, PacketWriter* p, const CacheCommandSender* commands, SessionManager* manager, LobbyZone* lobby, ChatThreadPool* c, IProfileCache* pc, IZonePacketQueue& packets, IZoneRequestQueue& requests) {
        zonePackets = &packets;
        zoneRequests = &requests;
        networkSender = sender;
        auth = s;
        writer = p;
        cacheCommands = commands;
        sessionManager = manager;
        lobbyZone = lobby;
        chat = c;
        profileCache = pc;
    }

    bool NonSimulationHandler::IsReady() const {
        if (networkSender == nullptr) {
            sysLogger->LogError("non simulation handler", "network sender not initialized");
            return false;
        }
        if (auth == nullptr) {
            sysLogger->LogError("non simulation handler", "session auth not initialized");
            return false;
        }
        if (writer == nullptr) {
            sysLogger->LogError("non simulation handler", "packet writer not initialized");
            return false;
        }
        if (cacheCommands == nullptr) {
            sysLogger->LogError("non simulation handler", "cache command sender not initialized");
            return false;
        }
        if (sessionManager == nullptr) {
            sysLogger->LogError("non simulation handler", "session manager not initialized");
            return false;
        }
        if (lobbyZone == nullptr) {
            sysLogger->LogError("non simulation handler", "lobby zone not initialized");
            return false;
        }
        if (zonePackets == nullptr) {
            sysLogger->LogError("non simulation handler", "zone packet queue not initialized");
            return false;
        }
        if (zoneRequests == nullptr) {
            sysLogger->LogError("non simulation handler", "zone request queue not initialized");
            return false;
        }
        return true;
    }

    void NonSimulationHandler::Process(std::unique_ptr<IPacketView, PacketViewDeleter> packet) {
        if (!packet) return;
        const auto opcode = packet->GetOpcode();
        if (opcode == OP::ITEM_USE || opcode == OP::EQUIPMENT_GET || opcode == OP::EQUIPMENT_SET) {
            ForwardZoneRequest(std::move(packet));
            return;
        }
        auto* p = packet.get();
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
                    const auto route = sessionManager->GetBootstrapRoute(p->GetSessionID(), body->ownershipEpoch);
                    if (route) zoneRequests->TryEnqueueBootstrap(route->zoneID, p->GetSessionID(), route->ownershipEpoch, false);
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
        const auto request = sessionManager->BeginDisconnect(sessionID);
        if (!request) return;
        if (request->zoneID != 0) {
            if (!zoneRequests->TryEnqueueDisconnect(request->zoneID, sessionID, request->ownershipEpoch))
                errorLogger->LogError("non simulation handler", "could not queue zone disconnect", "sessionID", sessionID, "zoneID", request->zoneID);
            return;
        }
        CharacterState character;
        const bool save = request->chatID != 0 && lobbyZone->EmigrateChar(sessionID, character);
        if (request->chatID == 0) lobbyZone->Disconnect(sessionID);
        if (!sessionManager->CompleteLobbyDisconnect(sessionID, request->ownershipEpoch))
            errorLogger->LogError("non simulation handler", "lobby disconnect metadata changed", "sessionID", sessionID);
        if (request->chatID != 0) chat->DeleteChatSession(sessionID, 0);
        if (save) cacheCommands->EnqueueCharacterStateUpdate(sessionID, character);
    }

    void NonSimulationHandler::RequestProfileRename(uint64_t sessionID, uint32_t profileId, const char* name, uint16_t nameLen) {
        if (profileId == INVALID_PROFILE_ID || name == nullptr || nameLen == 0 || nameLen >= MAX_CHARNAME_LEN) {
            errorLogger->LogError("non zone handler", "profile rename invalid", "session", sessionID, "profile_id", profileId, "nameLen", nameLen);
            return;
        }
        cacheCommands->EnqueueProfileRename(sessionID, profileId, {name, nameLen});
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

    void NonSimulationHandler::ForwardZoneRequest(std::unique_ptr<IPacketView, PacketViewDeleter> packet) {
        const auto sessionID = packet->GetSessionID();
        const auto zoneID = sessionManager->GetZoneID(sessionID);
        const auto opcode = packet->GetOpcode();
        if (opcode == OP::ITEM_USE) {
            const auto request = *parseBody<ItemUseRequestBody>(packet->GetPtr());
            ItemUseResponseBody result{};
            if (sessionManager->CheckItemUse(sessionID, zoneID, request, result)) {
                if (zonePackets->TryEnqueuePacket(zoneID, std::move(packet))) return;
                result.status = 6; // Existing Busy response; no item was consumed.
                errorLogger->LogWarn("non simulation handler", "item request queue full", "sessionID", sessionID, "zoneID", zoneID);
            }
            auto response = writer->WriteItemUseResponse(result);
            if (response) networkSender->SendDataUnique(sessionID, std::move(response));
            return;
        }
        uint64_t requestID, epoch;
        uint32_t itemID = 0;
        uint8_t slot = 0;
        const bool write = opcode == OP::EQUIPMENT_SET;
        if (write) {
            const auto request = *parseBody<EquipmentSetRequestBody>(packet->GetPtr());
            if (request.slot > 2) return;
            requestID = request.requestID; epoch = request.ownershipEpoch; slot = request.slot; itemID = request.itemID;
        } else {
            const auto request = *parseBody<EquipmentGetRequestBody>(packet->GetPtr());
            requestID = request.requestID; epoch = request.ownershipEpoch;
        }
        EquipmentResponseBody result{};
        if (sessionManager->CheckEquipment(sessionID, zoneID, requestID, epoch, write, slot, itemID, result)) {
            if (zonePackets->TryEnqueuePacket(zoneID, std::move(packet))) return;
            result.status = static_cast<uint8_t>(EquipmentStatus::Busy);
            errorLogger->LogWarn("non simulation handler", "equipment request queue full", "sessionID", sessionID, "zoneID", zoneID);
        }
        auto response = writer->WriteEquipmentResponse(result);
        if (response) networkSender->SendDataUnique(sessionID, std::move(response));
    }
}
