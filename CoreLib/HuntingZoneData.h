#pragma once
#include <array>
#include "MonsterData.h"

namespace Data {
    struct HuntingZoneData {
        uint16_t zoneID;
        const char* label;
        uint16_t minLevel, maxLevel;
        uint32_t regularHp, strongHp, bossHp;
        uint32_t regularAttack, strongAttack, bossAttack;
        uint32_t regularExp, strongExp, bossExp;
        uint64_t regularGold, strongGold;
    };

    // Explicit UTF-8 bytes keep labels independent of the compiler source character set.
    inline constexpr HuntingZoneData huntingZones[] = {
        {1, "\xEB\xA7\x88\xEC\x9D\x84", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {2, "\xEC\x84\xB1\xEB\xAC\xB8\x20\xEB\xB0\x96", 1, 5, 900, 2000, 0, 50, 80, 0, 10, 30, 0, 10, 30},
        {3, "\xEC\x99\xB8\xEA\xB3\xBD\x20\xEC\xB4\x88\xEC\xA7\x80", 6, 10, 1200, 2800, 0, 70, 110, 0, 25, 75, 0, 15, 45},
        {4, "\xED\x8F\x90\xED\x97\x88\x20\xEA\xB8\xB8\xEB\xAA\xA9", 11, 15, 1600, 3800, 24000, 90, 150, 18, 40, 120, 1200, 20, 60},
        {5, "\xEC\x84\xB1\xEA\xB3\xBD\x20\xEC\x99\xB8\xEA\xB3\xBD", 16, 20, 2100, 5000, 0, 120, 190, 0, 55, 165, 0, 30, 90},
        {6, "\xEA\xB9\x8A\xEC\x9D\x80\x20\xEC\x82\xAC\xEB\x83\xA5\xED\x84\xB0", 21, 25, 2700, 6500, 36000, 150, 240, 30, 70, 210, 2100, 40, 120},
    };

    // Each runtime monster keeps a pointer to these immutable, process-lifetime copies.
    inline const auto huntingMonsters = [] {
        std::array<std::array<MonsterData, 3>, 5> result{{
            {monsters[0], monsters[1], monsters[2]},
            {monsters[0], monsters[1], monsters[2]},
            {monsters[0], monsters[1], monsters[2]},
            {monsters[0], monsters[1], monsters[2]},
            {monsters[0], monsters[1], monsters[2]},
        }};
        for (size_t zone = 0; zone < result.size(); ++zone) {
            const auto& source = huntingZones[zone + 1];
            for (size_t type = 0; type < result[zone].size(); ++type) {
                auto& monster = result[zone][type];
                monster.maxHp = type == 0 ? source.regularHp : type == 1 ? source.strongHp : source.bossHp;
                monster.attack = type == 0 ? source.regularAttack : type == 1 ? source.strongAttack : source.bossAttack;
                monster.reward = {
                    type == 0 ? source.regularExp : type == 1 ? source.strongExp : source.bossExp,
                    type == 0 ? source.regularGold : type == 1 ? source.strongGold : 0,
                };
                for (size_t phase = 0; phase < monster.phasesCnt; ++phase)
                    monster.phases[phase].ComputeThreshold(monster.maxHp);
            }
        }
        // Zone 3 strong monsters trade the circular special for the quarry slam.
        result[1][1].phases[0].steps[2].skillID = 2;
        result[2][1].phases[0].steps[2] = {5, 20};
        result[3][1].phases[0].steps[2] = {6, 20};
        result[3][1].phases[0].steps[3].delayTicks = 26;
        result[4][1].phases[0].steps[1] = {2, 20};
        result[4][1].phases[0].steps[2] = {7, 26};
        result[4][1].phases[0].steps[3].delayTicks = 30;
        return result;
    }();

    // Snapshot identity is separate from the three combat archetypes.
    inline constexpr uint16_t HuntingSpeciesID(uint16_t zone, uint16_t archetype) {
        return archetype < 2 && zone >= 2 && zone <= 6
            ? static_cast<uint16_t>(3 + (zone - 2) * 2 + archetype) : archetype;
    }

    struct HuntingSpecies {
        uint16_t id, zone;
        const char* name;
    };

    inline constexpr HuntingSpecies huntingSpecies[] = {
        {3, 2, "\xEC\x84\xB1\xEB\xAC\xB8\x20\xEC\x88\x9C\xEC\xB0\xB0\xEB\xB3\x91"},
        {4, 2, "\xED\x8F\x90\xED\x97\x88\x20\xEA\xB2\xBD\xEB\xB9\x84\xEB\xB3\x91"},
        {5, 3, "\xEC\xB1\x84\xEC\x84\x9D\xEC\x9E\xA5\x20\xEC\x95\xBD\xED\x83\x88\xEC\x9E\x90"},
        {6, 3, "\xEC\xB1\x84\xEC\x84\x9D\xEC\x9E\xA5\x20\xEB\xB6\x84\xEC\x87\x84\xEC\x9E\x90"},
        {7, 4, "\xEC\xB0\xB8\xEB\x82\x98\xEB\xAC\xB4\x20\xEC\x88\xB2\x20\xEC\x82\xAC\xEB\x83\xA5\xEA\xBE\xBC"},
        {8, 4, "\xEC\xB0\xB8\xEB\x82\x98\xEB\xAC\xB4\x20\xEC\x88\xB2\x20\xEC\x88\x98\xED\x98\xB8\xEC\x9E\x90"},
        {9, 5, "\xEC\xA0\x84\xEC\xB4\x88\xEA\xB8\xB0\xEC\xA7\x80\x20\xEC\xB2\x99\xED\x9B\x84\xEB\xB3\x91"},
        {10, 5, "\xEC\xA0\x84\xEC\xB4\x88\xEA\xB8\xB0\xEC\xA7\x80\x20\xED\x8C\x8C\xEC\x88\x98\xEA\xBE\xBC"},
        {11, 6, "\xEC\x84\x9D\xED\x99\x98\x20\xEC\x88\x9C\xEB\xA1\x80\xEC\x9E\x90"},
        {12, 6, "\xEC\x84\x9D\xED\x99\x98\x20\xEC\xA0\x9C\xEB\x8B\xA8\x20\xEC\x88\x98\xED\x98\xB8\xEC\x9E\x90"},
        {2, 4, "\xED\x8F\x90\xED\x97\x88\x20\xEA\xB8\xB8\xEB\xAA\xA9\x20\xEB\xB3\xB4\xEC\x8A\xA4"},
        {2, 6, "\xEC\x84\x9D\xED\x99\x98\x20\xEC\xA0\x9C\xEB\x8B\xA8\x20\xEB\xB3\xB4\xEC\x8A\xA4"},
    };

    inline const MonsterData& GetHuntingMonsterData(uint16_t zoneID, uint16_t monsterType) {
        if (zoneID < 2 || zoneID > 6 || monsterType > 2 || (monsterType == 2 && huntingZones[zoneID - 1].bossHp == 0))
            throw std::out_of_range("Monster type is not available in this hunting zone.");
        return huntingMonsters[zoneID - 2][monsterType];
    }
}
