#pragma once
#include <cstdint>
#include "Config.h"

namespace Core {
    struct SkillSlotEntry {
        uint16_t skillID;
        uint32_t skillCoolDownTick;
    };
    struct CharacterState {
        uint32_t zoneInternalID; // zone 내부에서 사용하는 id
        uint32_t profileId;      // 공개 프로필 키. 이름은 여기 담지 않는다.
        uint32_t profileVersion; //9 rename 시 증가하며 delta로 전파된다.
        int hp; // 0
        int mp; // 1
        int maxHp; // 2
        int maxMp; // 3
        uint32_t exp; // 4
        uint16_t level; // 5
        uint8_t dir = 0;  // 6
        float x, y; // 7, 8

        // --  내부 정보
        float moveBudget = 0.0f; // 남은 이동 허용 거리. 틱마다 보충되고 이동 시 소비된다.
        uint8_t cellX;
        uint8_t cellY;
        uint16_t cellIdx;

        uint16_t attack; // 기본 공격력
        uint8_t skillSlotCnt; 
        std::vector<SkillSlotEntry> skillSlot;

        uint16_t lastZone;
        uint32_t dirtyBit = 0x00; // 변경된 필드 표시

        uint64_t sessionID;
        uint64_t characterID;
    };
}