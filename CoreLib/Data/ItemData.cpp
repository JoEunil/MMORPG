#include "pch.h"
#include "ItemData.h"
#include "CsvReader.h"

#include <stdexcept>

namespace Data {
    ItemType ItemData::GetType() const {
        if (std::holds_alternative<ItemDataPotion>(details))
            return ItemType::Potion;
        if (std::holds_alternative<ItemDataArmor>(details))
            return ItemType::Armor;
        return ItemType::WeaponShield;
    }

    std::optional<EquipmentSlot> ItemData::GetEquipmentSlot() const {
        if (std::holds_alternative<ItemDataWeapon>(details))
            return EquipmentSlot::Weapon;
        if (std::holds_alternative<ItemDataShield>(details))
            return EquipmentSlot::Shield;
        if (std::holds_alternative<ItemDataArmor>(details))
            return EquipmentSlot::Body;
        return std::nullopt;
    }

    bool IsValidItemData(const ItemData& item) {
        if (item.name.empty() || item.name.find('\0') != std::string::npos || item.weight == 0)
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

    namespace {
        std::unordered_map<uint32_t, ItemData> items;
        bool itemsInitialized = false;

        template<typename ReadDetails>
        void ReadItems(std::unordered_map<uint32_t, ItemData>& result, const std::filesystem::path& path,
            std::initializer_list<std::string_view> columns, ReadDetails readDetails) {
            CsvReader csv(path, columns);
            while (csv.ReadRow()) {
                const auto id = csv.GetUnsigned<uint32_t>("itemID");
                if (id == 0 || result.contains(id))
                    csv.Fail("itemID", "item ID must be nonzero and unique across all item files");
                ItemData item{std::string(csv.Get("name")), csv.GetUnsigned<uint32_t>("weight"), readDetails(csv)};
                if (item.name.empty() || item.name.find('\0') != std::string::npos)
                    csv.Fail("name", "item name must be nonempty and cannot contain a null character");
                if (item.weight == 0)
                    csv.Fail("weight", "item weight must be positive");
                result.emplace(id, std::move(item));
            }
        }
    }

    const std::unordered_map<uint32_t, ItemData>& itemMap = items;

    std::unordered_map<uint32_t, ItemData> LoadItemData(const std::filesystem::path& directory) {
        std::unordered_map<uint32_t, ItemData> result;
        ReadItems(result, directory / "Potions.csv", {"itemID", "name", "weight", "potionType", "recovery", "cooldownTicks"},
            [](const CsvReader& csv) {
                const auto type = csv.GetUnsigned<uint8_t>("potionType");
                if (type != static_cast<uint8_t>(PotionType::HP) && type != static_cast<uint8_t>(PotionType::MP))
                    csv.Fail("potionType", "expected 0 (HP) or 1 (MP)");
                const auto recovery = csv.GetUnsigned<uint32_t>("recovery");
                if (recovery == 0 || recovery > INT32_MAX)
                    csv.Fail("recovery", "expected a positive recovery no greater than INT32_MAX");
                const auto cooldown = csv.GetUnsigned<uint32_t>("cooldownTicks");
                if (cooldown == 0)
                    csv.Fail("cooldownTicks", "cooldown must be positive");
                return ItemDataPotion{static_cast<PotionType>(type), recovery, cooldown};
            });
        ReadItems(result, directory / "Weapons.csv", {"itemID", "name", "weight", "attackBonus"},
            [](const CsvReader& csv) { return ItemDataWeapon{csv.GetUnsigned<uint16_t>("attackBonus")}; });
        ReadItems(result, directory / "Shields.csv", {"itemID", "name", "weight", "defensePercent"},
            [](const CsvReader& csv) {
                const auto defense = csv.GetUnsigned<uint8_t>("defensePercent");
                if (defense > 99)
                    csv.Fail("defensePercent", "defense must not exceed 99 percent");
                return ItemDataShield{defense};
            });
        ReadItems(result, directory / "Armors.csv", {"itemID", "name", "weight", "defensePercent"},
            [](const CsvReader& csv) {
                const auto defense = csv.GetUnsigned<uint8_t>("defensePercent");
                if (defense > 99)
                    csv.Fail("defensePercent", "defense must not exceed 99 percent");
                return ItemDataArmor{defense};
            });
        uint8_t maxShieldDefense = 0;
        uint8_t maxBodyDefense = 0;
        for (const auto& [id, item] : result) {
            if (const auto* shield = std::get_if<ItemDataShield>(&item.details); shield && shield->defensePercent > maxShieldDefense)
                maxShieldDefense = shield->defensePercent;
            if (const auto* armor = std::get_if<ItemDataArmor>(&item.details); armor && armor->defensePercent > maxBodyDefense)
                maxBodyDefense = armor->defensePercent;
        }
        if (result.empty())
            throw std::runtime_error(directory.string() + ": item definitions cannot be empty");
        if (static_cast<uint16_t>(maxShieldDefense) + maxBodyDefense > 99)
            throw std::runtime_error(directory.string() + ": combined Shields.csv/Armors.csv defense must not exceed 99 percent");
        return result;
    }

    void InitializeItemData(const std::filesystem::path& directory) {
        // Publish once before any worker or cache recovery uses item definitions.
        if (itemsInitialized)
            throw std::logic_error("Item data is already initialized.");
        auto loaded = LoadItemData(directory);
        items.swap(loaded);
        itemsInitialized = true;
    }

    const ItemData* FindItem(uint32_t itemID) {
        if (!itemsInitialized)
            throw std::logic_error("Item data has not been initialized.");
        const auto found = itemMap.find(itemID);
        return found == itemMap.end() ? nullptr : &found->second;
    }

    bool CanEquip(EquipmentSlot slot, uint32_t itemID) {
        if (!IsValidEquipmentSlot(slot))
            return false;
        if (itemID == 0)
            return true; // Zero clears a valid equipment slot.
        const auto* item = FindItem(itemID);
        return item != nullptr && item->GetEquipmentSlot() == slot;
    }
}
