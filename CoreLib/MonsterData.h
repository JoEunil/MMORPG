#pragma once
#include <cstdint>
#include "SkillData.h"

namespace Data {
    static constexpr int MAX_STEPS_PER_PHASE = 16;
    static constexpr int MAX_PHASES = 8;

    struct PatternStep {
        uint16_t skillID;
        uint32_t delayTicks; // skill 이후 대기시간
    };

    struct PhasePattern {
        float hpThreshold;
        PatternStep steps[MAX_STEPS_PER_PHASE];
    };

    struct AttackPattern {
        PhasePattern phases[MAX_PHASES];

        // 패턴 전환 대기시간
        uint32_t delay = 100;
    };

    struct Reward {
        uint32_t exp = 0;
    };

    struct MonsterData {
        uint16_t id;
        uint32_t maxHp;
        uint32_t attack;
        uint32_t respawn;
        float moveSpeed;

        AttackPattern pattern;

        Reward reword;
    };

    // 운영 환경에서는 json으로 관리하고 binary로 파싱하여 사용. (.dat)
    inline const MonsterData monsters[] = {
        // 일반 몬스터 1
        {
            0,          // id
            100,        // maxHp
            5,          // attack
            3000,       // respawn
            1.0f,       // moveSpeed
            {
                {
                    // Phase 0 (HP 100% ~ 0%)
                    {
                        1.0f,      // hpThreshold
                        {
                            {0, 50},  // 기본공격, 1초 주기
                            { 0, 0 },
                        }
                    },
                },
                0 // delay
            },
            {10} // reward
        },

        {
            1,          // id
            300,        // maxHp
            8,          // damage
            5000,       // respawn
            1.2f,       // moveSpeed
            {
                {
                    {
                        1.0f,
                        {
                            {0, 700}, // 기본공격
                            {0, 700},
                            {1, 1200}, // 스킬1(강한 공격)
                            {0, 0},
                        }
                    },
                },
                200
            },

            {30} // reward
        },

        {
            2,          // id
            5000,       // maxHp
            20,         // damage
            15000,      // respawn
            0.6f,       // moveSpeed (느린 보스)

            {
                {
                    {
                        1.0f,
                        {
                            {3, 2000},   // 보스 스킬(방사형)
                            {0, 0}
                        }
                    },
                    // Phase 1 : HP 50%~20%
                    {
                        0.5f,
                        {
                            {3, 1500},
                            {4, 1500},  // 보스 패턴 2 (직선 파동)
                            {3, 1500},
                            {0, 0},
                        }
                    },
                    // Phase 2 : HP 20% 이하 (광폭화)
                    {
                        0.2f,
                        {
                            {4, 1000},
                            {3, 1000},
                            {4, 800},
                            {0, 0},
                        }
                    },
                {0.0f, {}}, {0.0f, {}}, {0.0f, {}}, {0.0f, {}}, {0.0f, {}}
            },

            50 // phase 전환 후 1초 (50 tick) 지연
        },

        {300} // reward
        }
    };
}