#pragma once
#include <memory>
#include <chrono>
#include <cstring>

#include "MessageTypes.h"
#include "ZoneState.h"
#include "IPacketPoolManager.h"
#include "PacketTypes.h"
#include "IPacket.h"
#include "IPingPacketWriter.h"


namespace Core {
    class IPacket;
    class PacketTypes;
    class PacketWriter : public IPingPacketWriter{
        IPacketPoolManager* packetPoolManager = nullptr;

        void Initialize(IPacketPoolManager* poolManager);
        bool IsReady();
        std::unique_ptr<IPacket, PacketDeleter> AcquireUniquePacket(PacketPoolType type);
        std::shared_ptr<IPacket> AcquireSharedPacket(PacketPoolType type);

        template<typename T>
        std::unique_ptr<IPacket, PacketDeleter> WriteFixedPacket(OP opcode, uint8_t flags, const T& body);

        template<typename T>
        PacketStruct<T>* InitializePacketHeader(IPacket& packetBuffer, OP opcode, uint8_t flags, uint32_t length = sizeof(PacketStruct<T>)) {
            auto* packet = reinterpret_cast<PacketStruct<T>*>(packetBuffer.GetBuffer());
            packet->header = { MAGIC, length, static_cast<uint8_t>(opcode), flags };
            packetBuffer.SetLength(length);
            return packet;
        }
        
        friend class Initializer;
        friend class ZoneBenchmarkAccess;
    public:
        std::unique_ptr<IPacket, PacketDeleter> WriteItemUseResponse(const ItemUseResponseBody& body);
        std::unique_ptr<IPacket, PacketDeleter> WriteParryResponse(const ParryResponseBody& body);
        std::unique_ptr<IPacket, PacketDeleter> WriteItemReward(const ItemRewardBody& body);
        std::unique_ptr<IPacket, PacketDeleter> WriteGoldReward(uint8_t status);
        std::unique_ptr<IPacket, PacketDeleter> WriteAuthResponse(uint8_t resStatus);
        std::unique_ptr<IPacket, PacketDeleter>  WriteCharacterListResponse(const MsgCharacterListResBody* body);
        std::unique_ptr<IPacket, PacketDeleter>  WriteEnterWorldResponse(const MsgCharacterStateResBody* body);
        std::unique_ptr<IPacket, PacketDeleter>  WriteInventoryResponse(const MsgInventoryResBody* body);
        std::unique_ptr<IPacket, PacketDeleter>  WriteInventoryUpdateResponse(const MsgInventoryUpdateResBody* body);
        std::unique_ptr<IPacket, PacketDeleter>  GetChatWhisperPacket(uint64_t sender, uint32_t senderProfileId, const std::string& message);
        std::shared_ptr<IPacket> GetInitialChatBatchPacket(CHAT_SCOPE scope);
        uint16_t WriteChatBatchPacketField(std::shared_ptr<IPacket> packetPtr, uint64_t sender, uint32_t senderProfileId, const std::string& message);
        
        std::shared_ptr<IPacket> GetDeltaHeader(uint16_t zoneID, uint64_t generation);
        template<typename T>
        bool WriteDeltaField(std::shared_ptr<IPacket> packetPtr, uint32_t zoneInternalID,uint16_t fieldID, T val) {
            static_assert(sizeof(T) <= sizeof(uint32_t), "Delta field too large"); // 컴파일 타임
            if (packetPtr->GetLength() + sizeof(DeltaUpdateField) > packetPtr->GetCapacity())
                return false;
            auto fields = reinterpret_cast<DeltaUpdateField*>(packetPtr->GetBuffer());
            packetPtr->SetLength(packetPtr->GetLength() + sizeof(DeltaUpdateField));
            packetPtr->Count();
            auto& slot = fields[packetPtr->GetCount() - 1];
            slot.zoneInternalID = zoneInternalID;
            slot.fieldID = fieldID;
            std::memset(&slot.fieldVal, 0, sizeof(slot.fieldVal)); // 나머지 바이트 초기화
            std::memcpy(&slot.fieldVal, &val, sizeof(T));
            return true;
        }
        std::shared_ptr<IPacket> GetFullHeader(uint16_t zoneID, uint64_t generation);
        std::shared_ptr<IPacket> GetBootstrapFullHeader(bool monsters, uint16_t zoneID, uint64_t generation, uint64_t ownershipEpoch, uint64_t bootstrapID);
        bool WriteFullField(std::shared_ptr<IPacket> packetPtr, const CharacterState& state);
        std::shared_ptr<IPacket> GetMonsterDeltaHeader(uint16_t zoneID, uint64_t generation);
        template<typename T>
        bool WriteMonsterDeltaField(std::shared_ptr<IPacket> packetPtr, uint16_t internalID, uint16_t fieldID, T val) {
            static_assert(sizeof(T) <= sizeof(uint32_t), "Delta field too large"); // 컴파일 타임
            if (packetPtr->GetLength() + sizeof(MonsterDeltaField) > packetPtr->GetCapacity())
                return false;
            auto fields = reinterpret_cast<MonsterDeltaField*>(packetPtr->GetBuffer());
            packetPtr->SetLength(packetPtr->GetLength() + sizeof(MonsterDeltaField));
            packetPtr->Count();
            auto& slot = fields[packetPtr->GetCount() - 1];
            slot.internalId = internalID;
            slot.fieldId = fieldID;
            std::memset(&slot.fieldVal, 0, sizeof(slot.fieldVal)); // 나머지 바이트 초기화
            std::memcpy(&slot.fieldVal, &val, sizeof(T));
            return true;
        }
        std::shared_ptr<IPacket> GetMonsterFullHeader(uint16_t zoneID, uint64_t generation);
        bool WriteMonsterFullField(std::shared_ptr<IPacket> packetPtr, const MonsterState& state);
        std::shared_ptr<IPacket> GetActionHeader(uint16_t zoneID, uint64_t generation);
        bool WriteActionField(std::shared_ptr<IPacket> packetPtr, const ActionResult& state);

        std::shared_ptr<IPacket> GetInitialChunk(bool useGiant = false);
        void AddChunk(std::shared_ptr<IPacket> packetPtr, size_t len, uint16_t count);

        std::unique_ptr<IPacket, PacketDeleter> WriteEquipmentResponse(const EquipmentResponseBody& body);
        std::unique_ptr<IPacket, PacketDeleter> WriteBazaarBuyResponse(const MsgBazaarBuyResBody* body);
        std::unique_ptr<IPacket, PacketDeleter> WriteBazaarOutboxListResponse(const MsgBazaarOutboxListResBody* body);
        std::unique_ptr<IPacket, PacketDeleter> WriteBazaarCheckOutboxResponse(const MsgBazaarCheckOutboxResBody* body);
        std::unique_ptr<IPacket, PacketDeleter> WriteBazaarCancelListingResponse(const MsgBazaarCancelListingResBody* body);
        std::unique_ptr<IPacket, PacketDeleter> WriteBazaarClaimResponse(const MsgBazaarClaimResBody* body);
        std::unique_ptr<IPacket, PacketDeleter> WriteBazaarMyListResponse(const MsgBazaarMyListResBody* body);
        std::unique_ptr<IPacket, PacketDeleter> WriteBazaarSearchResponse(const MsgBazaarSearchResBody* body);
        std::unique_ptr<IPacket, PacketDeleter> WriteBazaarRegisterResponse(const MsgBazaarRegisterResBody* body);
        std::unique_ptr<IPacket, PacketDeleter> WriteWalletGoldResponse(const MsgCurrencyResBody* body);
        std::unique_ptr<IPacket, PacketDeleter> WriteWalletDiamondResponse(const MsgDiamondResBody* body);
        std::unique_ptr<IPacket, PacketDeleter> WriteProfileBatchRes(const ProfileEntry* entries, uint16_t count);
        std::unique_ptr<IPacket, PacketDeleter> WriteZoneChangeFailed();
        std::unique_ptr<IPacket, PacketDeleter> WriteZoneChangeSucess(uint16_t zoneID, uint64_t chatID, uint32_t zoneInternalID, float x, float y, uint64_t generation, uint64_t ownershipEpoch);
        std::unique_ptr<IPacket, PacketDeleter>  GetPingPacket(uint64_t rtt,uint64_t nowMs) override;
    };
}
