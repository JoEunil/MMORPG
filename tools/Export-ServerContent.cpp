#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "SkillData.h"
#include "MonsterData.h"
#include "HuntingZoneData.h"
#include "GrowthData.h"
#include "ItemData.h"

namespace {
    void Require(bool valid, const char* message) {
        if (!valid) throw std::runtime_error(message);
    }

    void WriteJsonString(std::ostream& json, const char* text) {
        json << '"';
        for (const auto* character = reinterpret_cast<const unsigned char*>(text); *character; ++character) {
            if (*character == '"' || *character == '\\') json << '\\' << static_cast<char>(*character);
            else if (*character < 0x20) json << "\\u00" << "0123456789abcdef"[*character >> 4] << "0123456789abcdef"[*character & 15];
            else json << static_cast<char>(*character);
        }
        json << '"';
    }

    void Validate() {
        Require(Data::parryWindowTicks > 0 && Data::parryCooldownTicks >= Data::parryWindowTicks && Data::parryCharges == 1,
            "Parry requires one charge and a positive window within its cooldown.");
        Require(Data::itemMap.at(1).hpRecovery > 0 && Data::itemMap.at(1).mpRecovery == 0 &&
            Data::itemMap.at(2).mpRecovery > 0 && Data::itemMap.at(2).hpRecovery == 0 &&
            Data::itemMap.at(1).cooldownTicks == Data::itemMap.at(2).cooldownTicks,
            "HP and MP potion entries require their own recovery resource and a shared cooldown.");
        Require(!Data::itemMap.empty(), "Item definitions cannot be empty.");
        for (const auto& [id, item] : Data::itemMap) {
            Require(id > 0 && item.name != nullptr && item.name[0] != '\0' && item.weight > 0 &&
                item.itemType >= 1 && item.itemType <= 3 && (item.itemType == 2 || item.weaponAttackBonus == 0), "Item IDs, names, types, or weights are invalid.");
            Require(item.hpRecovery <= INT32_MAX && item.mpRecovery <= INT32_MAX &&
                (item.itemType == 1 ? (item.hpRecovery > 0) != (item.mpRecovery > 0) && item.cooldownTicks > 0 :
                item.hpRecovery == 0 && item.mpRecovery == 0 && item.cooldownTicks == 0),
                "Potions require one recovery resource and a cooldown; equipment cannot recover resources.");
        }
        for (const auto* skills : {&Data::skillList, &Data::monsterSkillList}) {
            Require(skills->size() == 5, "Expected skill IDs 0 through 4 for each caster type.");
            for (size_t id = 0; id < skills->size(); ++id) {
                const auto& skill = (*skills)[id];
                Require(skill.skillID == id && (skill.flags & ~3) == 0,
                    "Skill IDs must match array indices and flags must use existing bits.");
                const size_t expectedPhases = id == 3 ? 4 : id == 4 ? 10 : 2;
                Require(skill.phases.size() == expectedPhases, "Existing skill phase counts must be retained.");
                for (size_t i = 0; i < skill.phases.size(); ++i) {
                    const auto& phase = skill.phases[i];
                    const auto range = phase.range.type;
                    Require(phase.actionID == (i & 1) && range <= Data::SkillRangeType::Boss2 &&
                        std::isfinite(phase.range.range1) && std::isfinite(phase.range.range2) &&
                        phase.range.range1 >= 0 && phase.range.range2 >= 0,
                        "Skill phases require existing Charging/Hit order and finite nonnegative ranges.");
                    if (range == Data::SkillRangeType::None) {
                        Require(phase.range.range1 == 0 && phase.range.range2 == 0 && phase.actionID == 0,
                            "Empty skill ranges must have zero dimensions and cannot hit.");
                    }
                    else {
                        Require(phase.range.range1 > 0, "Skill ranges require a positive first dimension.");
                        if (range == Data::SkillRangeType::Rectangle || range == Data::SkillRangeType::Boss1_1 ||
                            range == Data::SkillRangeType::Boss1_2)
                            Require(phase.range.range2 > 0, "This skill range requires a positive second dimension.");
                    }
                }
            }

        }

        Require(std::size(Data::huntingZones) == 6, "Expected hunting zone IDs 1 through 6.");
        for (size_t i = 0; i < std::size(Data::huntingZones); ++i) {
            const auto& zone = Data::huntingZones[i];
            Require(zone.zoneID == i + 1 && zone.label[0] != '\0', "Hunting zones require consecutive IDs and labels.");
            if (i == 0) {
                Require(zone.minLevel == 0 && zone.maxLevel == 0 && zone.regularHp == 0 && zone.strongHp == 0 &&
                    zone.bossHp == 0 && zone.regularAttack == 0 && zone.strongAttack == 0 && zone.bossAttack == 0 &&
                    zone.regularExp == 0 && zone.strongExp == 0 && zone.bossExp == 0 && zone.regularGold == 0 && zone.strongGold == 0,
                    "Town cannot contain hunting stats.");
                continue;
            }
            Require(zone.minLevel > 0 && zone.maxLevel >= zone.minLevel && zone.regularHp > 0 &&
                zone.strongHp > zone.regularHp && zone.strongHp <= INT32_MAX && zone.bossHp <= INT32_MAX &&
                zone.regularAttack > 0 && zone.strongAttack > zone.regularAttack && zone.regularExp > 0 &&
                zone.strongExp == 3 * zone.regularExp && zone.regularGold > 0 && zone.strongGold == 3 * zone.regularGold,
                "Hunting stats must be positive and stronger monsters require triple rewards.");
            Require((zone.zoneID == 4 || zone.zoneID == 6) ? zone.bossHp > 0 && zone.bossAttack > 0 && zone.bossExp > 0 :
                zone.bossHp == 0 && zone.bossAttack == 0 && zone.bossExp == 0, "Only zones 4 and 6 contain bosses.");
        }

        Require(std::size(Data::monsters) == 3, "Expected monster IDs 0 through 2.");
        for (size_t id = 0; id < std::size(Data::monsters); ++id) {
            const auto& monster = Data::monsters[id];
            Require(monster.id == id && monster.maxHp > 0 && monster.maxHp <= INT32_MAX &&
                monster.respawn > 0 && std::isfinite(monster.moveSpeed) && monster.moveSpeed > 0,
                "Monster IDs, signed-wire HP, respawn ticks, or move speed are invalid.");
            Require(monster.phasesCnt >= 1 && monster.phasesCnt <= Data::MAX_PHASES,
                "Monsters require 1 through 8 phases.");
            float previousThreshold = std::numeric_limits<float>::infinity();
            uint32_t previousAbsoluteThreshold = UINT32_MAX;
            for (size_t i = 0; i < monster.phasesCnt; ++i) {
                const auto& phase = monster.phases[i];
                Require(std::isfinite(phase.hpThreshold) && phase.hpThreshold > 0 && phase.hpThreshold <= 1 &&
                    phase.hpThreshold < previousThreshold,
                    "Monster HP thresholds must be finite, positive, at most 1, and strictly decreasing.");
                previousThreshold = phase.hpThreshold;
                if (i + 1 < monster.phasesCnt) {
                    Require(phase.hpThresholdAbs >= 2 && phase.hpThresholdAbs < previousAbsoluteThreshold,
                        "Non-final phases require decreasing absolute thresholds above one HP.");
                    previousAbsoluteThreshold = phase.hpThresholdAbs;
                }
                Require(phase.stepCnt >= 1 && phase.stepCnt <= Data::MAX_STEPS_PER_PHASE &&
                    (phase.stepCnt & (phase.stepCnt - 1)) == 0,
                    "Monster phase step count must be a power of two from 1 through 16.");
                for (size_t step = 0; step < phase.stepCnt; ++step)
                    Require(phase.steps[step].skillID < Data::monsterSkillList.size(), "Monster references an unknown skill ID.");
            }
        }
        Require(Data::growthMaxLevel == 100 && std::size(Data::growthLevels) == Data::growthMaxLevel,
            "Growth requires the complete level 1 through 100 table.");
        for (size_t i = 0; i < std::size(Data::growthLevels); ++i) {
            const auto& growth = Data::growthLevels[i];
            Require(growth.level == i + 1 && growth.maxHpIncrease <= INT32_MAX,
                "Growth levels must be consecutive and HP increases must fit the signed wire value.");
            if (i == 0)
                Require(growth.requiredTotalExp == 0 && growth.maxHpIncrease == 0,
                    "Level 1 requires zero total EXP and zero HP increase.");
            else
                Require(growth.requiredTotalExp > Data::growthLevels[i - 1].requiredTotalExp,
                    "Growth total EXP thresholds must strictly increase.");
        }
    }

    std::string Serialize() {
        std::ostringstream json;
        json.imbue(std::locale::classic());
        json << std::setprecision(std::numeric_limits<float>::max_digits10);
        json << "{\n  \"formatVersion\": 4,\n  \"skills\": [\n";
        for (const auto* skills : {&Data::skillList, &Data::monsterSkillList}) {
            if (skills == &Data::monsterSkillList) json << "\n  ],\n  \"monsterSkills\": [\n";
            for (size_t id = 0; id < skills->size(); ++id) {
                const auto& skill = (*skills)[id];
                if (id) json << ",\n";
                json << "    {\"skillID\":" << skill.skillID << ",\"flags\":" << unsigned(skill.flags)
                    << ",\"mana\":" << skill.mana << ",\"cooldownTicks\":" << skill.coolDown << ",\"phases\":[";
                for (size_t i = 0; i < skill.phases.size(); ++i) {
                    const auto& phase = skill.phases[i];
                    if (i) json << ',';
                    json << "{\"waitTicks\":" << phase.tick << ",\"action\":" << unsigned(phase.actionID)
                        << ",\"range\":" << unsigned(phase.range.type) << ",\"range1\":" << phase.range.range1
                        << ",\"range2\":" << phase.range.range2 << ",\"attack\":" << phase.attack << '}';
                }
                json << "]}";
            }
        }
        json << "\n  ],\n  \"huntingZones\": [\n";
        for (size_t i = 0; i < std::size(Data::huntingZones); ++i) {
            const auto& zone = Data::huntingZones[i];
            if (i) json << ",\n";
            json << "    {\"zoneID\":" << zone.zoneID << ",\"label\":\"" << zone.label << "\""
                << ",\"minLevel\":" << zone.minLevel << ",\"maxLevel\":" << zone.maxLevel
                << ",\"regularHp\":" << zone.regularHp << ",\"strongHp\":" << zone.strongHp << ",\"bossHp\":" << zone.bossHp
                << ",\"regularAttack\":" << zone.regularAttack << ",\"strongAttack\":" << zone.strongAttack << ",\"bossAttack\":" << zone.bossAttack
                << ",\"regularExp\":" << zone.regularExp << ",\"strongExp\":" << zone.strongExp << ",\"bossExp\":" << zone.bossExp
                << ",\"regularGold\":" << zone.regularGold << ",\"strongGold\":" << zone.strongGold << '}';
        }
        json << "\n  ],\n  \"monsters\": [\n";
        for (size_t id = 0; id < std::size(Data::monsters); ++id) {
            const auto& monster = Data::monsters[id];
            if (id) json << ",\n";
            json << "    {\"id\":" << monster.id << ",\"maxHp\":" << monster.maxHp << ",\"attack\":" << monster.attack
                << ",\"respawnTicks\":" << monster.respawn << ",\"moveSpeed\":" << monster.moveSpeed
                << ",\"rewardExp\":" << monster.reward.exp << ",\"phases\":[";
            for (size_t i = 0; i < monster.phasesCnt; ++i) {
                const auto& phase = monster.phases[i];
                if (i) json << ',';
                json << "{\"hpThreshold\":" << phase.hpThreshold << ",\"steps\":[";
                for (size_t step = 0; step < phase.stepCnt; ++step) {
                    if (step) json << ',';
                    json << "{\"skillID\":" << phase.steps[step].skillID << ",\"delayTicks\":" << phase.steps[step].delayTicks << '}';
                }
                json << "]}";
            }
            json << "]}";
        }
        json << "\n  ],\n  \"growth\": {\"maxLevel\":" << Data::growthMaxLevel << ",\"levels\":[\n";
        for (size_t i = 0; i < std::size(Data::growthLevels); ++i) {
            const auto& growth = Data::growthLevels[i];
            if (i) json << ",\n";
            json << "    {\"level\":" << growth.level << ",\"requiredTotalExp\":" << growth.requiredTotalExp
                << ",\"maxHpIncrease\":" << growth.maxHpIncrease << '}';
        }
        json << "\n  ]},\n  \"items\":[";
        std::vector<uint32_t> itemIDs;
        for (const auto& entry : Data::itemMap) itemIDs.push_back(entry.first);
        std::sort(itemIDs.begin(), itemIDs.end());
        for (size_t index = 0; index < itemIDs.size(); ++index) {
            const uint32_t itemID = itemIDs[index];
            const auto& item = Data::itemMap.at(itemID);
            if (index != 0) json << ',';
            json << "{\"itemID\":" << itemID << ",\"itemType\":" << item.itemType << ",\"name\":";
            WriteJsonString(json, item.name);
            json << ",\"weight\":" << item.weight << ",\"weaponAttackBonus\":" << item.weaponAttackBonus << ",\"hpRecovery\":" << item.hpRecovery
                << ",\"mpRecovery\":" << item.mpRecovery << ",\"cooldownTicks\":" << item.cooldownTicks << '}';
        }
        json << "],\n  \"parry\":{\"windowTicks\":" << Data::parryWindowTicks << ",\"cooldownTicks\":" << Data::parryCooldownTicks << ",\"charges\":" << Data::parryCharges << "}\n}\n";
        return json.str();
    }

    void WriteAtomically(const std::filesystem::path& output, const std::string& json) {
        const DWORD attributes = GetFileAttributesW(output.c_str());
        const bool exists = attributes != INVALID_FILE_ATTRIBUTES;
        if (!exists)
            Require(GetLastError() == ERROR_FILE_NOT_FOUND, "Cannot inspect the output file; its parent directory must exist.");
        Require(!exists || (attributes & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_DIRECTORY)) == 0,
            "Output destination is read-only or a directory.");

        // The stage is on the same volume; a failed write never exposes partial JSON.
        wchar_t stage[MAX_PATH]{};
        Require(GetTempFileNameW(output.parent_path().c_str(), L"sct", 0, stage) != 0, "Cannot create output stage.");
        HANDLE file = INVALID_HANDLE_VALUE;
        try {
            file = CreateFileW(stage, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            Require(file != INVALID_HANDLE_VALUE, "Cannot open output stage.");
            size_t offset = 0;
            while (offset < json.size()) {
                const DWORD count = static_cast<DWORD>(std::min<size_t>(json.size() - offset, MAXDWORD));
                DWORD written = 0;
                Require(WriteFile(file, json.data() + offset, count, &written, nullptr) && written > 0,
                    "Cannot write output stage.");
                offset += written;
            }
            Require(FlushFileBuffers(file) != 0, "Cannot flush output stage.");
            const BOOL closed = CloseHandle(file);
            file = INVALID_HANDLE_VALUE;
            Require(closed != 0, "Cannot close output stage.");
            Require(MoveFileExW(stage, output.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0,
                "Cannot atomically rename output stage.");
        }
        catch (...) {
            if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
            DeleteFileW(stage);
            throw;
        }
    }
}

int wmain(int argc, wchar_t* argv[]) {
    try {
        Require(argc == 2 && argv[1][0] != L'\0', "Usage: Export-ServerContent.exe <outputPath>");
        Validate();
        WriteAtomically(std::filesystem::absolute(argv[1]), Serialize());
        std::cout << "Exported player/monster skills, 6 hunting zones, 3 monsters, 100 growth levels, and " << Data::itemMap.size() << " items.\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Server content export failed: " << error.what() << '\n';
        return 1;
    }
}
