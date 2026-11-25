#include "pch.h"
#include "BazaarRegistration.h"

#include <algorithm>
#include "BazaarResult.h"
#include "CacheStorageInventory.h"
#include "CacheStorageCurrency.h"
#include "DB/DBWorker.h"
#include <CoreLib/Data/ItemData.h>
#include <CoreLib/LoggerGlobal.h>

namespace Cache {
    namespace {
        struct RegistrationSelection {
            BazaarRegistrationStatus failure;
            uint32_t itemID = 0;
        };

        RegistrationSelection ValidateSelection(
            const BazaarRegistrationRequest& request, const InventoryCacheItem& inventory) {
            if (request.inventorySlot >= MAX_INVENTORY || inventory.status != CACHE_STATUS::AVAILABLE)
                return {BazaarRegistrationStatus::Failed};

            const auto& selected = inventory.data.items[request.inventorySlot];
            if (selected.itemID == 0 || selected.slot != request.inventorySlot || request.quantity == 0 || request.price == 0)
                return {BazaarRegistrationStatus::InvalidRequest};
            if (selected.quantity < request.quantity)
                return {BazaarRegistrationStatus::InventoryRejected};

            // Removal uses item ID, so duplicate stacks make slot selection ambiguous.
            for (uint16_t slot = 0; slot < MAX_INVENTORY; ++slot) {
                if (slot != request.inventorySlot && inventory.data.items[slot].itemID == selected.itemID)
                    return {BazaarRegistrationStatus::InvalidRequest};
            }
            return {BazaarRegistrationStatus::Failed, selected.itemID};
        }
    }

    void BazaarRegistration::Register(BazaarRegistrationRequest request, Completion completed) {
        const auto selection = ValidateSelection(request, inventory->Getter(request.characterID));
        if (selection.itemID == 0) {
            completed(selection.failure);
            return;
        }

        const auto itemID = selection.itemID;
        const auto* item = Data::FindItem(itemID);
        constexpr uint64_t BAZAAR_FEE_MIN = 10;
        constexpr uint64_t BAZAAR_FEE_RATE = 10;
        const uint64_t fee = std::max(BAZAAR_FEE_MIN, request.price / BAZAAR_FEE_RATE);
        const auto reportUnknown = [request, itemID, fee] {
            Core::errorLogger->LogError("cache handler bazaar", "register result or compensation unknown; manual investigation required",
                {{"char_id", request.characterID}, {"item_id", itemID}, {"quantity", request.quantity}, {"fee", fee}});
        };
        const auto restore = [this, request, itemID, fee] {
            const auto inventoryStatus = std::get<0>(inventory->PartialUpdate(request.characterID, itemID, 2, request.quantity));
            const auto feeStatus = std::get<0>(currency->DepositCurrency(request.characterID, fee));
            return inventoryStatus == CACHE_STATUS::AVAILABLE && feeStatus == CACHE_STATUS::AVAILABLE;
        };
        try {
            const auto failure = item == nullptr
                ? std::optional<BazaarRegistrationStatus>{BazaarRegistrationStatus::InvalidRequest}
                : DeductCosts(request, itemID, fee);
            if (failure) {
                if (*failure == BazaarRegistrationStatus::Unknown)
                    reportUnknown();
                completed(*failure);
                return;
            }
            const auto failed = [completed, reportUnknown] {
                // A worker failure does not establish whether SQL committed.
                reportUnknown();
                completed(BazaarRegistrationStatus::Unknown);
            };
            const auto outcome = worker->Enqueue([request, itemID, itemType = static_cast<uint16_t>(item->GetType()), completed, restore, reportUnknown](DBConnectionBazaar* conn) {
                const auto status = ResolveBazaarRegister([conn, request, itemID, itemType] {
                    return conn->ExecuteUpdate(StatementId::InsertBazaarListing, itemID, request.characterID, itemType, request.quantity, request.price);
                }, restore, reportUnknown);
                completed(static_cast<BazaarRegistrationStatus>(status));
            }, nullptr, failed);
            if (outcome != DBEnqueueResult::Queued) {
                // Rejection proves INSERT never ran, so compensation is permitted.
                completed(static_cast<BazaarRegistrationStatus>(ResolveBazaarRegister([] { return 0; }, restore, reportUnknown)));
            }
        }
        catch (...) {
            // Partial mutation or an uncertain enqueue/response must not be retried.
            reportUnknown();
            completed(BazaarRegistrationStatus::Unknown);
        }
    }

    std::optional<BazaarRegistrationStatus> BazaarRegistration::DeductCosts(
        const BazaarRegistrationRequest& request, uint32_t itemID, uint64_t fee) {
        const auto inventoryStatus = std::get<0>(inventory->PartialUpdate(
            request.characterID, itemID, 2, -static_cast<int32_t>(request.quantity)));
        if (inventoryStatus != CACHE_STATUS::AVAILABLE)
            return BazaarRegistrationStatus::InventoryRejected;

        const auto feeStatus = std::get<0>(currency->TryWithdrawCurrency(request.characterID, fee));
        if (feeStatus == CACHE_STATUS::AVAILABLE)
            return std::nullopt;

        // The fee was not withdrawn; restore only the item.
        const auto restored = std::get<0>(inventory->PartialUpdate(request.characterID, itemID, 2, request.quantity));
        return restored == CACHE_STATUS::AVAILABLE
            ? BazaarRegistrationStatus::CurrencyRejected : BazaarRegistrationStatus::Unknown;
    }
}
