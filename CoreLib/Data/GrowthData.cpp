#include "pch.h"
#include "GrowthData.h"
#include "CsvReader.h"

namespace Data {
    namespace {
        std::vector<GrowthLevelData> levels;
        bool growthInitialized = false;
    }

    const std::vector<GrowthLevelData>& growthLevels = levels;

    std::vector<GrowthLevelData> LoadGrowthData(const std::filesystem::path& path) {
        CsvReader csv(path, {"level", "requiredTotalExp", "maxHpIncrease"});
        std::vector<GrowthLevelData> result;
        result.reserve(growthMaxLevel);
        while (csv.ReadRow()) {
            GrowthLevelData row{csv.GetUnsigned<uint16_t>("level"), csv.GetUnsigned<uint32_t>("requiredTotalExp"),
                csv.GetUnsigned<uint32_t>("maxHpIncrease")};
            if (row.level != result.size() + 1 || row.level > growthMaxLevel)
                csv.Fail("level", "levels must be consecutive from 1 through 100");
            if (row.maxHpIncrease > INT32_MAX)
                csv.Fail("maxHpIncrease", "HP increase must not exceed INT32_MAX");
            if (result.empty()) {
                if (row.requiredTotalExp != 0)
                    csv.Fail("requiredTotalExp", "level 1 must require zero total EXP");
                if (row.maxHpIncrease != 0)
                    csv.Fail("maxHpIncrease", "level 1 must have zero HP increase");
            }
            else if (row.requiredTotalExp <= result.back().requiredTotalExp) {
                csv.Fail("requiredTotalExp", "total EXP thresholds must strictly increase");
            }
            result.push_back(row);
        }
        if (result.size() != growthMaxLevel)
            csv.Fail("level", "expected the complete level 1 through 100 table");
        return result;
    }

    void InitializeGrowthData(const std::filesystem::path& path) {
        if (growthInitialized)
            throw std::logic_error("Growth data is already initialized.");
        auto loaded = LoadGrowthData(path);
        levels.swap(loaded);
        growthInitialized = true;
    }
}
