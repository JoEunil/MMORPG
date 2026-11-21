#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <variant>
#include "PotionType.h"

namespace Data {
    // Existing marketplace categories; weapon and shield share category 2.
    enum class ItemType : uint16_t {
        Potion = 1,
        WeaponShield = 2,
        Armor = 3
    };

    enum class EquipmentSlot : uint8_t {
        Weapon = 0,
        Shield = 1,
        Body = 2
    };

    inline constexpr bool IsValidEquipmentSlot(EquipmentSlot slot) {
        return slot == EquipmentSlot::Weapon || slot == EquipmentSlot::Shield || slot == EquipmentSlot::Body;
    }

    struct ItemDataPotion {
        PotionType type = PotionType::HP;
        uint32_t recovery = 0;
        uint32_t cooldownTicks = 0;
    };

    struct ItemDataWeapon {
        uint16_t attackBonus = 0;
    };

    struct ItemDataShield {
        uint8_t defensePercent = 0;
    };

    struct ItemDataArmor {
        uint8_t defensePercent = 0;
    };

    struct ItemData {
        const char* name;
        uint32_t weight;
        std::variant<ItemDataPotion, ItemDataWeapon, ItemDataShield, ItemDataArmor> details;

        ItemType GetType() const;

        std::optional<EquipmentSlot> GetEquipmentSlot() const;
    };

    bool IsValidItemData(const ItemData& item);

    extern const std::unordered_map<uint32_t, ItemData> itemMap;

    const ItemData* FindItem(uint32_t itemID);

    bool CanEquip(EquipmentSlot slot, uint32_t itemID);
}
