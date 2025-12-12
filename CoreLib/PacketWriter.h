#pragma once
#include <memory>
#include "MessageTypes.h"
#include "ZoneState.h"
#include "IPacketPool.h"
#include "PacketTypes.h"
#include "IPacket.h"

namespace Core {
    class IPacket;
    class PacketTypes;
    class PacketWriter {
        IPacketPool* packetPool;
        IPacketPool* bigPacketPool;
        
        void Initialize(IPacketPool* p, IPacketPool* big) {
            packetPool = p;
            bigPacketPool = big;
        }
        
        bool IsReady() {
            return packetPool != nullptr;
        }
        
        friend class Initializer;
    public:
        std::shared_ptr<IPacket> WriteAuthResponse(uint8_t resStatus);
        std::shared_ptr<IPacket> WriteCharacterListResponse(MsgCharacterListResBody* body);
        std::shared_ptr<IPacket> WriteEnterWorldResponse(MsgCharacterStateResBody* body);
        std::shared_ptr<IPacket> WriteInventoryResponse(MsgInventoryResBody* body);
        std::shared_ptr<IPacket> WriteInventoryUpdateResponse(MsgInventoryUpdateResBody* body);
        std::shared_ptr<IPacket> GetInitialChatPacket();
        void WriteChatPacketField(std::shared_ptr<IPacket> p, uint64_t sender, std::string& message);
        
        // 인라인 최적화 기대
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
            static_assert(sizeof(T) <= sizeof(uint64_t), "Delta field too large"); // 컴파일 타임

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
        std::shared_ptr<IPacket> WriteZoneChangeFailed();
        std::shared_ptr<IPacket> WriteZoneChangeSucess(uint16_t zoneID, uint64_t zoneInternalID, float x, float y);
    };
}
