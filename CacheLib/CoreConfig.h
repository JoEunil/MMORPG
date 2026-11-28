#pragma once

#include <cstdint>
#include <optional>
#include <CoreLib/Data/ItemData.h>

namespace Cache {
    // Cache가 사용하는 Core의 아이템 조회·장비 규칙을 연결한다.
    inline constexpr uint8_t WEAPON_SLOT = static_cast<uint8_t>(Data::EquipmentSlot::Weapon);
    inline constexpr uint8_t SHIELD_SLOT = static_cast<uint8_t>(Data::EquipmentSlot::Shield);
    inline constexpr uint8_t BODY_SLOT = static_cast<uint8_t>(Data::EquipmentSlot::Body);

    inline std::optional<uint16_t> FindItemType(uint32_t itemID) {
        const auto* item = Data::FindItem(itemID);
        if (item == nullptr)
            return std::nullopt;
        return static_cast<uint16_t>(item->GetType());
    }

    inline bool CanEquipItem(uint8_t slot, uint32_t itemID) {
        return Data::CanEquip(static_cast<Data::EquipmentSlot>(slot), itemID);
    }
}
