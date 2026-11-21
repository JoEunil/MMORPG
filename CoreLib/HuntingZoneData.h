#pragma once
#include <array>
#include <string>
#include <vector>
#include "Data/MonsterData.h"

namespace Data {
    struct HuntingZoneData {
        uint16_t zoneID;
        std::string label;
        uint16_t minLevel, maxLevel;
        uint32_t regularHp, strongHp, bossHp;
        uint32_t regularAttack, strongAttack, bossAttack;
        uint32_t regularExp, strongExp, bossExp;
        uint64_t regularGold, strongGold;
    };

    struct HuntingSpecies {
        uint16_t id, zone, monsterType;
        std::string name;
    };

    extern const std::vector<HuntingZoneData>& huntingZones;
    extern const std::vector<std::array<MonsterData, 3>>& huntingMonsters;
    extern const std::vector<HuntingSpecies>& huntingSpecies;

    void InitializeHuntingData(const std::filesystem::path& directory);
    uint16_t HuntingSpeciesID(uint16_t zone, uint16_t archetype);
    const MonsterData& GetHuntingMonsterData(uint16_t zoneID, uint16_t monsterType);
}
