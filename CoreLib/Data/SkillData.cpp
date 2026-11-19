#include "pch.h"
#include "SkillData.h"

namespace Data {
    const std::vector<SkillData> skillList = {
#include "Skills.inc"
    };
    const std::vector<SkillData> monsterSkillList = {
#include "MonsterSkills.inc"
    };
}
