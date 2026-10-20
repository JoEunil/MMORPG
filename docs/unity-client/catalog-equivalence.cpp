// Baseline copies are from the parent commit under CatalogBaseline/.
// Compile with current CoreLib/SkillData.cpp; include repository, CoreLib, and baseline parent.
#include "CoreLib/pch.h"
#define Data CatalogBaselineData
#include "CatalogBaseline/CoreLib/MonsterData.h"
#include "CatalogBaseline/CoreLib/SkillData.cpp"
#undef Data
#include "CoreLib/MonsterData.h"
#include <cstdio>
#include <iterator>

int main() {
    const auto& before = CatalogBaselineData::skillList;
    const auto& after = Data::skillList;
    if (before.size() != 5 || after.size() != before.size()) return 1;
    for (size_t i = 0; i < before.size(); ++i) {
        const auto& a = before[i]; const auto& b = after[i];
        if (a.skillID != b.skillID || b.skillID != i || a.flags != b.flags || a.mana != b.mana ||
            a.coolDown != b.coolDown || a.phases.size() != b.phases.size()) return 2;
        for (size_t p = 0; p < a.phases.size(); ++p) {
            const auto& x = a.phases[p]; const auto& y = b.phases[p];
            if (x.tick != y.tick || x.actionID != y.actionID || x.attack != y.attack ||
                static_cast<int>(x.range.type) != static_cast<int>(y.range.type) ||
                x.range.range1 != y.range.range1 || x.range.range2 != y.range.range2) return 3;
        }
    }
    if (std::size(CatalogBaselineData::monsters) != 3 || std::size(Data::monsters) != 3) return 4;
    for (size_t i = 0; i < std::size(Data::monsters); ++i) {
        const auto& a = CatalogBaselineData::monsters[i]; const auto& b = Data::monsters[i];
        if (a.id != b.id || b.id != i || a.maxHp != b.maxHp || a.attack != b.attack ||
            a.respawn != b.respawn || a.moveSpeed != b.moveSpeed || a.aggroTimeOut != b.aggroTimeOut ||
            a.reward.exp != b.reward.exp || a.phasesCnt != b.phasesCnt) return 5;
        for (size_t p = 0; p < a.phasesCnt; ++p) {
            const auto& x = a.phases[p]; const auto& y = b.phases[p];
            if (x.hpThreshold != y.hpThreshold || x.hpThresholdAbs != y.hpThresholdAbs || x.stepCnt != y.stepCnt) return 6;
            for (size_t s = 0; s < x.stepCnt; ++s)
                if (x.steps[s].skillID != y.steps[s].skillID || x.steps[s].delayTicks != y.steps[s].delayTicks) return 7;
        }
    }
    std::puts("PASS: all fields of 5 skills and 3 monsters, ordered IDs, phase/step durations, ranges, rewards and derived values match the parent C++ tables.");
    return 0;
}