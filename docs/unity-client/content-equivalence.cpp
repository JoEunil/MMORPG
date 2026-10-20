// Compile with CoreLib/SkillData.cpp and /I<repository root>. No live server or DB.
#include <cstdio>
#include <iterator>
#include "CoreLib/MonsterData.h"

int main() {
    using namespace Data;
    if (skillList.size() != 5 || std::size(monsters) != 3) return 1;
    for (size_t i = 0; i < skillList.size(); ++i) if (skillList[i].skillID != i) return 2;
    for (size_t i = 0; i < std::size(monsters); ++i) if (monsters[i].id != i) return 3;
    const auto& ring = skillList[1];
    if (ring.flags != 2 || ring.mana != 20 || ring.coolDown != 120 || ring.phases.size() != 2) return 4;
    const auto& charge = ring.phases[0];
    const auto& hit = ring.phases[1];
    if (charge.tick != 0 || charge.actionID != 0 || charge.range.type != SkillRangeType::None ||
        charge.range.range1 != 0 || charge.range.range2 != 0 || charge.attack != 0) return 5;
    if (hit.tick != 16 || hit.actionID != 1 || hit.range.type != SkillRangeType::Circle ||
        hit.range.range1 != 8 || hit.range.range2 != 0 || hit.attack != 100) return 6;
    const auto& mob = monsters[0];
    if (mob.maxHp != 1500 || mob.attack != 5 || mob.respawn != 300 || mob.moveSpeed != 1 ||
        mob.aggroTimeOut != 100 || mob.phasesCnt != 1 || mob.reward.exp != 10) return 7;
    const auto& phase = mob.phases[0];
    if (phase.hpThreshold != 1 || phase.hpThresholdAbs != 1500 || phase.stepCnt != 2) return 8;
    for (size_t i = 0; i < phase.stepCnt; ++i)
        if (phase.steps[i].skillID != 0 || phase.steps[i].delayTicks != 20) return 9;
    std::puts("PASS: 5 skills / 3 monsters retain ordered IDs; Ring and monster 0 defaults and derived thresholds match.");
    return 0;
}