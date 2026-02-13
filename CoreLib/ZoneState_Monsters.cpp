#include "pch.h"
#include "ZoneState.h"

namespace Core {
    void ZoneState::InitializeMonster() {
        int uniqueId = 0;

        // 1. 일반 몬스터(0) : 셀마다 8마리
        for (int i = 0; i < CELLS_Y; i++)
        {
            for (int j = 0; j < CELLS_X; j++)
            {
                auto& cell = m_cells[i][j];
                auto& area = cell.area;

                if (i == 2 && j == 2)
                    continue;

                // 일반몬스터 8마리
                for (int k = 0; k < 8; k++)
                {
                    int spawnX = area.x_min + 1 + (rand() % (int)(area.x_max - area.x_min - 1));
                    int spawnY = area.y_min + 1 + (rand() % (int)(area.y_max - area.y_min - 1));

                    uint16_t monsterType = 0;

                    m_monsters.emplace_back();
                    m_monsters.back().Initialize(monsterType, uniqueId, spawnX, spawnY, j, i);

                    cell.monsterIndexes.push_back(uniqueId);
                    uniqueId++;
                }

                // 조금 센 일반 몬스터 2마리
                for (int k = 0; k < 2; k++)
                {
                    int spawnX = area.x_min + 1 + (rand() % (int)(area.x_max - area.x_min - 1));
                    int spawnY = area.y_min + 1 + (rand() % (int)(area.y_max - area.y_min - 1));

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


            // 2. Phase 처리 (보스, 일반 동일하게)
            auto& phase = monster.data->phases[monster.phase];
            if (monster.hp < phase.hpThresholdAbs && monster.phase < monster.data->phasesCnt - 1) {
                monster.phase++;
                monster.tick = 0; // phase 전환 시 tick 초기화
                monster.skillStep = 0;
                continue;
            }

            // aggro 처리
            if (monster.aggro != 0 && monster.aggroTick >= monster.data->aggroTimeOut) {
                monster.aggro = 0;
                monster.aggroTick = 0;
            }

            if (monster.aggro == 0) {
                MoveAround(monster);
                continue;
            }

            auto it = m_InternalIDToIndex.find(monster.aggro);
            if (it == m_InternalIDToIndex.end()) {
                monster.aggro = 0;
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
                m_cells[monster.cellY][monster.cellX].activeSkills.emplace_back(ActiveSkill{
                    1, 0, 0, monster.internalID, 0, monster.dir, monster.x, monster.y, skill.skillID, 0, 0
                    });
                monster.skillStep++;
                monster.tick = 0;
            } else if (monster.aggroTick >= step.delayTicks and monster.data->id == 2) {
                m_cells[monster.cellY][monster.cellX].activeSkills.emplace_back(ActiveSkill{
                    1, 0, 0, monster.internalID, 0, monster.dir, monster.x, monster.y, skill.skillID, 0, 0
                    });
                monster.skillStep++;
                monster.tick = 0;
            } else {
                MoveToward(monster, target);
            }
        }

    }

    void ZoneState::MoveToward(MonsterState& monster, CharacterState& character) {
        int dx = character.x - monster.x;
        int dy = character.y - monster.y;

        // x, y 중 큰 거리 쪽으로만 이동 (상하좌우)
        float x = 0, y = 0;
        if (std::abs(dx) > std::abs(dy)) {
            x += (dx > 0) ? 1 : -1;
        }
        else if (dy != 0) {
            y += (dy > 0) ? 1 : -1;
        }
        auto& area = m_cells[monster.cellY][monster.cellX].area;
        x *= monster.data->moveSpeed;
        y *= monster.data->moveSpeed;
        if (x != 0) {
            if ((monster.x + x) < area.x_min or (monster.x + x) > area.x_max) {
                monster.aggro = 0;
                monster.aggroTick = 0;
                return;
            }
            if (x > 0) 
                monster.dir = 3;
            else
                monster.dir = 2;
            monster.x += x;
            monster.dirtyBit |= 0x01;
            monster.dirtyBit |= 0x04;
        }
        if (y != 0) {
            if ((monster.y + y) < area.y_min or (monster.y + y) > area.y_max) {
                monster.aggro = 0;
                monster.aggroTick = 0;
                return;
            }
            if (y > 0)
                monster.dir = 0;
            else
                monster.dir = 1;
            monster.y += y;
            monster.dirtyBit |= 0x02;
            monster.dirtyBit |= 0x04;
        }
    }

    void ZoneState::MoveAround(MonsterState& monster) {
        float speed = 0.6;
        float dx = 0, dy = 0;

        auto& area = m_cells[monster.cellY][monster.cellX].area;

        switch (monster.dir) {
        case 0: dy = speed; break; // 상
        case 1: dy = -speed; break; // 하
        case 2: dx = -speed; break; // 좌
        case 3: dx = speed; break;  // 우
        }

        // 영역 체크
        if (monster.x + dx < area.x_min || monster.x + dx > area.x_max) {
            monster.dir = (monster.dir == 2 ? 3 : 2); // 좌우 반전
            monster.dirtyBit |= 0x04; // 방향 변경 표시
        }
        else
            monster.x += dx;

        if (monster.y + dy < area.y_min || monster.y + dy > area.y_max) {
            monster.dir = (monster.dir == 0 ? 1 : 0); // 상하 반전
            monster.dirtyBit |= 0x04; // 방향 변경 표시
        }
        else
            monster.y += dy;

        // 이동 표시
        if (dx != 0) monster.dirtyBit |= 0x01;
        if (dy != 0) monster.dirtyBit |= 0x02;
        monster.dirtyBit |= 0x04; // 방향 변경 표시
    }
}
