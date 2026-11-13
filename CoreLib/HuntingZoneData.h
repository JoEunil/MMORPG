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
        return result;
    }();

    inline const MonsterData& GetHuntingMonsterData(uint16_t zoneID, uint16_t monsterType) {
        if (zoneID < 2 || zoneID > 6 || monsterType > 2 || (monsterType == 2 && huntingZones[zoneID - 1].bossHp == 0))
            throw std::out_of_range("Monster type is not available in this hunting zone.");
        return huntingMonsters[zoneID - 2][monsterType];
    }
}
