#include "pch.h"
#include "PacketWriter.h"
#include "IPacket.h"
#include "Config.h"

namespace Core {
    std::shared_ptr<IPacket> PacketWriter::WriteAuthResponse(uint8_t resStatus) {
        auto p = packetPool->Acquire();
        auto p_st = reinterpret_cast<PacketStruct<AuthResponseBody>*>(p->GetBuffer());
        p_st->header.length = sizeof(PacketHeader) + sizeof(AuthResponseBody);
        p->SetLength(p_st->header.length);
        p_st->header.opcode = OP::AUTH_RESPONSE;
        p_st->header.magic = MAGIC;
        p_st->header.flags = 0x00;
        p_st->body.resStatus = resStatus;
        return p;
    }

    std::shared_ptr<IPacket> PacketWriter::WriteCharacterListResponse(MsgCharacterListResBody* body) {
        auto p = packetPool->Acquire();
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

    std::shared_ptr<IPacket> PacketWriter::WriteEnterWorldResponse(MsgCharacterStateResBody* body) {
        auto p = packetPool->Acquire();
        auto p_st = reinterpret_cast<PacketStruct<EnterWorldResponseBody>*>(p->GetBuffer());
        p_st->header.length = sizeof(PacketHeader) + sizeof(EnterWorldResponseBody);
        p->SetLength(p_st->header.length);
        p_st->header.opcode = OP::ENTER_WORLD_RESPONSE;
        p_st->header.magic = MAGIC;
        p_st->header.flags = 0x00;
        p_st->body.resStatus = body->resStatus;
        std::memcpy(p_st->body.name, body->name, sizeof(p_st->body.name));
        p_st->body.level = body->level;
        p_st->body.currentZone = body->currentZone;
        p_st->body.exp = body->exp;
        p_st->body.hp = body->hp;
        p_st->body.mp = body->mp;
        p_st->body.dir = body->dir;
        p_st->body.startX = body->startX;
        p_st->body.startY = body->startY;
        return p;
    }

    std::shared_ptr<IPacket> PacketWriter::WriteInventoryResponse(MsgInventoryResBody* body) {
        auto p = packetPool->Acquire();
        
        auto p_st = reinterpret_cast<PacketStruct<InventoryResBody>*>(p->GetBuffer());
        p_st->header.length = sizeof(PacketHeader) + sizeof(InventoryResBody);
        p->SetLength(p_st->header.length);
        p_st->header.opcode = OP::INVENTORY_RES;
        p_st->header.magic = MAGIC;
        p_st->header.flags = 0x00;
        p_st->body.resStatus = body->resStatus;
        p_st->body.itemCount = body->itemCount;
        for (int i = 0; i < body->itemCount; i++)
        {
            p_st->body.items[i].itemID = body->items[i].itemID;
            p_st->body.items[i].quantity = body->items[i].quantity;
            p_st->body.items[i].slot = body->items[i].slot;
        }
        
        return p;
    }

    std::shared_ptr<IPacket> PacketWriter::WriteInventoryUpdateResponse(MsgInventoryUpdateResBody* body) {
        auto p = packetPool->Acquire();
        
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

    std::shared_ptr<IPacket> PacketWriter::GetInitialChatPacket() {
        auto p = bigPacketPool->Acquire();
        auto p_st = reinterpret_cast<PacketStruct<ChatFloodBody>*>(p->GetBuffer());
        p_st->header.length = sizeof(PacketHeader) + sizeof(ChatFloodBody);
        p->SetLength(p_st->header.length);
        p_st->header.opcode = OP::CHAT_BROADCAST;
        p_st->header.magic = MAGIC;
        p_st->header.flags = 0x00;
        p_st->body.chatCnt = 0;
        p_st->body.totalMessageLength = 0;
        return p;
    }

    void PacketWriter::WriteChatPacketField(std::shared_ptr<IPacket> p, uint64_t sender, std::string& message) {
        auto p_st = reinterpret_cast<PacketStruct<ChatFloodBody>*>(p->GetBuffer());
        p_st->header.length += message.length();
        p->SetLength(p_st->header.length);
        
        uint8_t* msgStart = reinterpret_cast<uint8_t*>(p->GetBuffer()) + sizeof(PacketStruct<ChatFloodBody>) + p_st->body.totalMessageLength;
        memcpy(msgStart, message.data(), message.length());

        p_st->body.entities[p_st->body.chatCnt].zoneInternalID = sender;
        p_st->body.entities[p_st->body.chatCnt].offset = p_st->body.totalMessageLength;
        p_st->body.entities[p_st->body.chatCnt].messageLength = message.length();
        p_st->body.chatCnt++;
        p_st->body.totalMessageLength += message.length();
    }

    std::shared_ptr<IPacket> PacketWriter::GetInitialFullPacket() {
        auto p = bigPacketPool->Acquire();
        auto p_st = reinterpret_cast<PacketStruct<FullSnapshotBody>*>(p->GetBuffer());
        p_st->header.length = sizeof(PacketHeader) + sizeof(p_st->body.count);
        p->SetLength(p_st->header.length);
        p_st->header.opcode = OP::ZONE_FULL_STATE_BROADCAST;
        p_st->header.magic = MAGIC;
        p_st->header.flags = 0x00;
        p_st->header.flags |= FLAG_SIMULATION;
        p_st->body.count = 0;
        return p;
    }

    void PacketWriter::WriteFullField(std::shared_ptr<IPacket> p, CharacterState& state){
        auto p_st = reinterpret_cast<PacketStruct<FullSnapshotBody>*>(p->GetBuffer());
        p_st->header.length += sizeof(FullStateField);
        p->SetLength(p_st->header.length);
        p_st->body.count++;
        auto& slot = p_st->body.states[p_st->body.count-1];
        slot.zoneInternalID = state.zoneInternalID;
        slot.hp = state.hp;
        slot.mp = state.mp;
        slot.level = state.level;
        slot.exp = state.exp;
        slot.dir = state.dir;
        slot.x = state.x;
        slot.y = state.y;
        std::memcpy(p_st->body.states[p_st->body.count-1].charName, state.charName, MAX_CHARNAME_LEN);
    }

    std::shared_ptr<IPacket> PacketWriter::WriteZoneChangeFailed() {
        auto p = packetPool->Acquire();
        auto p_st = reinterpret_cast<PacketStruct<ZoneChangeResponseBody>*>(p->GetBuffer());
        p_st->header.length = sizeof(PacketHeader) + sizeof(ZoneChangeResponseBody);
        p->SetLength(p_st->header.length);
        p_st->header.opcode = OP::ZONE_CHANGE_RESPONSE;
        p_st->header.magic = MAGIC;
        p_st->header.flags = 0x00;
        p_st->header.flags |= FLAG_SIMULATION;
        p_st->body.resStatus = 0;
        return p;
    }

    std::shared_ptr<IPacket> PacketWriter::WriteZoneChangeSucess(uint16_t zoneID, uint64_t zoneInternalID, float x, float y) {
        auto p = packetPool->Acquire();
        auto p_st = reinterpret_cast<PacketStruct<ZoneChangeResponseBody>*>(p->GetBuffer());
        p_st->header.length = sizeof(PacketHeader) + sizeof(ZoneChangeResponseBody);
        p->SetLength(p_st->header.length);
        p_st->header.opcode = OP::ZONE_CHANGE_RESPONSE;
        p_st->header.magic = MAGIC;
        p_st->header.flags |= 0x00;
        p_st->header.flags |= FLAG_SIMULATION;
        p_st->body.resStatus = 1;
        p_st->body.zoneID = zoneID;
        p_st->body.startX = x;
        p_st->body.startY = y;
        p_st->body.zoneInternalID = zoneInternalID;
        return p;
    }
}
