#pragma once

#include <unordered_map>
#include <cstdint>
#include <stdexcept>

namespace Data {
    struct ItemData {
        uint16_t  itemType;
        const char* name;
        uint32_t  weight;
        uint32_t hpRecovery = 0;
        uint32_t cooldownTicks = 0;
        uint32_t mpRecovery = 0;
        uint16_t weaponAttackBonus = 0;
        uint8_t defensePercent = 0;
    };

    inline const std::unordered_map<uint32_t, ItemData> itemMap = [] {
        std::unordered_map<uint32_t, ItemData> items = {
        { 1, { 1, "HP포션",   5, 1500, 120 } },
        { 2, { 1, "MP포션",   5, 0, 120, 500 } },
        { 3, { 2, "철검",   150, 0, 0, 0, 2 } },
        { 4, { 2, "철방패",  200, 0, 0, 0, 0, 2 } },
        { 5, { 3, "가죽갑옷", 80, 0, 0, 0, 0, 3 } },
        };
        for (const auto& [id, item] : items) {
            if (item.defensePercent > 99)
                throw std::invalid_argument("Item defense must be in the range 0..99.");
        }
        if (static_cast<uint16_t>(items.at(4).defensePercent) + items.at(5).defensePercent > 99)
            throw std::invalid_argument("Combined shield and body defense must not exceed 99 percent.");
        return items;
    }();
}
