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


        float spawnX, spawnY;
        uint64_t aggro;
        uint32_t aggroTick;
        uint8_t cellX;
        uint8_t cellY;
        uint32_t respawnTick;
        uint64_t tick;
        uint16_t skillStep;
        uint8_t phase;
        
        void Initialize(uint16_t monsterID , uint16_t id, float posX, float posY, uint8_t CellX, uint8_t CellY) {
            data = &Data::monsters[monsterID];
            hp = data->maxHp;
            respawnTick = data->respawn;
            internalID = id;
            x = posX;
            y = posY;
            spawnX = x;
            spawnY = y;
            dir = 2;
            aggro = 0;
            aggroTick = 0;
            tick = 0;
            skillStep = 0;
            phase = 0;
            cellX = CellX;
            cellY = CellY;
        }

        void Respawn() {
            hp = data->maxHp;

            x = spawnX;
            y = spawnY;
            dir = 1;
            aggro = 0;
            aggroTick = 0;
            respawnTick = data->respawn;
        }
    };
}