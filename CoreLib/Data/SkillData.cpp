#include "pch.h"
#include "SkillData.h"
#include "CsvReader.h"

namespace Data {
    namespace {
        std::vector<SkillData> playerSkills;
        std::vector<SkillData> monsterSkills;
        bool skillsInitialized = false;
    }

    const std::vector<SkillData>& skillList = playerSkills;
    const std::vector<SkillData>& monsterSkillList = monsterSkills;

    std::vector<SkillData> LoadSkillData(const std::filesystem::path& skillsPath, const std::filesystem::path& phasesPath) {
        CsvReader skills(skillsPath, {"skillID", "flags", "mana", "coolDown"});
        std::vector<SkillData> result;
        while (skills.ReadRow()) {
            const auto id = skills.GetUnsigned<uint16_t>("skillID");
            if (id != result.size())
                skills.Fail("skillID", "skill IDs must be consecutive from zero");
            const auto flags = skills.GetUnsigned<uint8_t>("flags");
            if ((flags & ~(SkillFlags::Critical | SkillFlags::AreaOfEffect)) != 0)
                skills.Fail("flags", "unknown skill flag bits");
            result.push_back({id, flags, skills.GetUnsigned<uint32_t>("mana"), skills.GetUnsigned<uint32_t>("coolDown"), {}});
        }
        if (result.empty())
            skills.Fail("skillID", "skill catalog cannot be empty");

        CsvReader phases(phasesPath, {"skillID", "phaseIndex", "tick", "actionID", "rangeType", "range1", "range2", "attack"});
        while (phases.ReadRow()) {
            const auto id = phases.GetUnsigned<uint16_t>("skillID");
            if (id >= result.size())
                phases.Fail("skillID", "phase references an unknown skill ID");
            auto& skill = result[id];
            const auto phaseIndex = phases.GetUnsigned<uint16_t>("phaseIndex");
            if (phaseIndex != skill.phases.size() || skill.phases.size() >= UINT16_MAX)
                phases.Fail("phaseIndex", "phases must be consecutive from zero and fit the runtime phase counter");
            const auto action = phases.GetUnsigned<uint8_t>("actionID");
            if (action > static_cast<uint8_t>(SkillActionType::Hit))
                phases.Fail("actionID", "expected 0 (Charging) or 1 (Hit)");
            const auto range = phases.GetUnsigned<uint8_t>("rangeType");
            if (range > static_cast<uint8_t>(SkillRangeType::Boss2))
                phases.Fail("rangeType", "unknown skill range type");
            const float range1 = phases.GetFloat("range1");
            const float range2 = phases.GetFloat("range2");
            if (range1 < 0)
                phases.Fail("range1", "range must be nonnegative");
            if (range2 < 0)
                phases.Fail("range2", "range must be nonnegative");
            const auto rangeType = static_cast<SkillRangeType>(range);
            if (rangeType == SkillRangeType::None) {
                if (range1 != 0) phases.Fail("range1", "empty ranges must have zero dimensions");
                if (range2 != 0) phases.Fail("range2", "empty ranges must have zero dimensions");
                if (action != static_cast<uint8_t>(SkillActionType::Charging))
                    phases.Fail("actionID", "empty ranges cannot hit");
            }
            else {
                if (range1 == 0)
                    phases.Fail("range1", "nonempty ranges require a positive first dimension");
                if ((rangeType == SkillRangeType::Rectangle || rangeType == SkillRangeType::Boss1_1 || rangeType == SkillRangeType::Boss1_2)
                    && range2 == 0)
                    phases.Fail("range2", "this range type requires a positive second dimension");
            }
            skill.phases.push_back({phases.GetUnsigned<uint32_t>("tick"), static_cast<SkillActionType>(action),
                {rangeType, range1, range2}, phases.GetUnsigned<uint32_t>("attack")});
        }
        for (const auto& skill : result)
            if (skill.phases.empty())
                phases.Fail("skillID", "missing phases for skill " + std::to_string(skill.skillID));
        return result;
    }

    void InitializeSkillData(const std::filesystem::path& directory) {
        if (skillsInitialized)
            throw std::logic_error("Skill data is already initialized.");
        auto players = LoadSkillData(directory / "PlayerSkills.csv", directory / "PlayerSkillPhases.csv");
        auto monsters = LoadSkillData(directory / "MonsterSkills.csv", directory / "MonsterSkillPhases.csv");
        playerSkills.swap(players);
        monsterSkills.swap(monsters);
        skillsInitialized = true;
    }
}
