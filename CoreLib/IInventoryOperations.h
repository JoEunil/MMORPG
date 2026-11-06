#pragma once
#include <cstdint>
#include <functional>

namespace Core {
    enum class ItemOperationStatus : uint8_t { Success, Missing, Busy, Full, StorageFailure };
    struct ItemOperationResult {
        ItemOperationStatus status;
        uint16_t remainingQuantity = 0;
    };
    enum class EquipmentOperationStatus : uint8_t { Success, Missing, Busy, Unsupported, StorageFailure };
    struct EquipmentStateResult {
        EquipmentOperationStatus status;
        uint32_t weaponID = 0, shieldID = 0, bodyID = 0;
    };
    class IInventoryOperations {
    public:
        virtual ~IInventoryOperations() = default;
        virtual EquipmentStateResult GetEquipmentState(uint64_t characterID) = 0;
        virtual EquipmentStateResult EquipItem(uint64_t characterID, uint8_t slot, uint32_t itemID) = 0;
        virtual void LoadEquipmentState(uint64_t characterID, std::function<void(EquipmentStateResult)> completed) = 0;
        virtual ItemOperationResult ConsumeItem(uint64_t characterID, uint32_t itemID) = 0;
        virtual void GrantItem(uint64_t characterID, uint32_t itemID, uint16_t quantity,
            std::function<void(ItemOperationResult)> completed) = 0;
    };
}
