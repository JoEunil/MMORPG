#include "pch.h"
#include "MonsterData.h"
#include "SkillData.h"
#include "CsvReader.h"

namespace Data {
    namespace {
        std::vector<MonsterData> definitions;
        bool monstersInitialized = false;

        std::unordered_map<uint16_t, std::vector<Phase>> LoadPhases(const std::filesystem::path& directory) {
            std::unordered_map<uint16_t, std::vector<Phase>> result;
            CsvReader csv(directory / "MonsterPhases.csv", {"monsterID", "phaseIndex", "hpThreshold"});
            while (csv.ReadRow()) {
                const auto id = csv.GetUnsigned<uint16_t>("monsterID");
                auto& phases = result[id];
                const auto index = csv.GetUnsigned<uint8_t>("phaseIndex");
                if (index != phases.size() || phases.size() >= MAX_PHASES)
                    csv.Fail("phaseIndex", "phases must be consecutive from zero with at most 8 phases");
                const float threshold = csv.GetFloat("hpThreshold");
                if (threshold <= 0 || threshold > 1 || (!phases.empty() && threshold >= phases.back().hpThreshold))
                    csv.Fail("hpThreshold", "HP ratios must strictly decrease within (0, 1]");
                Phase phase{};
                phase.hpThreshold = threshold;
                phases.push_back(phase);
            }
            return result;
        }

        void ReadSkillSteps(const std::filesystem::path& path, std::unordered_map<uint16_t, std::vector<Phase>>& phaseDefinitions) {
            CsvReader csv(path, {"monsterID", "phaseIndex", "stepIndex", "skillID", "delayTicks"});
            while (csv.ReadRow()) {
                const auto id = csv.GetUnsigned<uint16_t>("monsterID");
                const auto found = phaseDefinitions.find(id);
                if (found == phaseDefinitions.end())
                    csv.Fail("monsterID", "step references a monster with no phases");
                const auto phaseIndex = csv.GetUnsigned<uint8_t>("phaseIndex");
                if (phaseIndex >= found->second.size())
                    csv.Fail("phaseIndex", "step references an unknown phase");
                auto& phase = found->second[phaseIndex];
                const auto stepIndex = csv.GetUnsigned<uint16_t>("stepIndex");
                if (stepIndex != phase.stepCnt || phase.stepCnt >= MAX_STEPS_PER_PHASE)
                    csv.Fail("stepIndex", "steps must be consecutive from zero with at most 16 steps");
                const auto skillID = csv.GetUnsigned<uint16_t>("skillID");
                if (skillID >= monsterSkillList.size())
                    csv.Fail("skillID", "unknown monster skill ID");
                phase.steps[phase.stepCnt++] = {skillID, csv.GetUnsigned<uint32_t>("delayTicks")};
            }
        }
    }

    const std::vector<MonsterData>& monsters = definitions;

    void ValidateMonsterData(const MonsterData& monster, const std::filesystem::path& source) {
        const auto prefix = source.string() + " [monsterID=" + std::to_string(monster.id) + "]: ";
        uint32_t previousThreshold = UINT32_MAX;
        for (std::size_t index = 0; index < monster.phasesCnt; ++index) {
            const auto& phase = monster.phases[index];
            if (phase.stepCnt == 0 || phase.stepCnt > MAX_STEPS_PER_PHASE || (phase.stepCnt & (phase.stepCnt - 1)) != 0)
                throw std::runtime_error(prefix + "phase " + std::to_string(index) + " step count must be a power of two from 1 through 16");
            if (index + 1 < monster.phasesCnt) {
                if (phase.hpThresholdAbs < 2 || phase.hpThresholdAbs >= previousThreshold)
                    throw std::runtime_error(prefix + "non-final HP thresholds must strictly decrease above one HP");
                previousThreshold = phase.hpThresholdAbs;
            }
            for (std::size_t step = 0; step < phase.stepCnt; ++step)
                if (phase.steps[step].skillID >= monsterSkillList.size())
                    throw std::runtime_error(prefix + "unknown monster skill ID");
        }
    }

    std::vector<MonsterData> LoadMonsterData(const std::filesystem::path& directory) {
        if (monsterSkillList.empty())
            throw std::logic_error("Load skill data before monster data.");
        auto phases = LoadPhases(directory);
        ReadSkillSteps(directory / "MonsterSkillSteps.csv", phases);
        CsvReader csv(directory / "Monsters.csv", {"monsterID", "maxHp", "attack", "respawn", "aggroTimeOut", "moveSpeed",
            "chaseDistance", "rewardExp", "rewardGold"});
        std::vector<MonsterData> result;
        while (csv.ReadRow()) {
            const auto id = csv.GetUnsigned<uint16_t>("monsterID");
            if (id != result.size() || id > 2)
                csv.Fail("monsterID", "expected consecutive archetypes 0 (regular), 1 (strong), 2 (boss)");
            const auto found = phases.find(id);
            if (found == phases.end() || found->second.empty())
                csv.Fail("monsterID", "monster requires at least one phase");
            const auto hp = csv.GetUnsigned<uint32_t>("maxHp");
            if (hp == 0 || hp > INT32_MAX)
                csv.Fail("maxHp", "HP must be positive and fit INT32_MAX");
            const auto respawn = csv.GetUnsigned<uint32_t>("respawn");
            if (respawn == 0) csv.Fail("respawn", "respawn ticks must be positive");
            const float speed = csv.GetFloat("moveSpeed");
            if (speed <= 0) csv.Fail("moveSpeed", "speed must be positive");
            const float chase = csv.GetFloat("chaseDistance");
            if (chase <= 0) csv.Fail("chaseDistance", "distance must be positive");
            MonsterData monster{id, hp, csv.GetUnsigned<uint32_t>("attack"), respawn, speed, found->second,
                {csv.GetUnsigned<uint32_t>("rewardExp"), csv.GetUnsigned<uint64_t>("rewardGold")}};
            monster.aggroTimeOut = csv.GetUnsigned<uint32_t>("aggroTimeOut");
            monster.chaseDistance = chase;
            ValidateMonsterData(monster, directory / "MonsterPhases.csv");
            result.push_back(monster);
        }
        if (result.size() != 3)
            csv.Fail("monsterID", "expected all three monster archetypes");
        for (const auto& [id, unused] : phases)
            if (id >= result.size())
                throw std::runtime_error((directory / "MonsterPhases.csv").string() + ": unknown monster ID " + std::to_string(id));
        return result;
    }

    void InitializeMonsterData(const std::filesystem::path& directory) {
        if (monstersInitialized)
            throw std::logic_error("Monster data is already initialized.");
        auto loaded = LoadMonsterData(directory);
        definitions.swap(loaded);
        monstersInitialized = true;
    }
}
