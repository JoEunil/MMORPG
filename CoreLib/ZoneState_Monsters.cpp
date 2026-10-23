#include "pch.h"
#include "ZoneState.h"
#include "WorldMap.h"
#include "../UnityClient/Exports/ServerData/MonsterSpawns.inc"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace Core {
    namespace {
        constexpr float MONSTER_WANDER_RADIUS = 6.0f;
        constexpr float MONSTER_WANDER_SPEED = 0.375f * WORLD_MOVEMENT_SCALE;
        constexpr float MONSTER_CHASE_SPEED_SCALE = 0.75f;
        constexpr float MONSTER_ARRIVAL_DISTANCE = 0.05f;

        std::pair<int, int> ChooseSpawn(const CellArea& area) {
            const int minX = static_cast<int>(area.x_min) + Data::MONSTER_SPAWN_EDGE_INSET;
            const int minY = static_cast<int>(area.y_min) + Data::MONSTER_SPAWN_EDGE_INSET;
            const int width = static_cast<int>(area.x_max - area.x_min - Data::MONSTER_SPAWN_EDGE_INSET);
            const int height = static_cast<int>(area.y_max - area.y_min - Data::MONSTER_SPAWN_EDGE_INSET);
            const int x = minX + rand() % width;
            const int y = minY + rand() % height;
            if (WorldMap::CanTraverse(x, y, x, y))
                return {x, y};
            // Retain the authored count and owning cell; no extra random draws on the existing layout.
            for (int row = minY; row < minY + height; ++row)
                for (int column = minX; column < minX + width; ++column)
                    if (WorldMap::CanTraverse(column, row, column, row))
                        return {column, row};
            throw std::runtime_error("World map has no legal monster spawn in an authored cell.");
        }

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

        MonsterBehavior HomeBehavior(const MonsterState& monster) {
            if (std::abs(monster.spawnX - monster.x) > MONSTER_WANDER_RADIUS ||
                std::abs(monster.spawnY - monster.y) > MONSTER_WANDER_RADIUS)
                return MonsterBehavior::Returning;
            return MonsterBehavior::Wandering;
        }

        void PauseWandering(MonsterState& monster) {
            monster.wanderTargetX = monster.x;
            monster.wanderTargetY = monster.y;
            monster.wanderPauseTicks = static_cast<uint8_t>(10u + NextWanderValue(monster) % 21u);
        }

        void EndMonsterCombat(MonsterState& monster) {
            monster.aggro = 0;
            monster.aggroTick = 0;
            monster.behavior = HomeBehavior(monster);
            PauseWandering(monster);
        }

        bool InMonsterCastRange(const MonsterState& monster, const CharacterState& target,
            const Data::SkillData& skill) {
            if ((monster.data->id == 0 || monster.data->id == 1) && skill.skillID == 0) {
                const auto hitPhase = std::find_if(skill.phases.begin(), skill.phases.end(), [](const Data::SkillPhase& phase) {
                    return phase.actionID == static_cast<uint8_t>(Data::SkillActionType::Hit);
                });
                if (hitPhase != skill.phases.end())
                    return hitPhase->range.InRange(monster.dir, monster.x, monster.y, target.x, target.y);
            }
            const float dx = target.x - monster.x;
            const float dy = target.y - monster.y;
            return dx * dx + dy * dy <= 2.0f;
        }

        MonsterBehavior CombatBehavior(const MonsterState& monster, const CharacterState& target) {
            const auto& phase = monster.data->phases[monster.phase];
            const auto& step = phase.steps[monster.skillStep & (phase.stepCnt - 1)];
            const auto& skill = Data::skillList[step.skillID];
            if (monster.tick >= step.delayTicks &&
                (monster.data->id == 2 || InMonsterCastRange(monster, target, skill)))
                return MonsterBehavior::Attacking;
            return MonsterBehavior::Chasing;
        }
    }

    void ZoneState::InitializeMonster() {
        static_assert(CELLS_X == 5 && CELLS_Y == 5, "Revalidate authored monster spawn cell counts.");
        static_assert(ZONE_COUNT == 6, "Revalidate authored Zone density count.");
        static_assert(sizeof(Data::MONSTER_ZONE_DENSITY_PERCENT) / sizeof(Data::MONSTER_ZONE_DENSITY_PERCENT[0]) == ZONE_COUNT,
            "Authored density must have one value per field Zone.");
        static_assert([] {
            for (int zone = 0; zone < ZONE_COUNT; ++zone) {
                const int percent = Data::MONSTER_ZONE_DENSITY_PERCENT[zone];
                if (zone + 1 == TOWN_ZONE_ID) { if (percent != 0) return false; continue; }
                if (percent < 50 || percent > 200) return false;
                int64_t total = 1; // The central boss is never scaled.
                for (int cell = 0; cell < 25; ++cell) {
                    if (Data::REGULAR_MONSTERS_BY_CELL[cell] < 0 || Data::STRONG_MONSTERS_BY_CELL[cell] < 0)
                        return false;
                    total += int64_t{Data::REGULAR_MONSTERS_BY_CELL[cell]} * percent / 100;
                    total += int64_t{Data::STRONG_MONSTERS_BY_CELL[cell]} * percent / 100;
                }
                if (total > MAX_MONSTER_COUNT) return false;
            }
            return Data::REGULAR_MONSTERS_BY_CELL[12] == 0 && Data::STRONG_MONSTERS_BY_CELL[12] == 0;
        }(), "Authored Zone spawn density exceeds the supported range or capacity.");
        if (m_zoneID < 1 || m_zoneID > ZONE_COUNT)
            throw std::runtime_error("Monster spawning requires a valid field Zone.");
        static_assert(Data::MONSTER_TOWN_ZONE_ID == TOWN_ZONE_ID, "Town spawn authoring must match server ownership.");
        if (m_zoneID == TOWN_ZONE_ID) return;
        const int densityPercent = Data::MONSTER_ZONE_DENSITY_PERCENT[m_zoneID - 1];
        int uniqueId = 0;

        // Generated counts preserve per-cell type order; open-map rand() distribution is unchanged.
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
                const int regularCount = static_cast<int>(int64_t{Data::REGULAR_MONSTERS_BY_CELL[cellIndex]} * densityPercent / 100);
                for (int k = 0; k < regularCount; k++)
                {
                    const auto [spawnX, spawnY] = ChooseSpawn(area);

                    uint16_t monsterType = 0;

                    m_monsters.emplace_back();
                    m_monsters.back().Initialize(monsterType, uniqueId, spawnX, spawnY, j, i);

                    cell.monsterIndexes.push_back(uniqueId);
                    uniqueId++;
                }

                // 조금 센 일반 몬스터
                const int strongCount = static_cast<int>(int64_t{Data::STRONG_MONSTERS_BY_CELL[cellIndex]} * densityPercent / 100);
                for (int k = 0; k < strongCount; k++)
                {
                    const auto [spawnX, spawnY] = ChooseSpawn(area);

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

            if (!WorldMap::CanTraverse(spawnX, spawnY, spawnX, spawnY))
                throw std::runtime_error("World map blocks the authored boss spawn.");

            uint16_t monsterType = 2;

            m_monsters.emplace_back();
            m_monsters.back().Initialize(monsterType, uniqueId, spawnX, spawnY, 2, 2);

            cell.monsterIndexes.push_back(uniqueId);
            uniqueId++;
        }
    }
    void ZoneState::UpdateMonster() {
        std::lock_guard lock(m_mutex);
        for (auto& monster : m_monsters) {
            monster.tick++;
            CharacterState* target = nullptr;
            if (monster.hp == 0) {
                monster.behavior = MonsterBehavior::Dead;
            }
            else {
                const float aggroDx = monster.x - monster.aggroX;
                const float aggroDy = monster.y - monster.aggroY;
                if (monster.aggro != 0 &&
                    (++monster.aggroTick >= monster.data->aggroTimeOut ||
                     aggroDx * aggroDx + aggroDy * aggroDy >= monster.data->chaseDistance * monster.data->chaseDistance)) {
                    EndMonsterCombat(monster);
                }

                // HP phases are attack patterns, separate from the behavior state.
                const auto& phase = monster.data->phases[monster.phase];
                if (monster.phase < monster.data->phasesCnt - 1 && monster.hp < phase.hpThresholdAbs) {
                    monster.phase++;
                    monster.tick = 0;
                    monster.skillStep = 0;
                    monster.behavior = monster.aggro != 0 ? MonsterBehavior::Chasing : HomeBehavior(monster);
                    continue;
                }

                if (monster.aggro != 0) {
                    const auto it = m_InternalIDToIndex.find(monster.aggro);
                    if (it == m_InternalIDToIndex.end() || m_chars[it->second].hp <= 0) {
                        EndMonsterCombat(monster);
                        continue;
                    }
                    target = &m_chars[it->second];
                }
                if (target)
                    monster.behavior = CombatBehavior(monster, *target);
                else if (monster.behavior == MonsterBehavior::Chasing || monster.behavior == MonsterBehavior::Attacking)
                    EndMonsterCombat(monster);

            }
            switch (monster.behavior) {
            case MonsterBehavior::Wandering:
                MoveAround(monster);
                break;
            case MonsterBehavior::Returning:
                MoveMonsterHome(monster);
                break;
            case MonsterBehavior::Chasing:
                MoveToward(monster, *target);
                break;
            case MonsterBehavior::Attacking:
                UseMonsterSkill(monster);
                break;
            case MonsterBehavior::Dead:
                if (--monster.respawnTick == 0) {
                    SetMonsterPosition(monster, monster.spawnX, monster.spawnY);
                    monster.Respawn();
                    m_monsterCellsChanged = true;
                }
                break;
            }
        }
    }

    void ZoneState::UseMonsterSkill(MonsterState& monster) {
        const auto& phase = monster.data->phases[monster.phase];
        const auto& step = phase.steps[monster.skillStep & (phase.stepCnt - 1)];
        m_cells[monster.cellY][monster.cellX].activeSkills.emplace_back(
            1, 0, 0, monster.internalID, 0, monster.dir, monster.x, monster.y, step.skillID, 0, 0);
        monster.skillStep++;
        monster.tick = 0;
    }

    void ZoneState::SetMonsterPosition(MonsterState& monster, float x, float y) {
        const auto [cellX, cellY] = GetCell(x, y, m_area);
        if (cellX != monster.cellX || cellY != monster.cellY) {
            auto& oldMembers = m_cells[monster.cellY][monster.cellX].monsterIndexes;
            auto& newMembers = m_cells[cellY][cellX].monsterIndexes;
            newMembers.push_back(monster.internalID);
            const auto member = std::find(oldMembers.begin(), oldMembers.end(), monster.internalID);
            if (member != oldMembers.end()) {
                *member = oldMembers.back();
                oldMembers.pop_back();
            }
            monster.cellX = static_cast<uint8_t>(cellX);
            monster.cellY = static_cast<uint8_t>(cellY);
            m_monsterCellsChanged = true;
        }
        if (monster.x != x) monster.dirtyBit |= 0x02;
        if (monster.y != y) monster.dirtyBit |= 0x04;
        monster.x = x;
        monster.y = y;
    }

    bool ZoneState::MoveMonster(MonsterState& monster, float x, float y) {
        if (x < m_area.x_min || x > m_area.x_max || y < m_area.y_min || y > m_area.y_max ||
            !WorldMap::CanTraverse(monster.x, monster.y, x, y))
            return false;
        const float dx = x - monster.x;
        const float dy = y - monster.y;
        if (dx == 0.0f && dy == 0.0f) return true;
        SetDirection(monster, std::abs(dx) > std::abs(dy) ? (dx > 0 ? 3 : 2) : (dy > 0 ? 0 : 1));
        SetMonsterPosition(monster, x, y);
        return true;
    }

    void ZoneState::MoveToward(MonsterState& monster, CharacterState& character) {
        const float dx = character.x - monster.x;
        const float dy = character.y - monster.y;
        const float speed = monster.data->moveSpeed * MONSTER_CHASE_SPEED_SCALE * WORLD_MOVEMENT_SCALE;
        float x = monster.x, y = monster.y;
        if (std::abs(dx) > std::abs(dy))
            x += std::copysign((std::min)(std::abs(dx), speed), dx);
        else if (dy != 0.0f)
            y += std::copysign((std::min)(std::abs(dy), speed), dy);

        const float aggroDx = x - monster.aggroX;
        const float aggroDy = y - monster.aggroY;
        if (aggroDx * aggroDx + aggroDy * aggroDy > monster.data->chaseDistance * monster.data->chaseDistance ||
            x < m_area.x_min || x > m_area.x_max || y < m_area.y_min || y > m_area.y_max) {
            EndMonsterCombat(monster);
            return;
        }
        MoveMonster(monster, x, y);
    }

    void ZoneState::MoveMonsterHome(MonsterState& monster) {
        const float dx = monster.spawnX - monster.x;
        const float dy = monster.spawnY - monster.y;
        const bool moveX = std::abs(dx) >= std::abs(dy);
        const float stepX = std::copysign((std::min)(std::abs(dx), MONSTER_WANDER_SPEED), dx);
        const float stepY = std::copysign((std::min)(std::abs(dy), MONSTER_WANDER_SPEED), dy);
        // Try the other axis if the preferred step is blocked. This is not pathfinding.
        if (moveX) {
            if (!MoveMonster(monster, monster.x + stepX, monster.y) && stepY != 0.0f)
                MoveMonster(monster, monster.x, monster.y + stepY);
        }
        else {
            if (!MoveMonster(monster, monster.x, monster.y + stepY) && stepX != 0.0f)
                MoveMonster(monster, monster.x + stepX, monster.y);
        }
        monster.behavior = HomeBehavior(monster);
        if (monster.behavior == MonsterBehavior::Wandering)
            PauseWandering(monster);
    }

    void ZoneState::MoveAround(MonsterState& monster) {
        if (monster.wanderPauseTicks != 0) {
            --monster.wanderPauseTicks;
            return;
        }
        float dx = monster.wanderTargetX - monster.x;
        float dy = monster.wanderTargetY - monster.y;
        if (dx * dx + dy * dy <= MONSTER_ARRIVAL_DISTANCE * MONSTER_ARRIVAL_DISTANCE) {
            bool found = false;
            for (int attempt = 0; attempt < 8; ++attempt) {
                const float offsetX = (static_cast<float>(NextWanderValue(monster) % 2001u) / 1000.0f - 1.0f) * MONSTER_WANDER_RADIUS;
                const float offsetY = (static_cast<float>(NextWanderValue(monster) % 2001u) / 1000.0f - 1.0f) * MONSTER_WANDER_RADIUS;
                const float x = monster.spawnX + offsetX, y = monster.spawnY + offsetY;
                if (offsetX * offsetX + offsetY * offsetY > MONSTER_WANDER_RADIUS * MONSTER_WANDER_RADIUS ||
                    x < m_area.x_min || x > m_area.x_max || y < m_area.y_min || y > m_area.y_max ||
                    !WorldMap::CanTraverse(monster.x, monster.y, x, y))
                    continue;
                dx = x - monster.x;
                dy = y - monster.y;
                if (dx * dx + dy * dy <= MONSTER_ARRIVAL_DISTANCE * MONSTER_ARRIVAL_DISTANCE)
                    continue;
                monster.wanderTargetX = x;
                monster.wanderTargetY = y;
                found = true;
                break;
            }
            if (!found) {
                PauseWandering(monster);
                return;
            }
        }
        const float distance = std::sqrt(dx * dx + dy * dy);
        const float scale = (std::min)(MONSTER_WANDER_SPEED, distance) / distance;
        if (!MoveMonster(monster, monster.x + dx * scale, monster.y + dy * scale)) {
            PauseWandering(monster);
            return;
        }
        dx = monster.wanderTargetX - monster.x;
        dy = monster.wanderTargetY - monster.y;
        if (dx * dx + dy * dy <= MONSTER_ARRIVAL_DISTANCE * MONSTER_ARRIVAL_DISTANCE)
            PauseWandering(monster);
    }
}
