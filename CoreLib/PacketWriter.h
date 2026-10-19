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
        IPacketPool* packetPool = nullptr;
        IPacketPool* bigPacketPool = nullptr;
        IPacketPool* giantPacketPool = nullptr;

        void Initialize(IPacketPool* p, IPacketPool* big, IPacketPool* giant) {
            packetPool = p;
            bigPacketPool = big;
            giantPacketPool = giant;
        }
        
        bool IsReady() {
            if (packetPool == nullptr || bigPacketPool == nullptr || giantPacketPool == nullptr)
                return false;
            return true;
        }
        
        friend class Initializer;
        friend class NonZoneThreadPoolFixture;
        friend class ZoneBenchmarkAccess;
    public:
        std::unique_ptr<IPacket, PacketDeleter> WriteAuthResponse(uint8_t resStatus);
        std::unique_ptr<IPacket, PacketDeleter>  WriteCharacterListResponse(const MsgCharacterListResBody* body);
        std::unique_ptr<IPacket, PacketDeleter>  WriteEnterWorldResponse(const MsgCharacterStateResBody* body);
        std::unique_ptr<IPacket, PacketDeleter>  WriteInventoryResponse(const MsgInventoryResBody* body);
        std::unique_ptr<IPacket, PacketDeleter>  WriteInventoryUpdateResponse(const MsgInventoryUpdateResBody* body);
        std::unique_ptr<IPacket, PacketDeleter>  GetChatWhisperPacket(uint64_t sender, uint32_t senderProfileId, const std::string& message);
        std::shared_ptr<IPacket> GetInitialChatBatchPacket(CHAT_SCOPE scope);
        uint16_t WriteChatBatchPacketField(std::shared_ptr<IPacket> p, uint64_t sender, uint32_t senderProfileId, const std::string& message);
        
        std::shared_ptr<IPacket> GetDeltaHeader(uint16_t zoneID, uint64_t generation) {
            auto p = packetPool->Acquire();
            if (!p) {
                return nullptr;
            }
            p->Clear();
            auto p_st = reinterpret_cast<PacketStruct<DeltaSnapshotBody>*>(p->GetBuffer());
            p_st->header.length = sizeof(PacketHeader) + sizeof(uint16_t) * 2 + sizeof(uint64_t);
            p->SetLength(p_st->header.length);
            p_st->header.magic = MAGIC;
            p_st->header.opcode = OP::ZONE_DELTA_UPDATE_BROADCAST;
            p_st->header.flags = 0x00;
            p_st->header.flags |= FLAG_SIMULATION;
            p_st->body.count = 0;
            p_st->body.zoneID = zoneID;
            p_st->body.zoneGeneration = generation;
            return p;
        }
        template<typename T>
        bool WriteDeltaField(std::shared_ptr<IPacket> p, uint32_t zoneInternalID,uint16_t fieldID, T val) {
            static_assert(sizeof(T) <= sizeof(uint32_t), "Delta field too large"); // 컴파일 타임

            if (p->GetLength() + sizeof(DeltaUpdateField) > p->GetCapacity())
                return false;

            auto fields = reinterpret_cast<DeltaUpdateField*>(p->GetBuffer());
            p->SetLength(p->GetLength() + sizeof(DeltaUpdateField));
            p->Count();
            auto& slot = fields[p->GetCount() - 1];
            slot.zoneInternalID = zoneInternalID;
            slot.fieldID = fieldID;
            
            std::memset(&slot.fieldVal, 0, sizeof(slot.fieldVal)); // 나머지 바이트 초기화
            std::memcpy(&slot.fieldVal, &val, sizeof(T));
            return true;
        }
        std::shared_ptr<IPacket> GetFullHeader(uint16_t zoneID, uint64_t generation);
        bool WriteFullField(std::shared_ptr<IPacket> p, const CharacterState& state);

        std::shared_ptr<IPacket> GetMonsterDeltaHeader(uint16_t zoneID, uint64_t generation) {
            auto p = packetPool->Acquire();
            if (!p) {
                return nullptr;
            }
            p->Clear();
            auto p_st = reinterpret_cast<PacketStruct<MonsterDeltaSnapshotBody>*>(p->GetBuffer());
            p_st->header.length = sizeof(PacketHeader) + sizeof(uint16_t) * 2 + sizeof(uint64_t);
            p->SetLength(p_st->header.length);
            p_st->header.magic = MAGIC;
            p_st->header.opcode = OP::MONSTER_DELTA_UPDATE_BROADCAST;
            p_st->header.flags = 0x00;
            p_st->header.flags |= FLAG_SIMULATION;
            p_st->body.count = 0;
            p_st->body.zoneID = zoneID;
            p_st->body.zoneGeneration = generation;
            return p;
        }
        template<typename T>
        bool WriteMonsterDeltaField(std::shared_ptr<IPacket> p, uint16_t internalID, uint16_t fieldID, T val) {
            static_assert(sizeof(T) <= sizeof(uint32_t), "Delta field too large"); // 컴파일 타임

            if (p->GetLength() + sizeof(MonsterDeltaField) > p->GetCapacity())
                return false;

            auto fields = reinterpret_cast<MonsterDeltaField*>(p->GetBuffer());
            p->SetLength(p->GetLength() + sizeof(MonsterDeltaField));
            p->Count();
            auto& slot = fields[p->GetCount() - 1];
            slot.internalId = internalID;
            slot.fieldId = fieldID;

            std::memset(&slot.fieldVal, 0, sizeof(slot.fieldVal)); // 나머지 바이트 초기화
            std::memcpy(&slot.fieldVal, &val, sizeof(T));
            return true;
        }
        std::shared_ptr<IPacket> GetMonsterFullHeader(uint16_t zoneID, uint64_t generation);
        bool WriteMonsterFullField(std::shared_ptr<IPacket> p, const MonsterState& state);

        std::shared_ptr<IPacket> GetActionHeader(uint16_t zoneID, uint64_t generation);
        bool WriteActionField(std::shared_ptr<IPacket> p, const ActionResult& state);

        std::shared_ptr<IPacket> GetInitialChunk(bool useGiant = false) {
            auto p = (useGiant ? giantPacketPool : bigPacketPool)->Acquire();
            if (!p) {
                return nullptr;
            }
            p->Clear();
            p->SetLength(0);
            return p;
        }

        void AddChunk(std::shared_ptr<IPacket> p, size_t len, uint16_t count) {
            auto header = reinterpret_cast<PacketHeader*>(p->GetBuffer());
            header->length += len;
            uint16_t* countPtr = reinterpret_cast<uint16_t*>(p->GetBuffer() + sizeof(PacketHeader));
            *countPtr += count;

        }

        std::unique_ptr<IPacket, PacketDeleter> WriteProfileBatchRes(const ProfileEntry* entries, uint16_t count);

        std::unique_ptr<IPacket, PacketDeleter> WriteZoneChangeFailed();
        std::unique_ptr<IPacket, PacketDeleter> WriteZoneChangeSucess(uint16_t zoneID, uint64_t chatID, uint32_t zoneInternalID, float x, float y, uint64_t generation);

        std::unique_ptr<IPacket, PacketDeleter>  GetPingPacket(uint64_t rtt,uint64_t nowMs) override {
            auto p = packetPool->AcquireUnique();
            if (!p) {
                return nullptr;
            }
            p->Clear();
            auto p_st = reinterpret_cast<PacketStruct<Ping>*>(p->GetBuffer());
            p_st->header.length = sizeof(PacketHeader) + sizeof(Ping);
            p_st->header.magic = MAGIC;
            p_st->header.opcode = OP::PING;
            p_st->header.flags = 0x00;
            p_st->body.serverTimeMs = static_cast<uint64_t>(nowMs);
            p_st->body.rtt = rtt;
            p->SetLength(sizeof(PacketHeader) + sizeof(Ping));
            return p;
        }
    };
}
