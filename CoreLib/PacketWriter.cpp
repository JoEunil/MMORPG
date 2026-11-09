#include "pch.h"
#include "PacketWriter.h"
#include "IPacket.h"
#include "Config.h"
#include "ItemData.h"
#include "LoggerGlobal.h"
#include <cstddef>
namespace Core {
    void PacketWriter::Initialize(IPacketPoolManager* poolManager) {
        packetPoolManager = poolManager;
    }

    bool PacketWriter::IsReady() {
        if (packetPoolManager == nullptr) {
            sysLogger->LogError("packet writer", "packetPoolManager not initialized");
            return false;
        }
        return true;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::AcquireUniquePacket(PacketPoolType type) {
        auto packet = packetPoolManager->AcquireUnique(type);
        if (!packet)
            return nullptr;
        packet->Clear();
        return packet;
    }

    std::shared_ptr<IPacket> PacketWriter::AcquireSharedPacket(PacketPoolType type) {
        auto packet = packetPoolManager->Acquire(type);
        if (!packet)
            return nullptr;
        packet->Clear();
        return packet;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteItemUseResponse(const ItemUseResponseBody& body) {
		auto packetPtr = AcquireUniquePacket(PacketPoolType::Normal);
        if(packetPtr == nullptr)
            return nullptr;
		auto* packet = InitializePacketHeader<ItemUseResponseBody>(*packetPtr, OP::ITEM_USE_RES, 0);
		packet->body = body;
        return packetPtr;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteParryResponse(const ParryResponseBody& body) {
        auto packetPtr = AcquireUniquePacket(PacketPoolType::Normal);
        if (packetPtr == nullptr)
            return nullptr;
        auto* packet = InitializePacketHeader<ParryResponseBody>(*packetPtr, OP::PARRY_RES, FLAG_SIMULATION);
        packet->body = body;
        return packetPtr;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteItemReward(const ItemRewardBody& body) {
        auto packetPtr = AcquireUniquePacket(PacketPoolType::Normal);
        if (packetPtr == nullptr)
            return nullptr;
        auto* packet = InitializePacketHeader<ItemRewardBody>(*packetPtr, OP::ITEM_REWARD, 0);
        packet->body = body;
        return packetPtr;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteGoldReward(uint8_t status) {
        auto packetPtr = AcquireUniquePacket(PacketPoolType::Normal);
        if (!packetPtr) return nullptr;
        auto* packet = InitializePacketHeader<uint8_t>(*packetPtr, OP::GOLD_REWARD, 0);
        packet->body = status;
        return packetPtr;
    }

    std::unique_ptr<IPacket, PacketDeleter>  PacketWriter::WriteAuthResponse(uint8_t resStatus) {
        auto packetPtr = AcquireUniquePacket(PacketPoolType::Normal);
        if (packetPtr == nullptr) {
            return nullptr;
        }
        auto* packet = InitializePacketHeader<AuthResponseBody>(*packetPtr, OP::AUTH_RESPONSE, 0);
        packet->body.resStatus = resStatus;
        return packetPtr;
    }

    std::unique_ptr<IPacket, PacketDeleter>  PacketWriter::WriteCharacterListResponse(const MsgCharacterListResBody* body) {
        auto packetPtr = AcquireUniquePacket(PacketPoolType::Big);
        if (packetPtr == nullptr) {
            return nullptr;
        }
        auto* packet = InitializePacketHeader<CharacterListResponseBody>(*packetPtr, OP::CHARACTER_LIST_RESPONSE, 0);
        packet->body.resStatus = body->resStatus;
        packet->body.count = body->count;
        for (int i = 0; i < body->count; i++)
        {
            packet->body.characters[i].characterID = body->characters[i].characterID;
            packet->body.characters[i].level = body->characters[i].level;
            std::memcpy(packet->body.characters[i].name, body->characters[i].name, MAX_CHARNAME_LEN);
        }
        return packetPtr;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteEnterWorldResponse(const MsgCharacterStateResBody* body) {
        auto packetPtr = AcquireUniquePacket(PacketPoolType::Normal);
        if (packetPtr == nullptr) {
            return nullptr;
        }
        auto* packet = InitializePacketHeader<EnterWorldResponseBody>(*packetPtr, OP::ENTER_WORLD_RESPONSE, 0);
        packet->body.resStatus = body->resStatus;
        std::memcpy(packet->body.name, body->name, sizeof(packet->body.name));
        packet->body.currentZone = body->currentZone;
        packet->body.hp = body->hp;
        packet->body.mp = body->mp;
        packet->body.maxHp = body->maxHp;
        packet->body.maxMp = body->maxMp;
        packet->body.attack = body->attack;
        packet->body.exp = body->exp;
        packet->body.level = body->level;
        packet->body.dir = body->dir;
        packet->body.startX = body->startX;
        packet->body.startY = body->startY;
        return packetPtr;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteInventoryResponse(const MsgInventoryResBody* body) {
        auto packetPtr = AcquireUniquePacket(PacketPoolType::Normal);
        if (packetPtr == nullptr) {
            return nullptr;
        }

        auto* packet = InitializePacketHeader<InventoryResBody>(*packetPtr, OP::INVENTORY_RES, 0);
        packet->body = {};
        if (body->resStatus != RES_STATUS::SUCCESS || body->itemCount > MAX_INVENTORY_ITEMS) {
            packet->body.resStatus = RES_STATUS::FAILED;
            return packetPtr;
        }

        packet->body.resStatus = RES_STATUS::SUCCESS;
        packet->body.itemCount = body->itemCount;
        for (int i = 0; i < body->itemCount; i++)
        {
            packet->body.items[i].itemID = body->items[i].itemID;
            packet->body.items[i].quantity = body->items[i].quantity;
            packet->body.items[i].slot = body->items[i].slot;
        }

        return packetPtr;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteInventoryUpdateResponse(const MsgInventoryUpdateResBody* body) {
        auto packetPtr = AcquireUniquePacket(PacketPoolType::Normal);
        if (packetPtr == nullptr) {
            return nullptr;
        }

        auto* packet = InitializePacketHeader<InventoryUpdateResBody>(*packetPtr, OP::INVENTORY_UPDATE_RES, 0);
        packet->body.resStatus = body->resStatus;
        packet->body.itemID = body->itemID;
        packet->body.itemQuantity = body->itemQuantity;
        packet->body.slot = body->slot;

        return packetPtr;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::GetChatWhisperPacket(uint64_t sender, uint32_t senderProfileId, const std::string& message) {
        auto packetPtr = AcquireUniquePacket(PacketPoolType::Normal);
        if (packetPtr == nullptr) {
            return nullptr;
        }
        auto* packet = InitializePacketHeader<ChatWhisperBody>(*packetPtr, OP::CHAT_WHISPER, 0, sizeof(PacketHeader) + sizeof(ChatWhisperBody) + message.length());
        packet->body.senderProfileId = senderProfileId;
        packet->body.senderChatID = sender;
        packet->body.messageLen = message.length();
        uint8_t* msgStart = reinterpret_cast<uint8_t*>(packetPtr->GetBuffer()) + sizeof(PacketStruct<ChatWhisperBody>);
        memcpy(msgStart, message.data(), message.length());
        return packetPtr;
    }

    std::shared_ptr<IPacket> PacketWriter::GetInitialChatBatchPacket(CHAT_SCOPE scope) {
        auto packetPtr = AcquireSharedPacket(PacketPoolType::Big);
        if (packetPtr == nullptr) {
            return nullptr;
        }
        auto* packet = InitializePacketHeader<ChatBatchNotifyBody>(*packetPtr, OP::CHAT_BROADCAST, 0);
        packet->body.scope = scope;
        packet->body.chatCnt = 0;
        packet->body.totalMessageLength = 0;
        return packetPtr;
    }

    uint16_t PacketWriter::WriteChatBatchPacketField(std::shared_ptr<IPacket> packetPtr, uint64_t sender, uint32_t senderProfileId, const std::string& message) {
        auto packet = reinterpret_cast<PacketStruct<ChatBatchNotifyBody>*>(packetPtr->GetBuffer());
        packet->header.length += message.length();
        packetPtr->SetLength(packet->header.length);

        uint8_t* msgStart = reinterpret_cast<uint8_t*>(packetPtr->GetBuffer()) + sizeof(PacketStruct<ChatBatchNotifyBody>) + packet->body.totalMessageLength;
        memcpy(msgStart, message.data(), message.length());

        packet->body.entities[packet->body.chatCnt].senderChatID = sender;
        packet->body.entities[packet->body.chatCnt].offset = packet->body.totalMessageLength;
        packet->body.entities[packet->body.chatCnt].messageLength = message.length();
        packet->body.entities[packet->body.chatCnt].senderProfileId = senderProfileId;
        packet->body.chatCnt++;
        packet->body.totalMessageLength += message.length();
        return packet->body.chatCnt;
    }

    std::shared_ptr<IPacket> PacketWriter::GetDeltaHeader(uint16_t zoneID, uint64_t generation) {
        auto packetPtr = AcquireSharedPacket(PacketPoolType::Normal);
        if (!packetPtr) {
            return nullptr;
        }
        auto* packet = InitializePacketHeader<DeltaSnapshotBody>(*packetPtr, OP::ZONE_DELTA_UPDATE_BROADCAST, FLAG_SIMULATION,
            sizeof(PacketHeader) + offsetof(DeltaSnapshotBody, updates));
        packet->body.count = 0;
        packet->body.zoneID = zoneID;
        packet->body.zoneGeneration = generation;
        return packetPtr;
    }

    std::shared_ptr<IPacket> PacketWriter::GetFullHeader(uint16_t zoneID, uint64_t generation) {
        auto packetPtr = AcquireSharedPacket(PacketPoolType::Normal);
        if (packetPtr == nullptr) {
            return nullptr;
        }
        auto* packet = InitializePacketHeader<FullSnapshotBody>(*packetPtr, OP::ZONE_FULL_STATE_BROADCAST, FLAG_SIMULATION,
            sizeof(PacketHeader) + offsetof(FullSnapshotBody, states));
        packet->body.count = 0;
        packet->body.zoneID = zoneID;
        packet->body.zoneGeneration = generation;
        packet->body.ownershipEpoch = 0;
        packet->body.bootstrapID = 0;
        packet->body.bootstrapEnd = 0;
        return packetPtr;
    }

    std::shared_ptr<IPacket> PacketWriter::GetBootstrapFullHeader(bool monsters, uint16_t zoneID, uint64_t generation, uint64_t ownershipEpoch, uint64_t bootstrapID) {
        auto packetPtr = AcquireSharedPacket(PacketPoolType::Normal);
        if (!packetPtr) return nullptr;
        if (monsters) {
            auto* packet = InitializePacketHeader<MonsterFullSnapshotBody>(*packetPtr, OP::MONSTER_FULL_STATE_BROADCAST, FLAG_SIMULATION,
                sizeof(PacketHeader) + offsetof(MonsterFullSnapshotBody, states));
            auto* body = &packet->body;
            body->count = 0; body->zoneID = zoneID; body->zoneGeneration = generation;
            body->ownershipEpoch = ownershipEpoch; body->bootstrapID = bootstrapID; body->bootstrapEnd = 1;
        } else {
            auto* packet = InitializePacketHeader<FullSnapshotBody>(*packetPtr, OP::ZONE_FULL_STATE_BROADCAST, FLAG_SIMULATION,
                sizeof(PacketHeader) + offsetof(FullSnapshotBody, states));
            auto* body = &packet->body;
            body->count = 0; body->zoneID = zoneID; body->zoneGeneration = generation;
            body->ownershipEpoch = ownershipEpoch; body->bootstrapID = bootstrapID; body->bootstrapEnd = 1;
        }
        return packetPtr;
    }

    bool PacketWriter::WriteFullField(std::shared_ptr<IPacket> packetPtr, const CharacterState& state){
        if (packetPtr->GetLength() + sizeof(FullStateField) > packetPtr->GetCapacity())
            return false;
        auto fields = reinterpret_cast<FullStateField*>(packetPtr->GetBuffer());
        packetPtr->SetLength(packetPtr->GetLength() + sizeof(FullStateField));
        packetPtr->Count();
        auto& slot = fields[packetPtr->GetCount() -1];
        slot.zoneInternalID = state.zoneInternalID;
        slot.profileId = state.profileId;
        slot.profileVersion = state.profileVersion;
        slot.hp = state.hp;
        slot.mp = state.mp;
        slot.maxHp = state.maxHp;
        slot.maxMp = state.maxMp;
        slot.exp = state.exp;
        slot.level = state.level;
        slot.dir = state.dir;
        slot.x = state.x;
        slot.y = state.y;
        return true;
    }

    std::shared_ptr<IPacket> PacketWriter::GetMonsterDeltaHeader(uint16_t zoneID, uint64_t generation) {
        auto packetPtr = AcquireSharedPacket(PacketPoolType::Normal);
        if (!packetPtr) {
            return nullptr;
        }
        auto* packet = InitializePacketHeader<MonsterDeltaSnapshotBody>(*packetPtr, OP::MONSTER_DELTA_UPDATE_BROADCAST, FLAG_SIMULATION,
            sizeof(PacketHeader) + offsetof(MonsterDeltaSnapshotBody, updates));
        packet->body.count = 0;
        packet->body.zoneID = zoneID;
        packet->body.zoneGeneration = generation;
        return packetPtr;
    }

    std::shared_ptr<IPacket> PacketWriter::GetMonsterFullHeader(uint16_t zoneID, uint64_t generation) {
        auto packetPtr = AcquireSharedPacket(PacketPoolType::Normal);
        if (packetPtr == nullptr) {
            return nullptr;
        }
        auto* packet = InitializePacketHeader<MonsterFullSnapshotBody>(*packetPtr, OP::MONSTER_FULL_STATE_BROADCAST, FLAG_SIMULATION,
            sizeof(PacketHeader) + offsetof(MonsterFullSnapshotBody, states));
        packet->body.count = 0;
        packet->body.zoneID = zoneID;
        packet->body.zoneGeneration = generation;
        packet->body.ownershipEpoch = 0;
        packet->body.bootstrapID = 0;
        packet->body.bootstrapEnd = 0;
        return packetPtr;
    }

    bool PacketWriter::WriteMonsterFullField(std::shared_ptr<IPacket> packetPtr, const MonsterState& state) {
        if (packetPtr->GetLength() + sizeof(MonsterFullField) > packetPtr->GetCapacity())
            return false;
        auto fields = reinterpret_cast<MonsterFullField*>(packetPtr->GetBuffer());
        packetPtr->SetLength(packetPtr->GetLength() + sizeof(MonsterFullField));
        packetPtr->Count();
        auto& slot = fields[packetPtr->GetCount() - 1];
        slot.internalId = state.internalID;
        slot.hp = state.hp;
        slot.maxHp = state.data->maxHp;
        slot.dir = state.dir;
        slot.x = state.x;
        slot.y = state.y;
        slot.attacked = state.attacked;
        slot.monsterId = state.data->id;
        return true;
    }

    std::shared_ptr<IPacket> PacketWriter::GetActionHeader(uint16_t zoneID, uint64_t generation) {
        auto packetPtr = AcquireSharedPacket(PacketPoolType::Normal);
        if (packetPtr == nullptr) {
            return nullptr;
        }
        auto* packet = InitializePacketHeader<ActionResultBody>(*packetPtr, OP::ACTION_RESULT, FLAG_SIMULATION,
            sizeof(PacketHeader) + offsetof(ActionResultBody, actions));
        packet->body.count = 0;
        packet->body.zoneID = zoneID;
        packet->body.zoneGeneration = generation;
        return packetPtr;
    }

    bool PacketWriter::WriteActionField(std::shared_ptr<IPacket> packetPtr, const ActionResult& state) {
        if (packetPtr->GetLength() + sizeof(ActionResultField) > packetPtr->GetCapacity())
            return false;
        auto fields = reinterpret_cast<ActionResultField*>(packetPtr->GetBuffer());
        packetPtr->SetLength(packetPtr->GetLength() + sizeof(ActionResultField));
        packetPtr->Count();
        auto& action = fields[packetPtr->GetCount() - 1];
        action.casterType = state.casterType;
        if (state.casterType == 0)
            action.casterId = state.zoneInternalId;
        else
            action.casterId = static_cast<uint32_t>(state.monsterId);
        action.dir = state.dir;
        action.x = state.x;
        action.y = state.y;
        action.skillSlot = state.skillSlot;
        action.skillId = state.skilIId;
        action.skillPhase = state.skillPhase;
        action.aimAngle = state.aimAngle;
        return true;
    }

    std::shared_ptr<IPacket> PacketWriter::GetInitialChunk(bool useGiant) {
        auto packetPtr = AcquireSharedPacket(useGiant ? PacketPoolType::Giant : PacketPoolType::Big);
        if (!packetPtr) {
            return nullptr;
        }
        packetPtr->SetLength(0);
        return packetPtr;
    }

    void PacketWriter::AddChunk(std::shared_ptr<IPacket> packetPtr, size_t len, uint16_t count) {
        auto header = reinterpret_cast<PacketHeader*>(packetPtr->GetBuffer());
        header->length += len;
        uint16_t* countPtr = reinterpret_cast<uint16_t*>(packetPtr->GetBuffer() + sizeof(PacketHeader));
        *countPtr += count;

    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteEquipmentResponse(const EquipmentResponseBody& body) {
        if (body.requestID == 0 || body.ownershipEpoch == 0 || body.operation > 1 || body.slot > 2 || (body.operation == 0 && body.slot != 0) || body.status > static_cast<uint8_t>(EquipmentStatus::StorageFailure)) return nullptr;
        if (body.status == static_cast<uint8_t>(EquipmentStatus::Success)) {
            if ((body.weaponID != 0 && body.weaponID != 3) || (body.shieldID != 0 && body.shieldID != 4)
                || (body.bodyID != 0 && body.bodyID != 5)) return nullptr;
            const uint16_t defense = (body.shieldID == 0 ? 0 : Data::itemMap.at(4).defensePercent)
                + (body.bodyID == 0 ? 0 : Data::itemMap.at(5).defensePercent);
            if (defense > 99 || body.defensePercent != defense) return nullptr;
            const uint16_t bonus = body.weaponID == 0 ? 0 : Data::itemMap.at(3).weaponAttackBonus;
            const uint32_t total = static_cast<uint32_t>(body.baseAttack) + bonus;
            if (body.bonusAttack != bonus || body.effectiveAttack != static_cast<uint16_t>(total > UINT16_MAX ? UINT16_MAX : total)) return nullptr;
        } else if (body.weaponID != 0 || body.shieldID != 0 || body.bodyID != 0 || body.defensePercent != 0 || body.baseAttack != 0 || body.bonusAttack != 0 || body.effectiveAttack != 0) return nullptr;
        auto packetPtr = AcquireUniquePacket(PacketPoolType::Normal);
        if (!packetPtr) return nullptr;
        auto* packet = InitializePacketHeader<EquipmentResponseBody>(*packetPtr, OP::EQUIPMENT_RES, 0);
        packet->body = body;
        return packetPtr;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteBazaarBuyResponse(const MsgBazaarBuyResBody* body) {
        if (body == nullptr || body->requestID == 0 || body->listingID == 0)
            return nullptr;
        auto packetPtr = AcquireUniquePacket(PacketPoolType::Normal);
        if (!packetPtr) return nullptr;
        auto* st = InitializePacketHeader<BazaarBuyResponseBody>(*packetPtr, OP::BAZAAR_BUY_RES, 0);
        st->body = {};
        st->body.requestID = body->requestID;
        st->body.listingID = body->listingID;
        const bool valid = body->resStatus == 0 || body->resStatus == 1 || body->resStatus == 3 || body->resStatus == 4 || body->resStatus == 5;
        st->body.resStatus = valid ? body->resStatus : 5;
        if (st->body.resStatus == 1) {
            if (body->itemID == 0 || body->quantity == 0) st->body.resStatus = 5;
            else {
                st->body.itemID = body->itemID;
                st->body.quantity = body->quantity;
                st->body.diamondSpent = body->diamondSpent;
            }
        }
        return packetPtr;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteBazaarOutboxListResponse(const MsgBazaarOutboxListResBody* body) {
        if (body == nullptr || body->requestID == 0 || body->characterID == 0 || body->resStatus > 2
            || static_cast<uint32_t>(body->count) + body->processingCount > 30
            || (body->resStatus != 1 && (body->count != 0 || body->processingCount != 0))) return nullptr;
        for (uint16_t i = 0; i < body->count; ++i) {
            const auto& row = body->rows[i];
            if (row.eventID == 0 || row.listingID == 0 || row.itemID == 0 || row.quantity == 0 || row.quantity > UINT16_MAX)
                return nullptr;
            for (uint16_t j = 0; j < i; ++j)
                if (body->rows[j].eventID == row.eventID) return nullptr;
        }
        auto packetPtr = AcquireUniquePacket(PacketPoolType::Big);
        if (!packetPtr) return nullptr;
        const uint32_t length = sizeof(PacketStruct<BazaarOutboxListResponseBody>) + sizeof(BazaarOutboxItem) * body->count;
        auto* st = InitializePacketHeader<BazaarOutboxListResponseBody>(*packetPtr, OP::BAZAAR_OUTBOX_LIST_RES, 0, length);
        st->body = { body->requestID, body->resStatus, body->processingCount, body->count };
        auto* rows = reinterpret_cast<BazaarOutboxItem*>(packetPtr->GetBuffer() + sizeof(*st));
        for (uint16_t i = 0; i < body->count; ++i) {
            const auto& row = body->rows[i];
            rows[i] = { row.eventID, row.listingID, row.itemID, row.quantity };
        }
        return packetPtr;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteBazaarCheckOutboxResponse(const MsgBazaarCheckOutboxResBody* body) {
        if (body == nullptr || body->requestID == 0 || body->resStatus > 1
            || static_cast<uint64_t>(body->deliveredCount) + body->duplicatedCount + body->blockedCount > 30)
            return nullptr;
        auto packetPtr = AcquireUniquePacket(PacketPoolType::Normal);
        if (!packetPtr) return nullptr;
        auto* st = InitializePacketHeader<BazaarCheckOutboxResponseBody>(*packetPtr, OP::BAZAAR_CHECK_OUTBOX_RES, 0);
        st->body = { body->requestID, body->resStatus, body->deliveredCount, body->duplicatedCount, body->blockedCount };
        return packetPtr;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteBazaarCancelListingResponse(const MsgBazaarCancelListingResBody* body) {
        if (body == nullptr || body->requestID == 0 || body->listingID == 0 || body->characterID == 0
            || (body->resStatus != 0 && body->resStatus != 1 && body->resStatus != 5)) return nullptr;
        auto packetPtr = AcquireUniquePacket(PacketPoolType::Normal);
        if (!packetPtr) return nullptr;
        auto* st = InitializePacketHeader<BazaarCancelListingResponseBody>(*packetPtr, OP::BAZAAR_CANCEL_LISTING_RES, 0);
        st->body = { body->requestID, body->listingID, body->resStatus };
        return packetPtr;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteBazaarClaimResponse(const MsgBazaarClaimResBody* body) {
        if (body == nullptr || body->requestID == 0 || body->listingID == 0)
            return nullptr;
        auto packetPtr = AcquireUniquePacket(PacketPoolType::Normal);
        if (!packetPtr)
            return nullptr;
        auto* st = InitializePacketHeader<BazaarClaimResponseBody>(*packetPtr, OP::BAZAAR_CLAIM_RES, 0);
        st->body = {};
        st->body.requestID = body->requestID;
        st->body.listingID = body->listingID;
        st->body.resStatus = body->resStatus <= 4 ? body->resStatus : 0;
        st->body.diamondClaimed = st->body.resStatus == 1 ? body->diamondClaimed : 0;
        return packetPtr;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteBazaarMyListResponse(const MsgBazaarMyListResBody* body) {
        if (body == nullptr || body->requestID == 0 || body->characterID == 0)
            return nullptr;
        bool valid = body->resStatus == RES_STATUS::SUCCESS && body->count <= MAX_BAZAAR_MY_LIST;
        if (valid) {
            for (uint16_t i = 0; i < body->count; ++i) {
                const auto& row = body->listings[i];
                if (row.listingID == 0 || row.sellerCharacterID != body->characterID || row.itemID == 0
                    || row.quantity == 0 || row.status > 2) {
                    valid = false;
                    break;
                }
            }
        }
        const uint16_t count = valid ? body->count : 0;
        const uint32_t length = sizeof(PacketStruct<BazaarMyListResponseBody>) + count * sizeof(BazaarSearchListing);
        auto packetPtr = AcquireUniquePacket(PacketPoolType::Big);
        if (!packetPtr || packetPtr->GetCapacity() < length)
            return nullptr;
        auto* st = InitializePacketHeader<BazaarMyListResponseBody>(*packetPtr, OP::BAZAAR_MY_LIST_RES, 0, length);
        st->body = { body->requestID, static_cast<uint8_t>(valid ? RES_STATUS::SUCCESS : RES_STATUS::FAILED), count };
        auto* rows = reinterpret_cast<BazaarSearchListing*>(packetPtr->GetBuffer() + sizeof(*st));
        for (uint16_t i = 0; i < count; ++i) {
            const auto& row = body->listings[i];
            rows[i] = { row.listingID, row.sellerCharacterID, row.itemID, row.quantity,
                row.price, row.registeredAt, row.status };
        }
        return packetPtr;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteBazaarSearchResponse(const MsgBazaarSearchResBody* body) {
        if (body == nullptr || body->requestID == 0)
            return nullptr;
        bool valid = body->resStatus == RES_STATUS::SUCCESS && body->count <= MAX_BAZAAR_SEARCH_RESULT;
        if (valid) {
            for (uint16_t i = 0; i < body->count; ++i) {
                const auto& row = body->listings[i];
                if (row.listingID == 0 || row.sellerCharacterID == 0 || row.itemID == 0
                    || row.quantity == 0 || row.status != 0) {
                    valid = false;
                    break;
                }
            }
        }
        const uint16_t count = valid ? body->count : 0;
        const uint32_t length = sizeof(PacketStruct<BazaarSearchResponseBody>) + count * sizeof(BazaarSearchListing);
        auto packetPtr = AcquireUniquePacket(PacketPoolType::Big);
        if (!packetPtr || packetPtr->GetCapacity() < length)
            return nullptr;
        auto* st = InitializePacketHeader<BazaarSearchResponseBody>(*packetPtr, OP::BAZAAR_SEARCH_RES, 0, length);
        st->body = { body->requestID, static_cast<uint8_t>(valid ? RES_STATUS::SUCCESS : RES_STATUS::FAILED), count };
        auto* rows = reinterpret_cast<BazaarSearchListing*>(packetPtr->GetBuffer() + sizeof(*st));
        for (uint16_t i = 0; i < count; ++i) {
            const auto& row = body->listings[i];
            rows[i] = { row.listingID, row.sellerCharacterID, row.itemID, row.quantity,
                row.price, row.registeredAt, row.status };
        }
        return packetPtr;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteBazaarRegisterResponse(const MsgBazaarRegisterResBody* body) {
        if (body == nullptr || body->requestID == 0 || body->characterID == 0 || body->resStatus > 5) return nullptr;
        auto packetPtr = AcquireUniquePacket(PacketPoolType::Normal);
        if (!packetPtr) return nullptr;
        auto* st = InitializePacketHeader<BazaarRegisterResponseBody>(*packetPtr, OP::BAZAAR_REGISTER_RES, 0);
        st->body = { body->requestID, body->resStatus };
        return packetPtr;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteWalletGoldResponse(const MsgCurrencyResBody* body) {
        if (body == nullptr || body->requestID == 0 || body->characterID == 0 || body->resStatus > 1) return nullptr;
        auto packetPtr = AcquireUniquePacket(PacketPoolType::Normal);
        if (!packetPtr) return nullptr;
        auto* st = InitializePacketHeader<WalletGoldResponseBody>(*packetPtr, OP::WALLET_GOLD_RES, 0);
        st->body = { body->requestID, body->resStatus, body->resStatus == 1 ? body->gold : 0 };
        return packetPtr;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteWalletDiamondResponse(const MsgDiamondResBody* body) {
        if (body == nullptr || body->requestID == 0 || body->characterID == 0 || body->resStatus > 1) return nullptr;
        auto packetPtr = AcquireUniquePacket(PacketPoolType::Normal);
        if (!packetPtr) return nullptr;
        auto* st = InitializePacketHeader<WalletDiamondResponseBody>(*packetPtr, OP::WALLET_DIAMOND_RES, 0);
        st->body = { body->requestID, body->resStatus, body->resStatus == 1 ? body->diamond : 0 };
        return packetPtr;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteProfileBatchRes(const ProfileEntry* entries, uint16_t count) {
        auto packetPtr = AcquireUniquePacket(PacketPoolType::Big);
        if (packetPtr == nullptr) {
            return nullptr;
        }
        const size_t fixedLen = sizeof(PacketHeader) + offsetof(ProfileBatchResBody, entries);
        if (count > MAX_PROFILE_BATCH)
            count = MAX_PROFILE_BATCH;
        // 클램프
        while (count > 0 && fixedLen + count * sizeof(ProfileEntry) > packetPtr->GetCapacity())
            count--;

        auto* packet = InitializePacketHeader<ProfileBatchResBody>(*packetPtr, OP::PROFILE_BATCH_RES, 0,
            static_cast<uint32_t>(fixedLen + count * sizeof(ProfileEntry)));
        packet->body.count = count;
        for (uint16_t i = 0; i < count; i++) {
            packet->body.entries[i] = entries[i];
        }
        return packetPtr;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteZoneChangeFailed() {
        auto packetPtr = AcquireUniquePacket(PacketPoolType::Normal);
        if (packetPtr == nullptr) {
            return nullptr;
        }
        auto* packet = InitializePacketHeader<ZoneChangeResponseBody>(*packetPtr, OP::ZONE_CHANGE_RESPONSE, FLAG_SIMULATION);
        packet->body = {};
        return packetPtr;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteZoneChangeSucess(uint16_t zoneID, uint64_t chatID, uint32_t zoneInternalID, float x, float y, uint64_t generation, uint64_t ownershipEpoch) {
        auto packetPtr = AcquireUniquePacket(PacketPoolType::Normal);
        if (packetPtr == nullptr) {
            return nullptr;
        }
        auto* packet = InitializePacketHeader<ZoneChangeResponseBody>(*packetPtr, OP::ZONE_CHANGE_RESPONSE, FLAG_SIMULATION);
        packet->body.resStatus = 1;
        packet->body.zoneID = zoneID;
        packet->body.chatID = chatID;
        packet->body.startX = x;
        packet->body.startY = y;
        packet->body.zoneInternalID = zoneInternalID;
        packet->body.zoneGeneration = generation;
        packet->body.ownershipEpoch = ownershipEpoch;
        return packetPtr;
    }

    std::unique_ptr<IPacket, PacketDeleter>  PacketWriter::GetPingPacket(uint64_t rtt,uint64_t nowMs) {
        auto packetPtr = AcquireUniquePacket(PacketPoolType::Normal);
        if (!packetPtr) {
            return nullptr;
        }
        auto* packet = InitializePacketHeader<Ping>(*packetPtr, OP::PING, 0);
        packet->body.serverTimeMs = static_cast<uint64_t>(nowMs);
        packet->body.rtt = rtt;
        return packetPtr;
    }
}
