#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
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

        ItemType GetType() const {
            if (std::holds_alternative<ItemDataPotion>(details))
                return ItemType::Potion;
            if (std::holds_alternative<ItemDataArmor>(details))
                return ItemType::Armor;
            return ItemType::WeaponShield;
        }

        std::optional<EquipmentSlot> GetEquipmentSlot() const {
            if (std::holds_alternative<ItemDataWeapon>(details))
                return EquipmentSlot::Weapon;
            if (std::holds_alternative<ItemDataShield>(details))
                return EquipmentSlot::Shield;
            if (std::holds_alternative<ItemDataArmor>(details))
                return EquipmentSlot::Body;
            return std::nullopt;
        }
    };

    inline bool IsValidItemData(const ItemData& item) {
        if (item.name == nullptr || item.name[0] == '\0' || item.weight == 0)
            return false;
        if (const auto* potion = std::get_if<ItemDataPotion>(&item.details))
            return (potion->type == PotionType::HP || potion->type == PotionType::MP) &&
                potion->recovery > 0 && potion->recovery <= INT32_MAX && potion->cooldownTicks > 0;
        if (const auto* shield = std::get_if<ItemDataShield>(&item.details))
            return shield->defensePercent <= 99;
        if (const auto* armor = std::get_if<ItemDataArmor>(&item.details))
            return armor->defensePercent <= 99;
        return std::holds_alternative<ItemDataWeapon>(item.details);
    }

    inline const std::unordered_map<uint32_t, ItemData> itemMap = [] {
        std::unordered_map<uint32_t, ItemData> items = {
            {1, {"HP포션", 5, ItemDataPotion{PotionType::HP, 1500, 120}}},
            {2, {"MP포션", 5, ItemDataPotion{PotionType::MP, 500, 120}}},
            {3, {"철검", 150, ItemDataWeapon{2}}},
            {4, {"철방패", 200, ItemDataShield{2}}},
            {5, {"가죽갑옷", 80, ItemDataArmor{3}}},
            {6, {"중급HP포션", 5, ItemDataPotion{PotionType::HP, 3000, 120}}},
            {7, {"중급MP포션", 5, ItemDataPotion{PotionType::MP, 1000, 120}}},
            {8, {"상급HP포션", 5, ItemDataPotion{PotionType::HP, 4500, 120}}},
            {9, {"상급MP포션", 5, ItemDataPotion{PotionType::MP, 1500, 120}}},
        };
        uint8_t maxShieldDefense = 0;
        uint8_t maxBodyDefense = 0;
        for (const auto& [id, item] : items) {
            if (id == 0 || !IsValidItemData(item))
                throw std::invalid_argument("Invalid item definition.");
            if (const auto* shield = std::get_if<ItemDataShield>(&item.details); shield && shield->defensePercent > maxShieldDefense)
                maxShieldDefense = shield->defensePercent;
            if (const auto* armor = std::get_if<ItemDataArmor>(&item.details); armor && armor->defensePercent > maxBodyDefense)
                maxBodyDefense = armor->defensePercent;
        }
        if (static_cast<uint16_t>(maxShieldDefense) + maxBodyDefense > 99)
            throw std::invalid_argument("Combined shield and body defense must not exceed 99 percent.");
        return items;
    }();

    inline const ItemData* FindItem(uint32_t itemID) {
        const auto found = itemMap.find(itemID);
        return found == itemMap.end() ? nullptr : &found->second;
    }

    inline bool CanEquip(EquipmentSlot slot, uint32_t itemID) {
        if (!IsValidEquipmentSlot(slot))
            return false;
        if (itemID == 0)
            return true; // Zero clears a valid equipment slot.
        const auto* item = FindItem(itemID);
        return item != nullptr && item->GetEquipmentSlot() == slot;
    }
}
