#pragma once
#include <cstdint>
#include <stdexcept>
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
        float chaseDistance = 20.0f; // Distance from the monster's position when combat starts.

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
            if (phaseList.size() < 1 || phaseList.size() > MAX_PHASES)
                throw std::invalid_argument("Monsters require 1 through 8 phases.");
            aggroTimeOut = 100; // 50ms 틱 기준 5초간 교전이 없으면 어그로 해제.
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
#include "Monsters.inc"
    };
}
