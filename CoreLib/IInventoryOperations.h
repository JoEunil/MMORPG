#pragma once
#include <cstdint>
#include <functional>

namespace Core {
    enum class ItemOperationStatus : uint8_t { Success, Missing, Busy, Full, StorageFailure };
    struct ItemOperationResult {
        ItemOperationStatus status;
        uint16_t remainingQuantity = 0;
    };
    class IInventoryOperations {
    public:
        virtual ~IInventoryOperations() = default;
        virtual ItemOperationResult ConsumeItem(uint64_t characterID, uint32_t itemID) = 0;
        virtual void GrantItem(uint64_t characterID, uint32_t itemID, uint16_t quantity,
            std::function<void(ItemOperationResult)> completed) = 0;
    };
}
