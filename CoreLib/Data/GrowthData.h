#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>

namespace Data {
    struct GrowthLevelData {
        uint16_t level;
        uint32_t requiredTotalExp;
        uint32_t maxHpIncrease;
    };

    inline constexpr uint16_t growthMaxLevel = 100;
    extern const std::vector<GrowthLevelData>& growthLevels;

    std::vector<GrowthLevelData> LoadGrowthData(const std::filesystem::path& path);
    void InitializeGrowthData(const std::filesystem::path& path);
}
