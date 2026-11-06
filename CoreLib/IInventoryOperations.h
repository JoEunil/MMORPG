#pragma once
#include <cstdint>
#include <functional>

namespace Core {
    enum class ItemOperationStatus : uint8_t { Success, Missing, Busy, Full, StorageFailure };
    struct ItemOperationResult {
        ItemOperationStatus status;
        uint16_t remainingQuantity = 0;
    };
    enum class WeaponOperationStatus : uint8_t { Success, Missing, Busy, Unsupported, StorageFailure };
    struct WeaponStateResult {
        WeaponOperationStatus status;
        uint32_t itemID = 0;
    };
    class IInventoryOperations {
    public:
        virtual ~IInventoryOperations() = default;
        virtual WeaponStateResult GetWeaponState(uint64_t characterID) = 0;
        virtual WeaponStateResult EquipWeapon(uint64_t characterID, uint32_t itemID) = 0;
        virtual void LoadWeaponState(uint64_t characterID, std::function<void(WeaponStateResult)> completed) = 0;
        virtual ItemOperationResult ConsumeItem(uint64_t characterID, uint32_t itemID) = 0;
        virtual void GrantItem(uint64_t characterID, uint32_t itemID, uint16_t quantity,
            std::function<void(ItemOperationResult)> completed) = 0;
    };
}
