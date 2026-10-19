#include "pch.h"
#include "SkillData.h"

namespace Data {
    const std::vector<SkillData> skillList = {
#include "../UnityClient/Exports/ServerData/Skills.inc"
    };
}
