#pragma once
#include <cstdint>
#include "Config.h"

namespace Core {
    struct SkillSlotEntry {
        uint16_t skillID;
        uint32_t skillCoolDownTick;
    };
    struct CharacterState {
        uint64_t zoneInternalID; // zone 내부에서 사용하는 id
        int hp; // 0
        int mp; // 1
        int maxHp; // 2
        int maxMp; // 3
        uint32_t exp; // 4
        uint16_t level; // 5
        uint8_t dir = 0;  // 6
        float x, y; // 7, 8

        // --  내부 정보
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

        char charName[MAX_CHARNAME_LEN];
    };
}