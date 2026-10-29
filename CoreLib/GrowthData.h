#pragma once
#include <cstdint>

namespace Data {
    struct GrowthLevelData {
        uint16_t level;
        uint32_t requiredTotalExp;
        uint32_t maxHpIncrease;
    };

    inline constexpr uint16_t growthMaxLevel = 100;
    inline constexpr GrowthLevelData growthLevels[] = {
#include "Growth.inc"
    };
}
