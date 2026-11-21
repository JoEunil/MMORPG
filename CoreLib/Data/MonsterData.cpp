#include "pch.h"
#include "MonsterData.h"
#include "../HuntingZoneData.h"

#include <cstddef>
#include <stdexcept>

namespace Data {
    const MonsterData monsters[] = {
#include "Monsters.inc"
    };

    namespace {
        void ApplyHuntingStats(MonsterData& monster, const HuntingZoneData& source, std::size_t type) {
            monster.maxHp = type == 0 ? source.regularHp : type == 1 ? source.strongHp : source.bossHp;
            monster.attack = type == 0 ? source.regularAttack : type == 1 ? source.strongAttack : source.bossAttack;
            monster.reward = {
                type == 0 ? source.regularExp : type == 1 ? source.strongExp : source.bossExp,
                type == 0 ? source.regularGold : type == 1 ? source.strongGold : 0,
            };
            for (std::size_t phase = 0; phase < monster.phasesCnt; ++phase)
                monster.phases[phase].ComputeThreshold(monster.maxHp);
        }

        void ConfigureHuntingSkillPatterns(std::array<std::array<MonsterData, 3>, 5>& result) {
            // Zone 3 strong monsters trade the circular special for the quarry slam.
            result[1][1].phases[0].steps[2].skillID = 2;
            result[2][1].phases[0].steps[2] = {5, 20};
            result[3][1].phases[0].steps[2] = {6, 20};
            result[3][1].phases[0].steps[3].delayTicks = 26;
            result[4][1].phases[0].steps[1] = {2, 20};
            result[4][1].phases[0].steps[2] = {7, 26};
            result[4][1].phases[0].steps[3].delayTicks = 30;
        }

        std::array<std::array<MonsterData, 3>, 5> BuildHuntingMonsters() {
            std::array<std::array<MonsterData, 3>, 5> result{{
                {monsters[0], monsters[1], monsters[2]},
                {monsters[0], monsters[1], monsters[2]},
                {monsters[0], monsters[1], monsters[2]},
                {monsters[0], monsters[1], monsters[2]},
                {monsters[0], monsters[1], monsters[2]},
            }};
            for (std::size_t zone = 0; zone < result.size(); ++zone) {
                const auto& source = huntingZones[zone + 1];
                for (std::size_t type = 0; type < result[zone].size(); ++type)
                    ApplyHuntingStats(result[zone][type], source, type);
            }
            ConfigureHuntingSkillPatterns(result);
            return result;
        }
    }

    // Keep this after monsters in the same translation unit so base data is initialized before it is copied.
    const std::array<std::array<MonsterData, 3>, 5> huntingMonsters = BuildHuntingMonsters();

    const MonsterData& GetHuntingMonsterData(uint16_t zoneID, uint16_t monsterType) {
        if (zoneID < 2 || zoneID > 6 || monsterType > 2 || (monsterType == 2 && huntingZones[zoneID - 1].bossHp == 0))
            throw std::out_of_range("Monster type is not available in this hunting zone.");
        return huntingMonsters[zoneID - 2][monsterType];
    }
}
