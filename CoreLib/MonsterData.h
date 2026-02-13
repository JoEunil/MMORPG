#pragma once
#include <cstdint>
#include "SkillData.h"

namespace Data {
    static constexpr int MAX_STEPS_PER_PHASE = 16;
    static constexpr int MAX_PHASES = 8;

    struct SkillStep {
        uint16_t skillID;
        uint32_t delayTicks; // skill 이후 대기시간
    };

    struct Phase {
        float hpThreshold;                 // phase 진입 기준 HP% (1.0f → 0.0f)
        uint32_t hpThresholdAbs;  // 초기화 시 설정, 나눗셈 비용이 큼.
        uint16_t stepCnt;      // 2의 거듭제곱으로.
        SkillStep steps[MAX_STEPS_PER_PHASE];
        void ComputeThreshold(uint32_t maxHp) {
            hpThresholdAbs = static_cast<uint32_t>(maxHp * hpThreshold);
        }
    };

    struct Reward {
        uint32_t exp = 0;
    };

    struct MonsterData {
        uint16_t id;
        uint32_t maxHp;
        uint32_t attack;
        uint32_t respawn;
        uint32_t aggroTimeOut; // 어그로 풀리는 시점
        float moveSpeed;

        uint8_t phasesCnt;        
        Phase phases[MAX_PHASES];          // phase 1개면 일반 몬스터

        Reward reward;

        MonsterData(
            uint16_t id,
            uint32_t maxHp,
            uint32_t attack,
            uint32_t respawn,
            float moveSpeed,
            std::initializer_list<Phase> phaseList,
            Reward reward)
            :
            id(id), maxHp(maxHp), attack(attack),
            respawn(respawn), moveSpeed(moveSpeed),
            phasesCnt(phaseList.size()),
            reward(reward)
        {
            aggroTimeOut = 100; // 10초간 공격 없을 시. aggro 해제.
            int i = 0;
            for (auto& p : phaseList) {
                phases[i] = p;
                phases[i].ComputeThreshold(maxHp);
                i++;
            }
        }
    };

    // 운영 환경에서는 json/데이터파일로 관리
    inline static const MonsterData monsters[] = {
        {
            0,          // id
            1500,        // maxHp
            5,          // attack
            300,       // respawn
            1.0f,       // moveSpeed
            {
                // Phase 0
                {
                    1.0f,
                    0,
                    2,
                    {
                        {0, 20},
                        {0, 20},
                    }
                },
            },
            {10}            // reward
        },

        {
            1,       
            3000,        
            8,         
            500,    
            1.2f,   
            {
                {
                    1.0f,
                    0,
                    4,
                    {
                        {0, 20},
                        {0, 20},
                        {1, 20},   // 강공격
                        {0, 20},
                    }
                },
            },
            {30}            // reward
        },
        {
            2,
            10000,       
            15,     
            6000,      
            0.6f,      
            {
                // Phase 0 : HP 100%~
                {
                    1.0f,
                    0,
                    2,
                    {
                        {3, 100},    
                        {4, 100}
                    }
                },

                // Phase 1 : HP 50%~
                {
                    0.5f,
                    0,
                    4,
                    {
                        {3, 50},
                        {4, 50},    
                        {3, 50},
                        {4, 50}
                    }
                },

                // Phase 2 : HP 20%~
                {
                    0.2f,
                    0,
                    4,
                    {
                        {4, 25},
                        {3, 25},
                        {4, 25},
                        {3, 25}
                    }
                }
             },
            {300} 
        }
    };
}
