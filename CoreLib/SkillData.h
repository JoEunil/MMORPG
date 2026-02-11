#pragma once
#include <cstdint>
#include <algorithm>
#include <vector>

namespace Data {
    enum class SkillRangeType : uint8_t {
        None,       
        Circle,         // 원형
        Rectangle,      // 직사각형 (전방)
        Boss1_1,          // 보스 방사형 
        Boss1_2,        
        Boss2,           // 보스 직선 파동형 
    };
    enum class SkillActionType : uint8_t {
        Charging = 0,
        Hit = 1,
    };

    struct SkillRange {
        SkillRangeType type;
        float range1; 
        float range2;
        // skill range 계산에 필요한 필드
        bool InRange(uint8_t dir, float cx, float cy, float mx, float my) const;
    };

    struct SkillPhase {
        uint32_t tick; // 해당 phase 처리까지 wait tick, 처리후 다음 phase 전환
        uint8_t actionID; // 0: charging, 1:hit
        SkillRange range;
        uint32_t attack;
    };

    struct SkillData {
        uint16_t skillID;
        uint8_t flags;      // bit0: crit, bit2: crowd
        uint32_t mana;
        uint32_t coolDown;
        std::vector<SkillPhase> phases;
    };

    extern const std::vector<SkillData> skillList;
} 