#pragma once
#include <cstdint>
#include "MonsterData.h"
#include "SkillData.h"

namespace Core {
    struct MonsterState {
        uint16_t internalID;
        const Data::MonsterData* data;  // static data pointer
        int hp; // 0
        float x, y; // 1. 2
        uint8_t dir; // 3
        uint32_t attacked; // 4
        uint8_t dirtyBit = 0x00;


        uint64_t aggro;
        uint8_t cellX;
        uint8_t cellY;

        void Initialize(const Data::MonsterData* data, float x, float y) {
            this->data = data;
            this->hp = data->maxHp;

            this->x = x;
            this->y = y;
            dir = 0;
            aggro = 0;
        }
    };
}