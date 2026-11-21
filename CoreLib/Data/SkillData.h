#pragma once
#include <cstdint>
#include <filesystem>
#include <algorithm>
#include <vector>

namespace Data {
    inline constexpr uint32_t parryWindowTicks = 5;
    inline constexpr uint32_t parryCooldownTicks = 60;
    inline constexpr uint32_t parryCharges = 1;
    inline constexpr uint8_t parrySkillSlot = 254;
    inline constexpr uint32_t parrySkillID = UINT32_MAX;
    enum class SkillRangeType : uint8_t {
        None = 0,
        Circle = 1,     // 원형
        Rectangle = 2,  // 직사각형 (전방)
        Boss1_1 = 3,    // 보스 방사형
        Boss1_2 = 4,
        Boss2 = 5,      // 보스 직선 파동형
    };
    enum class SkillActionType : uint8_t {
        Charging = 0,
        Hit = 1,
    };

    namespace SkillFlags {
        inline constexpr uint8_t None = 0;
        inline constexpr uint8_t Critical = 1 << 0;
        inline constexpr uint8_t AreaOfEffect = 1 << 1;
    }

    struct SkillRange {
        SkillRangeType type;
        float range1; 
        float range2;
        // skill range 계산에 필요한 필드
        bool InRange(uint8_t dir, float cx, float cy, float mx, float my) const;
        bool InRangeAngle(float aimAngle, float cx, float cy, float mx, float my) const;
    };

    struct SkillPhase {
        uint32_t tick; // 해당 phase 처리까지 wait tick, 처리후 다음 phase 전환
        SkillActionType actionID;
        SkillRange range;
        uint32_t attack;
    };

    struct SkillData {
        uint16_t skillID;
        uint8_t flags; // SkillFlags bitmask
        uint32_t mana;
        uint32_t coolDown;
        std::vector<SkillPhase> phases;
    };

    extern const std::vector<SkillData>& skillList;
    extern const std::vector<SkillData>& monsterSkillList;

    std::vector<SkillData> LoadSkillData(const std::filesystem::path& skillsPath, const std::filesystem::path& phasesPath);
    void InitializeSkillData(const std::filesystem::path& directory);
}
