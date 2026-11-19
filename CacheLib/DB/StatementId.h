#pragma once

#include <cstdint>

namespace Cache {
    enum class StatementId : uint16_t {
        SelectCharacterList = 1,
        InsertCharacter = 2,
        SelectCharacterState = 3,
        UpdateCharacterState = 4,
        SelectInventory = 5,
        UpdateInventory = 6,
        SelectCurrency = 7,
        UpsertCurrency = 8,
        SelectDiamond = 9,
        DepositDiamond = 10,
        SelectBazaarMyList = 11,
        SearchBazaar = 12,
        InsertBazaarListing = 13,
        CancelBazaarListing = 14,
        SelectBazaarListingItem = 15,
        BuyBazaarListing = 16,
        ClaimBazaarProceeds = 17,
        SelectBuyerOutbox = 18,
        ClaimBuyerOutboxEvent = 19,
        InsertProfile = 20,
        SelectProfile = 21,
        RenameProfile = 22,
    };

    enum class StatementType : uint8_t {
        Unknown,
        GameQuery,
        GameUpdate,
        BazaarQuery,
        BazaarUpdate,
    };

    constexpr StatementType GetStatementType(StatementId id) noexcept {
        switch (id) {
        case StatementId::SelectCharacterList:
        case StatementId::SelectCharacterState:
        case StatementId::SelectInventory:
        case StatementId::SelectCurrency:
        case StatementId::SelectProfile:
            return StatementType::GameQuery;
        case StatementId::InsertCharacter:
        case StatementId::UpdateCharacterState:
        case StatementId::UpdateInventory:
        case StatementId::UpsertCurrency:
        case StatementId::InsertProfile:
        case StatementId::RenameProfile:
            return StatementType::GameUpdate;
        case StatementId::SelectDiamond:
        case StatementId::SelectBazaarMyList:
        case StatementId::SearchBazaar:
        case StatementId::CancelBazaarListing:
        case StatementId::SelectBazaarListingItem:
        case StatementId::BuyBazaarListing:
        case StatementId::ClaimBazaarProceeds:
        case StatementId::SelectBuyerOutbox:
            return StatementType::BazaarQuery;
        case StatementId::DepositDiamond:
        case StatementId::InsertBazaarListing:
        case StatementId::ClaimBuyerOutboxEvent:
            return StatementType::BazaarUpdate;
        default:
            return StatementType::Unknown;
        }
    }
}
