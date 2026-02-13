#pragma once
#include <memory>
#include <chrono>

#include "MessageTypes.h"
#include "ZoneState.h"
#include "IPacketPool.h"
#include "PacketTypes.h"
#include "IPacket.h"
#include "IPingPacketWriter.h"


namespace Core {
    class IPacket;
    class PacketTypes;
    class PacketWriter : public IPingPacketWriter{
        IPacketPool* packetPool;
        IPacketPool* bigPacketPool;

        void Initialize(IPacketPool* p, IPacketPool* big) {
            packetPool = p;
            bigPacketPool = big;
        }
        
        bool IsReady() {
            if (packetPool == nullptr)
                return false;
            return true;
        }
        
        friend class Initializer;
    public:
        std::unique_ptr<IPacket, PacketDeleter> WriteAuthResponse(uint8_t resStatus);
        std::unique_ptr<IPacket, PacketDeleter>  WriteCharacterListResponse(MsgCharacterListResBody* body);
        std::unique_ptr<IPacket, PacketDeleter>  WriteEnterWorldResponse(MsgCharacterStateResBody* body);
        std::unique_ptr<IPacket, PacketDeleter>  WriteInventoryResponse(MsgInventoryResBody* body);
        std::unique_ptr<IPacket, PacketDeleter>  WriteInventoryUpdateResponse(MsgInventoryUpdateResBody* body);
        std::unique_ptr<IPacket, PacketDeleter>  GetChatWhisperPacket(uint64_t sender, std::string& userName, std::string& message);
        std::shared_ptr<IPacket> GetInitialChatBatchPacket(CHAT_SCOPE scope);
        uint16_t WriteChatBatchPacketField(std::shared_ptr<IPacket> p, uint64_t sender, std::string& userName, std::string& message);
        
        std::shared_ptr<IPacket> GetInitialDeltaPacket() {
            auto p = bigPacketPool->Acquire();
            auto p_st = reinterpret_cast<PacketStruct<DeltaSnapshotBody>*>(p->GetBuffer());
            p_st->header.length = sizeof(PacketHeader) + sizeof(p_st->body.count);
            p->SetLength(p_st->header.length);
            p_st->header.opcode = OP::ZONE_DELTA_UPDATE_BROADCAST;
            p_st->header.flags = 0x00;
            p_st->header.flags |= FLAG_SIMULATION;
            p_st->body.count = 0;
            return p;
        }
        template<typename T>
        void WriteDeltaField(std::shared_ptr<IPacket> p, uint64_t zoneInternalID,uint16_t fieldID, T val) {
            static_assert(sizeof(T) <= sizeof(uint32_t), "Delta field too large"); // 컴파일 타임

            auto p_st = reinterpret_cast<PacketStruct<DeltaSnapshotBody>*>(p->GetBuffer());
            p_st->header.length += sizeof(DeltaUpdateField);
            p->SetLength(p_st->header.length);
            p_st->body.count++;
            auto& slot = p_st->body.updates[p_st->body.count-1];
            slot.zoneInternalID = zoneInternalID;
            slot.fieldID = fieldID;
            
            std::memset(&slot.fieldVal, 0, sizeof(slot.fieldVal)); // 나머지 바이트 초기화
            std::memcpy(&slot.fieldVal, &val, sizeof(T));
        }
        std::shared_ptr<IPacket> GetInitialFullPacket();
        void WriteFullField(std::shared_ptr<IPacket> p, CharacterState& state);

        std::shared_ptr<IPacket> GetInitialMonsterDeltaPacket() {
            auto p = bigPacketPool->Acquire();
            auto p_st = reinterpret_cast<PacketStruct<MonsterDeltaSnapshotBody>*>(p->GetBuffer());
            p_st->header.length = sizeof(PacketHeader) + sizeof(p_st->body.count);
            p->SetLength(p_st->header.length);
            p_st->header.opcode = OP::MONSTER_DELTA_UPDATE_BROADCAST;
            p_st->header.flags = 0x00;
            p_st->header.flags |= FLAG_SIMULATION;
            p_st->body.count = 0;
            return p;
        }
        template<typename T>
        void WriteMonsterDeltaField(std::shared_ptr<IPacket> p, uint16_t internalID, uint16_t fieldID, T val) {
            static_assert(sizeof(T) <= sizeof(uint32_t), "Delta field too large"); // 컴파일 타임

            auto p_st = reinterpret_cast<PacketStruct<MonsterDeltaSnapshotBody>*>(p->GetBuffer());
            p_st->header.length += sizeof(MonsterDeltaField);
            p->SetLength(p_st->header.length);
            p_st->body.count++;
            auto& slot = p_st->body.updates[p_st->body.count - 1];
            slot.internalId = internalID;
            slot.fieldId = fieldID;

            std::memset(&slot.fieldVal, 0, sizeof(slot.fieldVal)); // 나머지 바이트 초기화
            std::memcpy(&slot.fieldVal, &val, sizeof(T));
        }
        std::shared_ptr<IPacket> GetInitialMonsterFullPacket();
        void WriteMonsterFullField(std::shared_ptr<IPacket> p, MonsterState& state);

        std::shared_ptr<IPacket> GetInitialActionPacket();
        void WriteActionField(std::shared_ptr<IPacket> p, ActionResult& state);

        std::unique_ptr<IPacket, PacketDeleter> WriteZoneChangeFailed();
        std::unique_ptr<IPacket, PacketDeleter> WriteZoneChangeSucess(uint16_t zoneID, uint64_t chatID, uint64_t zoneInternalID, float x, float y);

        std::unique_ptr<IPacket, PacketDeleter>  GetPingPacket(uint64_t rtt,uint64_t nowMs) override {
            auto p = packetPool->AcquireUnique();
            auto p_st = reinterpret_cast<PacketStruct<Ping>*>(p->GetBuffer());
            p_st->header.length = sizeof(PacketHeader) + sizeof(Ping);
            p_st->header.opcode = OP::PING;
            p_st->header.flags = 0x00;
            p_st->body.serverTimeMs = static_cast<uint64_t>(nowMs);
            p_st->body.rtt = rtt;
            p->SetLength(sizeof(PacketHeader) + sizeof(Ping));
            return p;
        }
    };
}
