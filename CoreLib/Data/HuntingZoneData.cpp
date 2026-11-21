#include "pch.h"
#include "../HuntingZoneData.h"
#include "CsvReader.h"
#include "SkillData.h"
#include "GrowthData.h"
#include "../World/WorldConfig.h"
#include <set>
#include <tuple>

namespace Data {
    namespace {
        std::vector<HuntingZoneData> zones;
        std::vector<std::array<MonsterData, 3>> zoneMonsters;
        std::vector<HuntingSpecies> species;
        std::vector<std::array<uint16_t, 3>> speciesIDs;
        bool huntingInitialized = false;

        bool HasHuntingMonster(const std::vector<HuntingZoneData>& definitions, uint16_t zone, uint16_t type) {
            return zone >= 2 && zone <= definitions.size() && type < 3 &&
                (type != 2 || definitions[zone - 1].bossHp != 0);
        }

        std::vector<HuntingZoneData> LoadZones(const std::filesystem::path& path) {
            CsvReader csv(path, {"zoneID", "label", "minLevel", "maxLevel", "regularHp", "strongHp", "bossHp",
                "regularAttack", "strongAttack", "bossAttack", "regularExp", "strongExp", "bossExp", "regularGold", "strongGold"});
            std::vector<HuntingZoneData> result;
            while (csv.ReadRow()) {
                HuntingZoneData zone{csv.GetUnsigned<uint16_t>("zoneID"), std::string(csv.Get("label")),
                    csv.GetUnsigned<uint16_t>("minLevel"), csv.GetUnsigned<uint16_t>("maxLevel"),
                    csv.GetUnsigned<uint32_t>("regularHp"), csv.GetUnsigned<uint32_t>("strongHp"), csv.GetUnsigned<uint32_t>("bossHp"),
                    csv.GetUnsigned<uint32_t>("regularAttack"), csv.GetUnsigned<uint32_t>("strongAttack"), csv.GetUnsigned<uint32_t>("bossAttack"),
                    csv.GetUnsigned<uint32_t>("regularExp"), csv.GetUnsigned<uint32_t>("strongExp"), csv.GetUnsigned<uint32_t>("bossExp"),
                    csv.GetUnsigned<uint64_t>("regularGold"), csv.GetUnsigned<uint64_t>("strongGold")};
                if (zone.zoneID != result.size() + 1 || zone.zoneID > Core::ZONE_COUNT)
                    csv.Fail("zoneID", "zone IDs must be consecutive from 1 and match the world layout");
                if (zone.label.empty() || zone.label.find('\0') != std::string::npos)
                    csv.Fail("label", "zone label must be nonempty and cannot contain a null character");
                if (zone.regularHp > INT32_MAX || zone.strongHp > INT32_MAX || zone.bossHp > INT32_MAX)
                    csv.Fail("regularHp/strongHp/bossHp", "HP must fit INT32_MAX");
                if (zone.zoneID == Core::TOWN_ZONE_ID) {
                    if (zone.minLevel || zone.maxLevel || zone.regularHp || zone.strongHp || zone.bossHp || zone.regularAttack ||
                        zone.strongAttack || zone.bossAttack || zone.regularExp || zone.strongExp || zone.bossExp || zone.regularGold || zone.strongGold)
                        csv.Fail("zoneID", "town cannot contain hunting stats");
                }
                else {
                    if (zone.minLevel == 0 || zone.maxLevel < zone.minLevel || zone.maxLevel > growthMaxLevel)
                        csv.Fail("minLevel/maxLevel", "invalid hunting level interval");
                    if (zone.regularHp == 0 || zone.strongHp == 0)
                        csv.Fail("regularHp/strongHp", "regular and strong monsters require positive HP");
                    if (zone.regularAttack == 0 || zone.strongAttack == 0)
                        csv.Fail("regularAttack/strongAttack", "regular and strong monsters require positive attack");
                    if ((zone.bossHp == 0 && (zone.bossAttack != 0 || zone.bossExp != 0)) || (zone.bossHp != 0 && zone.bossAttack == 0))
                        csv.Fail("bossHp/bossAttack/bossExp", "boss stats must agree with boss availability");
                }
                result.push_back(std::move(zone));
            }
            if (result.size() != Core::ZONE_COUNT)
                csv.Fail("zoneID", "expected one row for every world zone, including town");
            return result;
        }

        void ApplyHuntingStats(MonsterData& monster, const HuntingZoneData& source, std::size_t type) {
            monster.maxHp = type == 0 ? source.regularHp : type == 1 ? source.strongHp : source.bossHp;
            monster.attack = type == 0 ? source.regularAttack : type == 1 ? source.strongAttack : source.bossAttack;
            monster.reward = {type == 0 ? source.regularExp : type == 1 ? source.strongExp : source.bossExp,
                type == 0 ? source.regularGold : type == 1 ? source.strongGold : 0};
            for (std::size_t phase = 0; phase < monster.phasesCnt; ++phase)
                monster.phases[phase].ComputeThreshold(monster.maxHp);
        }

        std::vector<std::array<MonsterData, 3>> BuildHuntingMonsters(const std::vector<HuntingZoneData>& definitions,
            const std::filesystem::path& source) {
            std::vector<std::array<MonsterData, 3>> result;
            result.reserve(definitions.size() - 1);
            for (std::size_t zone = 1; zone < definitions.size(); ++zone) {
                result.push_back({monsters[0], monsters[1], monsters[2]});
                for (std::size_t type = 0; type < result.back().size(); ++type) {
                    auto& monster = result.back()[type];
                    ApplyHuntingStats(monster, definitions[zone], type);
                    if (monster.maxHp != 0)
                        ValidateMonsterData(monster, source);
                }
            }
            return result;
        }

        void ApplySkillOverrides(const std::filesystem::path& path, const std::vector<HuntingZoneData>& definitions,
            std::vector<std::array<MonsterData, 3>>& result) {
            CsvReader csv(path, {"zoneID", "monsterType", "phaseIndex", "stepIndex", "skillID", "delayTicks"});
            std::set<std::tuple<uint16_t, uint16_t, uint8_t, uint16_t>> seen;
            while (csv.ReadRow()) {
                const auto zone = csv.GetUnsigned<uint16_t>("zoneID");
                const auto type = csv.GetUnsigned<uint16_t>("monsterType");
                if (!HasHuntingMonster(definitions, zone, type))
                    csv.Fail("zoneID/monsterType", "unknown or unavailable hunting monster");
                auto& monster = result[zone - 2][type];
                const auto phase = csv.GetUnsigned<uint8_t>("phaseIndex");
                if (phase >= monster.phasesCnt)
                    csv.Fail("phaseIndex", "unknown monster phase");
                const auto step = csv.GetUnsigned<uint16_t>("stepIndex");
                if (step >= monster.phases[phase].stepCnt)
                    csv.Fail("stepIndex", "unknown monster step");
                if (!seen.emplace(zone, type, phase, step).second)
                    csv.Fail("stepIndex", "duplicate hunting skill override");
                const auto skill = csv.GetUnsigned<uint16_t>("skillID");
                if (skill >= monsterSkillList.size())
                    csv.Fail("skillID", "unknown monster skill ID");
                monster.phases[phase].steps[step] = {skill, csv.GetUnsigned<uint32_t>("delayTicks")};
            }
        }

        std::vector<HuntingSpecies> LoadSpecies(const std::filesystem::path& path, const std::vector<HuntingZoneData>& definitions,
            std::vector<std::array<uint16_t, 3>>& ids) {
            CsvReader csv(path, {"speciesID", "zoneID", "monsterType", "name"});
            std::vector<HuntingSpecies> result;
            std::vector<std::array<bool, 3>> seen(definitions.size() + 1);
            ids.assign(definitions.size() + 1, {0, 1, 2});
            while (csv.ReadRow()) {
                HuntingSpecies entry{csv.GetUnsigned<uint16_t>("speciesID"), csv.GetUnsigned<uint16_t>("zoneID"),
                    csv.GetUnsigned<uint16_t>("monsterType"), std::string(csv.Get("name"))};
                if (!HasHuntingMonster(definitions, entry.zone, entry.monsterType))
                    csv.Fail("zoneID/monsterType", "unknown or unavailable hunting monster");
                if (entry.name.empty() || entry.name.find('\0') != std::string::npos)
                    csv.Fail("name", "species name must be nonempty and cannot contain a null character");
                if (seen[entry.zone][entry.monsterType])
                    csv.Fail("monsterType", "duplicate species for this zone and monster type");
                for (std::size_t type = 0; type < 3; ++type)
                    if (seen[entry.zone][type] && ids[entry.zone][type] == entry.id)
                        csv.Fail("speciesID", "species IDs must be unique within a zone");
                seen[entry.zone][entry.monsterType] = true;
                ids[entry.zone][entry.monsterType] = entry.id;
                result.push_back(std::move(entry));
            }
            for (uint16_t zone = 2; zone <= definitions.size(); ++zone)
                for (uint16_t type = 0; type < 3; ++type)
                    if (HasHuntingMonster(definitions, zone, type) && !seen[zone][type])
                        csv.Fail("zoneID/monsterType", "missing species for zone " + std::to_string(zone) + " type " + std::to_string(type));
            return result;
        }
    }

    const std::vector<HuntingZoneData>& huntingZones = zones;
    const std::vector<std::array<MonsterData, 3>>& huntingMonsters = zoneMonsters;
    const std::vector<HuntingSpecies>& huntingSpecies = species;

    void InitializeHuntingData(const std::filesystem::path& directory) {
        if (huntingInitialized)
            throw std::logic_error("Hunting data is already initialized.");
        if (monsters.size() != 3 || monsterSkillList.empty())
            throw std::logic_error("Load skills and base monsters before hunting data.");
        auto loadedZones = LoadZones(directory / "HuntingZones.csv");
        auto loadedMonsters = BuildHuntingMonsters(loadedZones, directory / "HuntingZones.csv");
        ApplySkillOverrides(directory / "HuntingSkillOverrides.csv", loadedZones, loadedMonsters);
        std::vector<std::array<uint16_t, 3>> loadedIDs;
        auto loadedSpecies = LoadSpecies(directory / "HuntingSpecies.csv", loadedZones, loadedIDs);
        zones.swap(loadedZones);
        zoneMonsters.swap(loadedMonsters);
        species.swap(loadedSpecies);
        speciesIDs.swap(loadedIDs);
        huntingInitialized = true;
    }

    uint16_t HuntingSpeciesID(uint16_t zone, uint16_t archetype) {
        return zone < speciesIDs.size() && archetype < 3 ? speciesIDs[zone][archetype] : archetype;
    }

    const MonsterData& GetHuntingMonsterData(uint16_t zoneID, uint16_t monsterType) {
        if (!huntingInitialized)
            throw std::logic_error("Hunting data is not initialized.");
        if (!HasHuntingMonster(zones, zoneID, monsterType))
            throw std::out_of_range("Monster type is not available in this hunting zone.");
        return zoneMonsters[zoneID - 2][monsterType];
    }
}
