#include "pch.h"
#include "PacketWriter.h"
#include "IPacket.h"
#include "Config.h"
namespace Core {
    static_assert(sizeof(PacketStruct<BazaarBuyRequestBody>) == 20);
    static_assert(sizeof(PacketStruct<BazaarBuyResponseBody>) == 37);
    static_assert(sizeof(PacketStruct<BazaarCheckOutboxRequestBody>) == 12);
    static_assert(sizeof(PacketStruct<BazaarCheckOutboxResponseBody>) == 25);

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteBazaarBuyResponse(const MsgBazaarBuyResBody* body) {
        if (body == nullptr || body->requestID == 0 || body->listingID == 0)
            return nullptr;
        auto p = packetPool->AcquireUnique();
        if (!p) return nullptr;
        p->Clear();
        auto* st = reinterpret_cast<PacketStruct<BazaarBuyResponseBody>*>(p->GetBuffer());
        st->header = { MAGIC, sizeof(*st), OP::BAZAAR_BUY_RES, 0 };
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
        p->SetLength(sizeof(*st));
        return p;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteBazaarCheckOutboxResponse(const MsgBazaarCheckOutboxResBody* body) {
        if (body == nullptr || body->requestID == 0 || body->resStatus > 1
            || static_cast<uint64_t>(body->deliveredCount) + body->duplicatedCount + body->blockedCount > 30)
            return nullptr;
        auto p = packetPool->AcquireUnique();
        if (!p) return nullptr;
        p->Clear();
        auto* st = reinterpret_cast<PacketStruct<BazaarCheckOutboxResponseBody>*>(p->GetBuffer());
        st->header = { MAGIC, sizeof(*st), OP::BAZAAR_CHECK_OUTBOX_RES, 0 };
        st->body = { body->requestID, body->resStatus, body->deliveredCount, body->duplicatedCount, body->blockedCount };
        p->SetLength(sizeof(*st));
        return p;
    }

    static_assert(sizeof(PacketStruct<BazaarClaimRequestBody>) == 20);
    static_assert(sizeof(PacketStruct<BazaarClaimResponseBody>) == 29);

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteBazaarClaimResponse(const MsgBazaarClaimResBody* body) {
        if (body == nullptr || body->requestID == 0 || body->listingID == 0)
            return nullptr;
        auto p = packetPool->AcquireUnique();
        if (!p)
            return nullptr;
        p->Clear();
        auto* st = reinterpret_cast<PacketStruct<BazaarClaimResponseBody>*>(p->GetBuffer());
        st->header = { MAGIC, sizeof(*st), OP::BAZAAR_CLAIM_RES, 0 };
        st->body = {};
        st->body.requestID = body->requestID;
        st->body.listingID = body->listingID;
        st->body.resStatus = body->resStatus <= 4 ? body->resStatus : 0;
        st->body.diamondClaimed = st->body.resStatus == 1 ? body->diamondClaimed : 0;
        p->SetLength(sizeof(*st));
        return p;
    }

    static_assert(sizeof(MsgStruct<MsgBazaarSearchResBody>) <= MESSAGE_LEN);

    static_assert(sizeof(MsgStruct<MsgBazaarMyListResBody>) <= MESSAGE_LEN);

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
        auto p = bigPacketPool->AcquireUnique();
        if (!p || p->GetCapacity() < length)
            return nullptr;
        p->Clear();
        auto* st = reinterpret_cast<PacketStruct<BazaarMyListResponseBody>*>(p->GetBuffer());
        st->header.magic = MAGIC;
        st->header.length = length;
        st->header.opcode = OP::BAZAAR_MY_LIST_RES;
        st->header.flags = 0;
        st->body = { body->requestID, static_cast<uint8_t>(valid ? RES_STATUS::SUCCESS : RES_STATUS::FAILED), count };
        auto* rows = reinterpret_cast<BazaarSearchListing*>(p->GetBuffer() + sizeof(*st));
        for (uint16_t i = 0; i < count; ++i) {
            const auto& row = body->listings[i];
            rows[i] = { row.listingID, row.sellerCharacterID, row.itemID, row.quantity,
                row.price, row.registeredAt, row.status };
        }
        p->SetLength(length);
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
        auto p = bigPacketPool->AcquireUnique();
        if (!p || p->GetCapacity() < length)
            return nullptr;
        p->Clear();
        auto* st = reinterpret_cast<PacketStruct<BazaarSearchResponseBody>*>(p->GetBuffer());
        st->header.magic = MAGIC;
        st->header.length = length;
        st->header.opcode = OP::BAZAAR_SEARCH_RES;
        st->header.flags = 0;
        st->body = { body->requestID, static_cast<uint8_t>(valid ? RES_STATUS::SUCCESS : RES_STATUS::FAILED), count };
        auto* rows = reinterpret_cast<BazaarSearchListing*>(p->GetBuffer() + sizeof(*st));
        for (uint16_t i = 0; i < count; ++i) {
            const auto& row = body->listings[i];
            rows[i] = { row.listingID, row.sellerCharacterID, row.itemID, row.quantity,
                row.price, row.registeredAt, row.status };
        }
        p->SetLength(length);
        return p;
    }

    std::unique_ptr<IPacket, PacketDeleter>  PacketWriter::WriteAuthResponse(uint8_t resStatus) {
        auto p = packetPool->AcquireUnique();
        if (p == nullptr) {
            return nullptr;
        }
        p->Clear();
        auto p_st = reinterpret_cast<PacketStruct<AuthResponseBody>*>(p->GetBuffer());
        p_st->header.length = sizeof(PacketHeader) + sizeof(AuthResponseBody);
        p->SetLength(p_st->header.length);
        p_st->header.opcode = OP::AUTH_RESPONSE;
        p_st->header.magic = MAGIC;
        p_st->header.flags = 0x00;
        p_st->body.resStatus = resStatus;
        return p;
    }

    std::unique_ptr<IPacket, PacketDeleter>  PacketWriter::WriteCharacterListResponse(const MsgCharacterListResBody* body) {
        auto p = bigPacketPool->AcquireUnique();
        if (p == nullptr) {
            return nullptr;
        }
        p->Clear();
        auto p_st = reinterpret_cast<PacketStruct<CharacterListResponseBody>*>(p->GetBuffer());
        p_st->header.length = sizeof(PacketHeader) + sizeof(CharacterListResponseBody);
        p->SetLength(p_st->header.length);
        p_st->header.opcode = OP::CHARACTER_LIST_RESPONSE;
        p_st->header.magic = MAGIC;
        p_st->header.flags = 0x00;
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
        auto p = packetPool->AcquireUnique();
        if (p == nullptr) {
            return nullptr;
        }
        p->Clear();
        auto p_st = reinterpret_cast<PacketStruct<EnterWorldResponseBody>*>(p->GetBuffer());
        p_st->header.length = sizeof(PacketHeader) + sizeof(EnterWorldResponseBody);
        p->SetLength(p_st->header.length);
        p_st->header.opcode = OP::ENTER_WORLD_RESPONSE;
        p_st->header.magic = MAGIC;
        p_st->header.flags = 0x00;
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
        auto p = packetPool->AcquireUnique();
        if (p == nullptr) {
            return nullptr;
        }

        p->Clear();
        auto p_st = reinterpret_cast<PacketStruct<InventoryResBody>*>(p->GetBuffer());
        p_st->header.length = sizeof(PacketHeader) + sizeof(InventoryResBody);
        p->SetLength(p_st->header.length);
        p_st->header.opcode = OP::INVENTORY_RES;
        p_st->header.magic = MAGIC;
        p_st->header.flags = 0x00;
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
        auto p = packetPool->AcquireUnique();
        if (p == nullptr) {
            return nullptr;
        }

        p->Clear();
        auto p_st = reinterpret_cast<PacketStruct<InventoryUpdateResBody>*>(p->GetBuffer());
        p_st->header.length = sizeof(PacketHeader) + sizeof(InventoryUpdateResBody);
        p->SetLength(p_st->header.length);
        p_st->header.opcode = OP::INVENTORY_UPDATE_RES;
        p_st->header.magic = MAGIC;
        p_st->header.flags = 0x00;
        p_st->body.resStatus = body->resStatus;
        p_st->body.itemID = body->itemID;
        p_st->body.itemQuantity = body->itemQuantity;
        p_st->body.slot = body->slot;
        
        return p;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::GetChatWhisperPacket(uint64_t sender, uint32_t senderProfileId, const std::string& message) {
        auto p = packetPool->AcquireUnique();
        if (p == nullptr) {
            return nullptr;
        }
        p->Clear();
        auto p_st = reinterpret_cast<PacketStruct<ChatWhisperBody>*>(p->GetBuffer());
        p_st->header.length = sizeof(PacketHeader) + sizeof(ChatWhisperBody) + message.length();
        p->SetLength(p_st->header.length);
        p_st->header.opcode = OP::CHAT_WHISPER;
        p_st->header.magic = MAGIC;
        p_st->header.flags = 0x00;
        p_st->body.senderProfileId = senderProfileId;
        p_st->body.senderChatID = sender;
        p_st->body.messageLen = message.length();
        uint8_t* msgStart = reinterpret_cast<uint8_t*>(p->GetBuffer()) + sizeof(PacketStruct<ChatWhisperBody>);
        memcpy(msgStart, message.data(), message.length());
        return p;
    }

    std::shared_ptr<IPacket> PacketWriter::GetInitialChatBatchPacket(CHAT_SCOPE scope) {
        auto p = bigPacketPool->Acquire();
        if (p == nullptr) {
            return nullptr;
        }
        p->Clear();
        auto p_st = reinterpret_cast<PacketStruct<ChatBatchNotifyBody>*>(p->GetBuffer());
        p_st->header.length = sizeof(PacketHeader) + sizeof(ChatBatchNotifyBody);
        p->SetLength(p_st->header.length);
        p_st->header.opcode = OP::CHAT_BROADCAST;
        p_st->header.magic = MAGIC;
        p_st->header.flags = 0x00;
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

    std::shared_ptr<IPacket> PacketWriter::GetFullHeader(uint16_t zoneID, uint64_t generation) {
        auto p = packetPool->Acquire();
        if (p == nullptr) {
            return nullptr;
        }
        p->Clear();
        auto p_st = reinterpret_cast<PacketStruct<FullSnapshotBody>*>(p->GetBuffer());
        p_st->header.length = sizeof(PacketHeader) + sizeof(uint16_t) * 2 + sizeof(uint64_t);
        p->SetLength(p_st->header.length);
        p_st->header.opcode = OP::ZONE_FULL_STATE_BROADCAST;
        p_st->header.magic = MAGIC;
        p_st->header.flags = 0x00;
        p_st->header.flags |= FLAG_SIMULATION;
        p_st->body.count = 0;
        p_st->body.zoneID = zoneID;
        p_st->body.zoneGeneration = generation;
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

    std::shared_ptr<IPacket> PacketWriter::GetMonsterFullHeader(uint16_t zoneID, uint64_t generation) {
        auto p = packetPool->Acquire();
        if (p == nullptr) {
            return nullptr;
        }
        p->Clear();
        auto p_st = reinterpret_cast<PacketStruct<MonsterFullSnapshotBody>*>(p->GetBuffer());
        p_st->header.length = sizeof(PacketHeader) + sizeof(uint16_t) * 2 + sizeof(uint64_t);
        p->SetLength(p_st->header.length);
        p_st->header.opcode = OP::MONSTER_FULL_STATE_BROADCAST;
        p_st->header.magic = MAGIC;
        p_st->header.flags = 0x00;
        p_st->header.flags |= FLAG_SIMULATION;
        p_st->body.count = 0;
        p_st->body.zoneID = zoneID;
        p_st->body.zoneGeneration = generation;
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
        auto p = packetPool->Acquire();
        if (p == nullptr) {
            return nullptr;
        }
        p->Clear();
        auto p_st = reinterpret_cast<PacketStruct<ActionResultBody>*>(p->GetBuffer());
        p_st->header.length = sizeof(PacketHeader) + sizeof(uint16_t) * 2 + sizeof(uint64_t);
        p->SetLength(p_st->header.length);
        p_st->header.opcode = OP::ACTION_RESULT;
        p_st->header.magic = MAGIC;
        p_st->header.flags = 0x00;
        p_st->header.flags |= FLAG_SIMULATION;
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
        return true;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteZoneChangeFailed() {
        auto p = packetPool->AcquireUnique();
        if (p == nullptr) {
            return nullptr;
        }
        p->Clear();
        auto p_st = reinterpret_cast<PacketStruct<ZoneChangeResponseBody>*>(p->GetBuffer());
        p_st->header.length = sizeof(PacketHeader) + sizeof(ZoneChangeResponseBody);
        p->SetLength(p_st->header.length);
        p_st->header.opcode = OP::ZONE_CHANGE_RESPONSE;
        p_st->header.magic = MAGIC;
        p_st->header.flags = 0x00;
        p_st->header.flags |= FLAG_SIMULATION;
        p_st->body = {};
        return p;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteProfileBatchRes(const ProfileEntry* entries, uint16_t count) {
        auto p = bigPacketPool->AcquireUnique();
        if (p == nullptr) {
            return nullptr;
        }
        p->Clear();
        auto p_st = reinterpret_cast<PacketStruct<ProfileBatchResBody>*>(p->GetBuffer());

        const size_t fixedLen = sizeof(PacketHeader) + sizeof(p_st->body.count);
        if (count > MAX_PROFILE_BATCH)
            count = MAX_PROFILE_BATCH;
        // 클램프
        while (count > 0 && fixedLen + count * sizeof(ProfileEntry) > p->GetCapacity())
            count--;

        p_st->header.length = static_cast<uint32_t>(fixedLen + count * sizeof(ProfileEntry));
        p->SetLength(p_st->header.length);
        p_st->header.opcode = OP::PROFILE_BATCH_RES;
        p_st->header.magic = MAGIC;
        p_st->header.flags = 0x00;
        p_st->body.count = count;
        for (uint16_t i = 0; i < count; i++) {
            p_st->body.entries[i] = entries[i];
        }
        return p;
    }

    std::unique_ptr<IPacket, PacketDeleter> PacketWriter::WriteZoneChangeSucess(uint16_t zoneID, uint64_t chatID, uint32_t zoneInternalID, float x, float y, uint64_t generation) {
        auto p = packetPool->AcquireUnique();
        if (p == nullptr) {
            return nullptr;
        }
        p->Clear();
        auto p_st = reinterpret_cast<PacketStruct<ZoneChangeResponseBody>*>(p->GetBuffer());
        p_st->header.length = sizeof(PacketHeader) + sizeof(ZoneChangeResponseBody);
        p->SetLength(p_st->header.length);
        p_st->header.opcode = OP::ZONE_CHANGE_RESPONSE;
        p_st->header.magic = MAGIC;
        p_st->header.flags = 0x00;
        p_st->header.flags |= FLAG_SIMULATION;
        p_st->body.resStatus = 1;
        p_st->body.zoneID = zoneID;
        p_st->body.chatID = chatID;
        p_st->body.startX = x;
        p_st->body.startY = y;
        p_st->body.zoneInternalID = zoneInternalID;
        p_st->body.zoneGeneration = generation;
        return p;
    }
}
