#include "pch.h"
#include "SkillData.h"

namespace Data {
    const std::vector<SkillData> skillList = {
        // 기본 공격
        SkillData {
            0, 0,           // skillID, flags
            1, 20,         // mana, cooldown
            {
                // wait tick, actionType, range, attack
                SkillPhase{ 0, 0, {SkillRangeType::None, 0, 0}, 50 }, // 스킬 애니메이션 시작
                SkillPhase{ 6, 1, {SkillRangeType::Circle, 2.0f, 0.0f}, 5 }, // 타격 처리
            },
        },
         // 원형 스킬 광역기
        SkillData{
            1, 0,
            20, 120,
            {
                SkillPhase{ 0, 0, {SkillRangeType::None, 0, 0}, 0 },  // charging
                SkillPhase{ 16, 1, {SkillRangeType::Circle, 8.0f, 0}, 100 } // hit
            }
        },
        // 바라보는 방향 직사각형
        SkillData{
            2, 0,
            20, 80,
            {
                SkillPhase{ 0, 0, {SkillRangeType::None, 0, 0}, 0 },  // charging
                SkillPhase{ 10, 1, {SkillRangeType::Rectangle, 12.0f, 3.0f}, 100 } // hit
            }
        },
        //보스 스킬 1 방사형
        SkillData{
                3, 0,
                0, 0,
            {
                SkillPhase{ 0, 0, {SkillRangeType::None, 0, 0}, 0 },  // charging
                SkillPhase{ 20, 1, {SkillRangeType::Boss1_1, 10.0f, 30}, 100 }, // hit 1
                SkillPhase{ 10, 0, {SkillRangeType::None, 0, 0}, 0 },  // charging
                SkillPhase{ 20, 1, {SkillRangeType::Boss1_2, 10.0f, 30}, 100 } // hit 2
            }
        },
        // 보스 스킬 2 파동형
        SkillData{
                4, 0,
                0, 0,
            {
                SkillPhase{ 0, 0, {SkillRangeType::None, 0, 0}, 0 },  // charging
                SkillPhase{ 20, 1, {SkillRangeType::Boss2, 5.0f, 0}, 100 }, // hit 1
                SkillPhase{ 0, 0, {SkillRangeType::None, 0, 0}, 0 },  // charging
                SkillPhase{ 20, 1, {SkillRangeType::Boss2, 10.0f, 5.0f}, 100 }, // hit 2
                SkillPhase{ 0, 0, {SkillRangeType::None, 0, 0}, 0 },  // charging
                SkillPhase{ 20, 1, {SkillRangeType::Boss2, 15.0f, 10.0f}, 100 }, // hit 3 
                SkillPhase{ 0, 0, {SkillRangeType::None, 0, 0}, 0 },  // charging
                SkillPhase{ 20, 1, {SkillRangeType::Boss2, 20.0f, 15.0f}, 100 }, // hit 4 
                SkillPhase{ 0, 0, {SkillRangeType::None, 0, 0}, 0 },  // charging
                SkillPhase{ 20, 1, {SkillRangeType::Boss2, 25.0f, 20.0f}, 100 }, // hit 5
            }
        },
    };
}