#include "pch.h"
#include "SkillData.h"
#if __has_include("../UnityClient/Exports/ServerData/C14Export.pending")
#error C14 server content export is pending; do not compile a mixed generated content snapshot.
#endif

namespace Data {
    const std::vector<SkillData> skillList = {
#include "../UnityClient/Exports/ServerData/Skills.inc"
    };
}
