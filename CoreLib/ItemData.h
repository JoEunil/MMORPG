#pragma once

#include <unordered_map>
#include <cstdint>

namespace Data {
    inline constexpr uint32_t monsterPotionItemID = 1;
    inline constexpr uint16_t monsterPotionQuantity = 1;
    struct ItemData {
        uint16_t  itemType;
        const char* name;
        uint32_t  weight;
        uint32_t hpRecovery = 0;
        uint32_t cooldownTicks = 0;
    };

    inline const std::unordered_map<uint32_t, ItemData> itemMap = {
        { 1, { 1, "HP포션",   5, 3000, 40 } },
        { 2, { 1, "MP포션",   5  } },
        { 3, { 2, "철검",   150 } },
        { 4, { 2, "철방패",  200 } },
        { 5, { 3, "가죽갑옷", 80  } },
    };
}
