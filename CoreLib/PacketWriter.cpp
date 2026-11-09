#include "pch.h"
#include "PacketWriter.h"
#include "IPacket.h"
#include "Config.h"
#include "ItemData.h"
#include "LoggerGlobal.h"
namespace Core {
    void PacketWriter::Initialize(IPacketPoolManager* p) {
        packetPoolManager = p;
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
        auto p = AcquireUniquePacket(PacketPoolType::Normal);
        if (!p) return nullptr;
        auto* packet = InitializePacketHeader<uint8_t>(*p, OP::GOLD_REWARD, 0);
        packet->body = status;
        return p;
    }

    std::unique_ptr<IPacket, PacketDeleter>  PacketWriter::WriteAuthResponse(uint8_t resStatus) {
        auto p = AcquireUniquePacket(PacketPoolType::Normal);
        if (p == nullptr) {
            return nullptr;
        }
        auto* p_st = InitializePacketHeader<AuthResponseBody>(*p, OP::AUTH_RESPONSE, 0);
        p_st->body.resStatus = resStatus;
        return p;
    }

    std::unique_ptr<IPacket, PacketDeleter>  PacketWriter::WriteCharacterListResponse(const MsgCharacterListResBody* body) {
        auto p = AcquireUniquePacket(PacketPoolType::Big);
        if (p == nullptr) {
            return nullptr;
        }
        auto* p_st = InitializePacketHeader<CharacterListResponseBody>(*p, OP::CHARACTER_LIST_RESPONSE, 0);
        p_st->body.resStatus = body->resStatus;
        p_st->body.count = body->count;
        for (int i = 0; i < body->count; i++)
        {
            p_st->body.characters[i].characterID = body->characters[i].characterID;
            p_st->body.characters[i].level = body->characters[i].level;
            std::memcpy(p_st->body.characters[i].name, body->characters[i].name, MAX_CHARNAME_LEN);
        }
        return p;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteEnterWorldResponse(const MsgCharacterStateResBody* body) {
        auto p = AcquireUniquePacket(PacketPoolType::Normal);
        if (p == nullptr) {
            return nullptr;
        }
        auto* p_st = InitializePacketHeader<EnterWorldResponseBody>(*p, OP::ENTER_WORLD_RESPONSE, 0);
        p_st->body.resStatus = body->resStatus;
        std::memcpy(p_st->body.name, body->name, sizeof(p_st->body.name));
        p_st->body.currentZone = body->currentZone;
        p_st->body.hp = body->hp;
        p_st->body.mp = body->mp;
        p_st->body.maxHp = body->maxHp;
        p_st->body.maxMp = body->maxMp;
        p_st->body.attack = body->attack;
        p_st->body.exp = body->exp;
        p_st->body.level = body->level;
        p_st->body.dir = body->dir;
        p_st->body.startX = body->startX;
        p_st->body.startY = body->startY;
        return p;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteInventoryResponse(const MsgInventoryResBody* body) {
        auto p = AcquireUniquePacket(PacketPoolType::Normal);
        if (p == nullptr) {
            return nullptr;
        }

        auto* p_st = InitializePacketHeader<InventoryResBody>(*p, OP::INVENTORY_RES, 0);
        p_st->body = {};
        if (body->resStatus != RES_STATUS::SUCCESS || body->itemCount > MAX_INVENTORY_ITEMS) {
            p_st->body.resStatus = RES_STATUS::FAILED;
            return p;
        }

        p_st->body.resStatus = RES_STATUS::SUCCESS;
        p_st->body.itemCount = body->itemCount;
        for (int i = 0; i < body->itemCount; i++)
        {
            p_st->body.items[i].itemID = body->items[i].itemID;
            p_st->body.items[i].quantity = body->items[i].quantity;
            p_st->body.items[i].slot = body->items[i].slot;
        }

        return p;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteInventoryUpdateResponse(const MsgInventoryUpdateResBody* body) {
        auto p = AcquireUniquePacket(PacketPoolType::Normal);
        if (p == nullptr) {
            return nullptr;
        }

        auto* p_st = InitializePacketHeader<InventoryUpdateResBody>(*p, OP::INVENTORY_UPDATE_RES, 0);
        p_st->body.resStatus = body->resStatus;
        p_st->body.itemID = body->itemID;
        p_st->body.itemQuantity = body->itemQuantity;
        p_st->body.slot = body->slot;

        return p;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::GetChatWhisperPacket(uint64_t sender, uint32_t senderProfileId, const std::string& message) {
        auto p = AcquireUniquePacket(PacketPoolType::Normal);
        if (p == nullptr) {
            return nullptr;
        }
        auto* p_st = InitializePacketHeader<ChatWhisperBody>(*p, OP::CHAT_WHISPER, 0, sizeof(PacketHeader) + sizeof(ChatWhisperBody) + message.length());
        p_st->body.senderProfileId = senderProfileId;
        p_st->body.senderChatID = sender;
        p_st->body.messageLen = message.length();
        uint8_t* msgStart = reinterpret_cast<uint8_t*>(p->GetBuffer()) + sizeof(PacketStruct<ChatWhisperBody>);
        memcpy(msgStart, message.data(), message.length());
        return p;
    }

    std::shared_ptr<IPacket> PacketWriter::GetInitialChatBatchPacket(CHAT_SCOPE scope) {
        auto p = AcquireSharedPacket(PacketPoolType::Big);
        if (p == nullptr) {
            return nullptr;
        }
        auto* p_st = InitializePacketHeader<ChatBatchNotifyBody>(*p, OP::CHAT_BROADCAST, 0);
        p_st->body.scope = scope;
        p_st->body.chatCnt = 0;
        p_st->body.totalMessageLength = 0;
        return p;
    }

    uint16_t PacketWriter::WriteChatBatchPacketField(std::shared_ptr<IPacket> p, uint64_t sender, uint32_t senderProfileId, const std::string& message) {
        auto p_st = reinterpret_cast<PacketStruct<ChatBatchNotifyBody>*>(p->GetBuffer());
        p_st->header.length += message.length();
        p->SetLength(p_st->header.length);

        uint8_t* msgStart = reinterpret_cast<uint8_t*>(p->GetBuffer()) + sizeof(PacketStruct<ChatBatchNotifyBody>) + p_st->body.totalMessageLength;
        memcpy(msgStart, message.data(), message.length());

        p_st->body.entities[p_st->body.chatCnt].senderChatID = sender;
        p_st->body.entities[p_st->body.chatCnt].offset = p_st->body.totalMessageLength;
        p_st->body.entities[p_st->body.chatCnt].messageLength = message.length();
        p_st->body.entities[p_st->body.chatCnt].senderProfileId = senderProfileId;
        p_st->body.chatCnt++;
        p_st->body.totalMessageLength += message.length();
        return p_st->body.chatCnt;
    }

    std::shared_ptr<IPacket> PacketWriter::GetDeltaHeader(uint16_t zoneID, uint64_t generation) {
        auto p = AcquireSharedPacket(PacketPoolType::Normal);
        if (!p) {
            return nullptr;
        }
        auto* p_st = InitializePacketHeader<DeltaSnapshotBody>(*p, OP::ZONE_DELTA_UPDATE_BROADCAST, FLAG_SIMULATION,
            sizeof(PacketHeader) + sizeof(uint16_t) * 2 + sizeof(uint64_t));
        p_st->body.count = 0;
        p_st->body.zoneID = zoneID;
        p_st->body.zoneGeneration = generation;
        return p;
    }

    std::shared_ptr<IPacket> PacketWriter::GetFullHeader(uint16_t zoneID, uint64_t generation) {
        auto p = AcquireSharedPacket(PacketPoolType::Normal);
        if (p == nullptr) {
            return nullptr;
        }
        auto* p_st = InitializePacketHeader<FullSnapshotBody>(*p, OP::ZONE_FULL_STATE_BROADCAST, FLAG_SIMULATION,
            sizeof(PacketHeader) + sizeof(uint16_t) * 2 + sizeof(uint64_t) * 3 + sizeof(uint8_t));
        p_st->body.count = 0;
        p_st->body.zoneID = zoneID;
        p_st->body.zoneGeneration = generation;
        p_st->body.ownershipEpoch = 0;
        p_st->body.bootstrapID = 0;
        p_st->body.bootstrapEnd = 0;
        return p;
    }

    std::shared_ptr<IPacket> PacketWriter::GetBootstrapFullHeader(bool monsters, uint16_t zoneID, uint64_t generation, uint64_t ownershipEpoch, uint64_t bootstrapID) {
        auto p = AcquireSharedPacket(PacketPoolType::Normal);
        if (!p) return nullptr;
        if (monsters) {
            auto* packet = InitializePacketHeader<MonsterFullSnapshotBody>(*p, OP::MONSTER_FULL_STATE_BROADCAST, FLAG_SIMULATION,
                sizeof(PacketHeader) + sizeof(uint16_t) * 2 + sizeof(uint64_t) * 3 + sizeof(uint8_t));
            auto* body = &packet->body;
            body->count = 0; body->zoneID = zoneID; body->zoneGeneration = generation;
            body->ownershipEpoch = ownershipEpoch; body->bootstrapID = bootstrapID; body->bootstrapEnd = 1;
        } else {
            auto* packet = InitializePacketHeader<FullSnapshotBody>(*p, OP::ZONE_FULL_STATE_BROADCAST, FLAG_SIMULATION,
                sizeof(PacketHeader) + sizeof(uint16_t) * 2 + sizeof(uint64_t) * 3 + sizeof(uint8_t));
            auto* body = &packet->body;
            body->count = 0; body->zoneID = zoneID; body->zoneGeneration = generation;
            body->ownershipEpoch = ownershipEpoch; body->bootstrapID = bootstrapID; body->bootstrapEnd = 1;
        }
        return p;
    }

    bool PacketWriter::WriteFullField(std::shared_ptr<IPacket> p, const CharacterState& state){
        if (p->GetLength() + sizeof(FullStateField) > p->GetCapacity())
            return false;
        auto fields = reinterpret_cast<FullStateField*>(p->GetBuffer());
        p->SetLength(p->GetLength() + sizeof(FullStateField));
        p->Count();
        auto& slot = fields[p->GetCount() -1];
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
        auto p = AcquireSharedPacket(PacketPoolType::Normal);
        if (!p) {
            return nullptr;
        }
        auto* p_st = InitializePacketHeader<MonsterDeltaSnapshotBody>(*p, OP::MONSTER_DELTA_UPDATE_BROADCAST, FLAG_SIMULATION,
            sizeof(PacketHeader) + sizeof(uint16_t) * 2 + sizeof(uint64_t));
        p_st->body.count = 0;
        p_st->body.zoneID = zoneID;
        p_st->body.zoneGeneration = generation;
        return p;
    }

    std::shared_ptr<IPacket> PacketWriter::GetMonsterFullHeader(uint16_t zoneID, uint64_t generation) {
        auto p = AcquireSharedPacket(PacketPoolType::Normal);
        if (p == nullptr) {
            return nullptr;
        }
        auto* p_st = InitializePacketHeader<MonsterFullSnapshotBody>(*p, OP::MONSTER_FULL_STATE_BROADCAST, FLAG_SIMULATION,
            sizeof(PacketHeader) + sizeof(uint16_t) * 2 + sizeof(uint64_t) * 3 + sizeof(uint8_t));
        p_st->body.count = 0;
        p_st->body.zoneID = zoneID;
        p_st->body.zoneGeneration = generation;
        p_st->body.ownershipEpoch = 0;
        p_st->body.bootstrapID = 0;
        p_st->body.bootstrapEnd = 0;
        return p;
    }

    bool PacketWriter::WriteMonsterFullField(std::shared_ptr<IPacket> p, const MonsterState& state) {
        if (p->GetLength() + sizeof(MonsterFullField) > p->GetCapacity())
            return false;
        auto fields = reinterpret_cast<MonsterFullField*>(p->GetBuffer());
        p->SetLength(p->GetLength() + sizeof(MonsterFullField));
        p->Count();
        auto& slot = fields[p->GetCount() - 1];
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
        auto p = AcquireSharedPacket(PacketPoolType::Normal);
        if (p == nullptr) {
            return nullptr;
        }
        auto* p_st = InitializePacketHeader<ActionResultBody>(*p, OP::ACTION_RESULT, FLAG_SIMULATION,
            sizeof(PacketHeader) + sizeof(uint16_t) * 2 + sizeof(uint64_t));
        p_st->body.count = 0;
        p_st->body.zoneID = zoneID;
        p_st->body.zoneGeneration = generation;
        return p;
    }

    bool PacketWriter::WriteActionField(std::shared_ptr<IPacket> p, const ActionResult& state) {
        if (p->GetLength() + sizeof(ActionResultField) > p->GetCapacity())
            return false;
        auto fields = reinterpret_cast<ActionResultField*>(p->GetBuffer());
        p->SetLength(p->GetLength() + sizeof(ActionResultField));
        p->Count();
        auto& action = fields[p->GetCount() - 1];
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
        auto p = AcquireSharedPacket(useGiant ? PacketPoolType::Giant : PacketPoolType::Big);
        if (!p) {
            return nullptr;
        }
        p->SetLength(0);
        return p;
    }

    void PacketWriter::AddChunk(std::shared_ptr<IPacket> p, size_t len, uint16_t count) {
        auto header = reinterpret_cast<PacketHeader*>(p->GetBuffer());
        header->length += len;
        uint16_t* countPtr = reinterpret_cast<uint16_t*>(p->GetBuffer() + sizeof(PacketHeader));
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
        auto p = AcquireUniquePacket(PacketPoolType::Normal);
        if (!p) return nullptr;
        auto* packet = InitializePacketHeader<EquipmentResponseBody>(*p, OP::EQUIPMENT_RES, 0);
        packet->body = body;
        return p;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteBazaarBuyResponse(const MsgBazaarBuyResBody* body) {
        if (body == nullptr || body->requestID == 0 || body->listingID == 0)
            return nullptr;
        auto p = AcquireUniquePacket(PacketPoolType::Normal);
        if (!p) return nullptr;
        auto* st = InitializePacketHeader<BazaarBuyResponseBody>(*p, OP::BAZAAR_BUY_RES, 0);
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
        return p;
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
        auto p = AcquireUniquePacket(PacketPoolType::Big);
        if (!p) return nullptr;
        const uint32_t length = sizeof(PacketStruct<BazaarOutboxListResponseBody>) + sizeof(BazaarOutboxItem) * body->count;
        auto* st = InitializePacketHeader<BazaarOutboxListResponseBody>(*p, OP::BAZAAR_OUTBOX_LIST_RES, 0, length);
        st->body = { body->requestID, body->resStatus, body->processingCount, body->count };
        auto* rows = reinterpret_cast<BazaarOutboxItem*>(p->GetBuffer() + sizeof(*st));
        for (uint16_t i = 0; i < body->count; ++i) {
            const auto& row = body->rows[i];
            rows[i] = { row.eventID, row.listingID, row.itemID, row.quantity };
        }
        return p;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteBazaarCheckOutboxResponse(const MsgBazaarCheckOutboxResBody* body) {
        if (body == nullptr || body->requestID == 0 || body->resStatus > 1
            || static_cast<uint64_t>(body->deliveredCount) + body->duplicatedCount + body->blockedCount > 30)
            return nullptr;
        auto p = AcquireUniquePacket(PacketPoolType::Normal);
        if (!p) return nullptr;
        auto* st = InitializePacketHeader<BazaarCheckOutboxResponseBody>(*p, OP::BAZAAR_CHECK_OUTBOX_RES, 0);
        st->body = { body->requestID, body->resStatus, body->deliveredCount, body->duplicatedCount, body->blockedCount };
        return p;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteBazaarCancelListingResponse(const MsgBazaarCancelListingResBody* body) {
        if (body == nullptr || body->requestID == 0 || body->listingID == 0 || body->characterID == 0
            || (body->resStatus != 0 && body->resStatus != 1 && body->resStatus != 5)) return nullptr;
        auto p = AcquireUniquePacket(PacketPoolType::Normal);
        if (!p) return nullptr;
        auto* st = InitializePacketHeader<BazaarCancelListingResponseBody>(*p, OP::BAZAAR_CANCEL_LISTING_RES, 0);
        st->body = { body->requestID, body->listingID, body->resStatus };
        return p;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteBazaarClaimResponse(const MsgBazaarClaimResBody* body) {
        if (body == nullptr || body->requestID == 0 || body->listingID == 0)
            return nullptr;
        auto p = AcquireUniquePacket(PacketPoolType::Normal);
        if (!p)
            return nullptr;
        auto* st = InitializePacketHeader<BazaarClaimResponseBody>(*p, OP::BAZAAR_CLAIM_RES, 0);
        st->body = {};
        st->body.requestID = body->requestID;
        st->body.listingID = body->listingID;
        st->body.resStatus = body->resStatus <= 4 ? body->resStatus : 0;
        st->body.diamondClaimed = st->body.resStatus == 1 ? body->diamondClaimed : 0;
        return p;
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
        auto p = AcquireUniquePacket(PacketPoolType::Big);
        if (!p || p->GetCapacity() < length)
            return nullptr;
        auto* st = InitializePacketHeader<BazaarMyListResponseBody>(*p, OP::BAZAAR_MY_LIST_RES, 0, length);
        st->body = { body->requestID, static_cast<uint8_t>(valid ? RES_STATUS::SUCCESS : RES_STATUS::FAILED), count };
        auto* rows = reinterpret_cast<BazaarSearchListing*>(p->GetBuffer() + sizeof(*st));
        for (uint16_t i = 0; i < count; ++i) {
            const auto& row = body->listings[i];
            rows[i] = { row.listingID, row.sellerCharacterID, row.itemID, row.quantity,
                row.price, row.registeredAt, row.status };
        }
        return p;
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
        auto p = AcquireUniquePacket(PacketPoolType::Big);
        if (!p || p->GetCapacity() < length)
            return nullptr;
        auto* st = InitializePacketHeader<BazaarSearchResponseBody>(*p, OP::BAZAAR_SEARCH_RES, 0, length);
        st->body = { body->requestID, static_cast<uint8_t>(valid ? RES_STATUS::SUCCESS : RES_STATUS::FAILED), count };
        auto* rows = reinterpret_cast<BazaarSearchListing*>(p->GetBuffer() + sizeof(*st));
        for (uint16_t i = 0; i < count; ++i) {
            const auto& row = body->listings[i];
            rows[i] = { row.listingID, row.sellerCharacterID, row.itemID, row.quantity,
                row.price, row.registeredAt, row.status };
        }
        return p;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteBazaarRegisterResponse(const MsgBazaarRegisterResBody* body) {
        if (body == nullptr || body->requestID == 0 || body->characterID == 0 || body->resStatus > 5) return nullptr;
        auto p = AcquireUniquePacket(PacketPoolType::Normal);
        if (!p) return nullptr;
        auto* st = InitializePacketHeader<BazaarRegisterResponseBody>(*p, OP::BAZAAR_REGISTER_RES, 0);
        st->body = { body->requestID, body->resStatus };
        return p;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteWalletGoldResponse(const MsgCurrencyResBody* body) {
        if (body == nullptr || body->requestID == 0 || body->characterID == 0 || body->resStatus > 1) return nullptr;
        auto p = AcquireUniquePacket(PacketPoolType::Normal);
        if (!p) return nullptr;
        auto* st = InitializePacketHeader<WalletGoldResponseBody>(*p, OP::WALLET_GOLD_RES, 0);
        st->body = { body->requestID, body->resStatus, body->resStatus == 1 ? body->gold : 0 };
        return p;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteWalletDiamondResponse(const MsgDiamondResBody* body) {
        if (body == nullptr || body->requestID == 0 || body->characterID == 0 || body->resStatus > 1) return nullptr;
        auto p = AcquireUniquePacket(PacketPoolType::Normal);
        if (!p) return nullptr;
        auto* st = InitializePacketHeader<WalletDiamondResponseBody>(*p, OP::WALLET_DIAMOND_RES, 0);
        st->body = { body->requestID, body->resStatus, body->resStatus == 1 ? body->diamond : 0 };
        return p;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteProfileBatchRes(const ProfileEntry* entries, uint16_t count) {
        auto p = AcquireUniquePacket(PacketPoolType::Big);
        if (p == nullptr) {
            return nullptr;
        }
        const size_t fixedLen = sizeof(PacketHeader) + sizeof(ProfileBatchResBody::count);
        if (count > MAX_PROFILE_BATCH)
            count = MAX_PROFILE_BATCH;
        // 클램프
        while (count > 0 && fixedLen + count * sizeof(ProfileEntry) > p->GetCapacity())
            count--;

        auto* p_st = InitializePacketHeader<ProfileBatchResBody>(*p, OP::PROFILE_BATCH_RES, 0,
            static_cast<uint32_t>(fixedLen + count * sizeof(ProfileEntry)));
        p_st->body.count = count;
        for (uint16_t i = 0; i < count; i++) {
            p_st->body.entries[i] = entries[i];
        }
        return p;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteZoneChangeFailed() {
        auto p = AcquireUniquePacket(PacketPoolType::Normal);
        if (p == nullptr) {
            return nullptr;
        }
        auto* p_st = InitializePacketHeader<ZoneChangeResponseBody>(*p, OP::ZONE_CHANGE_RESPONSE, FLAG_SIMULATION);
        p_st->body = {};
        return p;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteZoneChangeSucess(uint16_t zoneID, uint64_t chatID, uint32_t zoneInternalID, float x, float y, uint64_t generation, uint64_t ownershipEpoch) {
        auto p = AcquireUniquePacket(PacketPoolType::Normal);
        if (p == nullptr) {
            return nullptr;
        }
        auto* p_st = InitializePacketHeader<ZoneChangeResponseBody>(*p, OP::ZONE_CHANGE_RESPONSE, FLAG_SIMULATION);
        p_st->body.resStatus = 1;
        p_st->body.zoneID = zoneID;
        p_st->body.chatID = chatID;
        p_st->body.startX = x;
        p_st->body.startY = y;
        p_st->body.zoneInternalID = zoneInternalID;
        p_st->body.zoneGeneration = generation;
        p_st->body.ownershipEpoch = ownershipEpoch;
        return p;
    }

    std::unique_ptr<IPacket, PacketDeleter>  PacketWriter::GetPingPacket(uint64_t rtt,uint64_t nowMs) {
        auto p = AcquireUniquePacket(PacketPoolType::Normal);
        if (!p) {
            return nullptr;
        }
        auto* p_st = InitializePacketHeader<Ping>(*p, OP::PING, 0);
        p_st->body.serverTimeMs = static_cast<uint64_t>(nowMs);
        p_st->body.rtt = rtt;
        return p;
    }
}
