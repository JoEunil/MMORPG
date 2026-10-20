#include "pch.h"
#include "ZoneState.h"
#include "../UnityClient/Exports/ServerData/MonsterSpawns.inc"
#include <algorithm>
#include <cmath>

namespace Core {
    namespace {
        constexpr float MONSTER_WANDER_RADIUS = 6.0f;
        constexpr float MONSTER_WANDER_SPEED = 0.375f * WORLD_MOVEMENT_SCALE;
        constexpr float MONSTER_CHASE_SPEED_SCALE = 0.75f;

        uint32_t NextWanderValue(MonsterState& monster) {
            uint32_t value = monster.wanderSeed;
            value ^= value << 13;
            value ^= value >> 17;
            value ^= value << 5;
            monster.wanderSeed = value == 0 ? 0xA341316Cu : value;
            return monster.wanderSeed;
        }

        void SetDirection(MonsterState& monster, uint8_t direction) {
            if (monster.dir == direction) return;
            monster.dir = direction;
            monster.dirtyBit |= 0x08;
        }
    }

    void ZoneState::InitializeMonster() {
        static_assert(CELLS_X == 5 && CELLS_Y == 5 && CELL_SIZE == 22, "Revalidate authored monster spawn geometry.");
        int uniqueId = 0;

        // Generated counts preserve the existing per-cell type order and rand() distribution.
        for (int i = 0; i < CELLS_Y; i++)
        {
            for (int j = 0; j < CELLS_X; j++)
            {
                auto& cell = m_cells[i][j];
                auto& area = cell.area;
                const int cellIndex = i * CELLS_X + j;

                if (i == 2 && j == 2)
                    continue;

                // 일반 몬스터
                for (int k = 0; k < Data::REGULAR_MONSTERS_BY_CELL[cellIndex]; k++)
                {
                    int spawnX = area.x_min + Data::MONSTER_SPAWN_EDGE_INSET + (rand() % (int)(area.x_max - area.x_min - Data::MONSTER_SPAWN_EDGE_INSET));
                    int spawnY = area.y_min + Data::MONSTER_SPAWN_EDGE_INSET + (rand() % (int)(area.y_max - area.y_min - Data::MONSTER_SPAWN_EDGE_INSET));

                    uint16_t monsterType = 0;

                    m_monsters.emplace_back();
                    m_monsters.back().Initialize(monsterType, uniqueId, spawnX, spawnY, j, i);

                    cell.monsterIndexes.push_back(uniqueId);
                    uniqueId++;
                }

                // 조금 센 일반 몬스터
                for (int k = 0; k < Data::STRONG_MONSTERS_BY_CELL[cellIndex]; k++)
                {
                    int spawnX = area.x_min + Data::MONSTER_SPAWN_EDGE_INSET + (rand() % (int)(area.x_max - area.x_min - Data::MONSTER_SPAWN_EDGE_INSET));
                    int spawnY = area.y_min + Data::MONSTER_SPAWN_EDGE_INSET + (rand() % (int)(area.y_max - area.y_min - Data::MONSTER_SPAWN_EDGE_INSET));

                    uint16_t monsterType = 1;

                    m_monsters.emplace_back();
                    m_monsters.back().Initialize(monsterType, uniqueId, spawnX, spawnY, j, i);

                    cell.monsterIndexes.push_back(uniqueId);
                    uniqueId++;
                }
            }
        }

        // 2. 보스 몬스터(type = 2)
        {
            int i = 2;
            int j = 2;
            auto& cell = m_cells[i][j];
            auto& area = cell.area;

            int spawnX = (area.x_min + area.x_max) / 2;
            int spawnY = (area.y_min + area.y_max) / 2;

            uint16_t monsterType = 2;

            m_monsters.emplace_back();
            m_monsters.back().Initialize(monsterType, uniqueId, spawnX, spawnY, 2, 2);

            cell.monsterIndexes.push_back(uniqueId);
            uniqueId++;
        }
    }
    void ZoneState::UpdateMonster() {
        for (auto& monster : m_monsters)
        {
            monster.tick++;

            // 1. 죽은 몬스터 respawn
            if (monster.hp == 0) {
                if (--monster.respawnTick == 0)
                    monster.Respawn();
                continue;
            }

            // Zone updates run every 50 ms, so the existing 100-tick aggro timeout is 5 seconds.
            if (monster.aggro != 0 && ++monster.aggroTick >= monster.data->aggroTimeOut) {
                monster.aggro = 0;
                monster.aggroTick = 0;
            }

            // 2. Phase 처리 (보스, 일반 동일하게)
            auto& phase = monster.data->phases[monster.phase];
            if (monster.hp < phase.hpThresholdAbs && monster.phase < monster.data->phasesCnt - 1) {
                monster.phase++;
                monster.tick = 0; // phase 전환 시 tick 초기화
                monster.skillStep = 0;
                continue;
            }

            if (monster.aggro == 0) {
                MoveAround(monster);
                continue;
            }

            auto it = m_InternalIDToIndex.find(monster.aggro);
            if (it == m_InternalIDToIndex.end()) {
                monster.aggro = 0;
                monster.aggroTick = 0;
                continue;
            }

            auto& target = m_chars[it->second];

            auto& step = phase.steps[monster.skillStep & (phase.stepCnt - 1)];
            auto& skill = Data::skillList[step.skillID];

            // 통합해서 쓸려면..   
            int dx = target.x - monster.x;
            int dy = target.y - monster.y;
            int dist2 = dx * dx + dy * dy;

            if (dist2 <= 2 and monster.tick >= step.delayTicks)
            {
                m_cells[monster.cellY][monster.cellX].activeSkills.emplace_back(
                    1, 0, 0, monster.internalID, 0, monster.dir, monster.x, monster.y, skill.skillID, 0, 0
                    );
                monster.skillStep++;
                monster.tick = 0;
            } else if (monster.tick >= step.delayTicks and monster.data->id == 2) {
                m_cells[monster.cellY][monster.cellX].activeSkills.emplace_back(
                    1, 0, 0, monster.internalID, 0, monster.dir, monster.x, monster.y, skill.skillID, 0, 0
                    );
                monster.skillStep++;
                monster.tick = 0;
            } else {
                MoveToward(monster, target);
            }
        }

    }

    void ZoneState::MoveToward(MonsterState& monster, CharacterState& character) {
        float dx = character.x - monster.x;
        float dy = character.y - monster.y;

        // x, y 중 큰 거리 쪽으로만 이동 (상하좌우)
        float x = 0, y = 0;
        if (std::abs(dx) > std::abs(dy)) {
            x += (dx > 0) ? 1 : -1;
        }
        else if (dy != 0) {
            y += (dy > 0) ? 1 : -1;
        }
        auto& area = m_cells[monster.cellY][monster.cellX].area;
        x *= monster.data->moveSpeed * MONSTER_CHASE_SPEED_SCALE * WORLD_MOVEMENT_SCALE;
        y *= monster.data->moveSpeed * MONSTER_CHASE_SPEED_SCALE * WORLD_MOVEMENT_SCALE;
        if (x != 0) {
            if ((monster.x + x) < area.x_min or (monster.x + x) > area.x_max) {
                monster.aggro = 0;
                monster.aggroTick = 0;
                return;
            }
            SetDirection(monster, x > 0 ? 3 : 2);
            monster.x += x;
            monster.dirtyBit |= 0x02;
        }
        if (y != 0) {
            if ((monster.y + y) < area.y_min or (monster.y + y) > area.y_max) {
                monster.aggro = 0;
                monster.aggroTick = 0;
                return;
            }
            SetDirection(monster, y > 0 ? 0 : 1);
            monster.y += y;
            monster.dirtyBit |= 0x04;
        }
    }

    void ZoneState::MoveAround(MonsterState& monster) {
        auto& area = m_cells[monster.cellY][monster.cellX].area;
        const float minX = (std::max)(area.x_min, monster.spawnX - MONSTER_WANDER_RADIUS);
        const float maxX = (std::min)(area.x_max, monster.spawnX + MONSTER_WANDER_RADIUS);
        const float minY = (std::max)(area.y_min, monster.spawnY - MONSTER_WANDER_RADIUS);
        const float maxY = (std::min)(area.y_max, monster.spawnY + MONSTER_WANDER_RADIUS);
        const float homeDx = monster.spawnX - monster.x;
        const float homeDy = monster.spawnY - monster.y;

        if (std::abs(homeDx) > MONSTER_WANDER_RADIUS || std::abs(homeDy) > MONSTER_WANDER_RADIUS) {
            monster.wanderTicks = 0;
            monster.wanderPauseTicks = 0;
            const bool moveX = std::abs(homeDx) >= std::abs(homeDy);
            const float distance = moveX ? homeDx : homeDy;
            const float step = std::copysign((std::min)(std::abs(distance), MONSTER_WANDER_SPEED), distance);
            if (moveX) {
                SetDirection(monster, step > 0 ? 3 : 2);
                monster.x += step;
                monster.dirtyBit |= 0x02;
            } else {
                SetDirection(monster, step > 0 ? 0 : 1);
                monster.y += step;
                monster.dirtyBit |= 0x04;
            }
            return;
        }

        if (monster.wanderPauseTicks != 0) {
            --monster.wanderPauseTicks;
            return;
        }
        if (monster.wanderTicks == 0) {
            const uint32_t choice = NextWanderValue(monster);
            if ((choice & 3u) == 0) {
                monster.wanderPauseTicks = static_cast<uint8_t>(10u + ((choice >> 2) % 21u));
                return;
            }
            SetDirection(monster, static_cast<uint8_t>((choice >> 4) & 3u));
            monster.wanderTicks = static_cast<uint8_t>(20u + ((choice >> 8) % 41u));
        }

        float dx = 0, dy = 0;
        switch (monster.dir) {
        case 0: dy = MONSTER_WANDER_SPEED; break;
        case 1: dy = -MONSTER_WANDER_SPEED; break;
        case 2: dx = -MONSTER_WANDER_SPEED; break;
        case 3: dx = MONSTER_WANDER_SPEED; break;
        }
        if (monster.x + dx < minX || monster.x + dx > maxX ||
            monster.y + dy < minY || monster.y + dy > maxY) {
            monster.wanderTicks = 0;
            return;
        }
        if (dx != 0) {
            monster.x += dx;
            monster.dirtyBit |= 0x02;
        }
        if (dy != 0) {
            monster.y += dy;
            monster.dirtyBit |= 0x04;
        }
        --monster.wanderTicks;
    }
}
