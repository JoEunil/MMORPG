#include "pch.h"
#include <cmath>
#include "ZoneState.h"
#include "IPacket.h"
#include "ILogger.h"
#include "PacketWriter.h"
#include "StateManager.h"

namespace Core {
    // 틱마다 이동 예산을 보충한다. Move()가 이 예산에서 이동 거리를 차감하므로,
    // 한 틱에 입력이 몇 개 들어오든 총 이동량이 경과 시간을 넘지 못한다.
    // CAP으로 이월을 제한해 오래 정지했다가 한 번에 순간이동하는 것을 막는다.
    void ZoneState::ReplenishMoveBudget() {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& character : m_chars) {
            character.moveBudget += MOVE_BUDGET_PER_TICK;
            if (character.moveBudget > MOVE_BUDGET_CAP)
                character.moveBudget = MOVE_BUDGET_CAP;
        }
    }

    void ZoneState::Move(uint64_t sessionID, uint8_t dir, float x, float y) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessionToIndex.find(sessionID);
        if (it == m_sessionToIndex.end()) {
            return;
        }
        auto& character = m_chars[it->second];

        // 방향은 좌표와 무관하게 반영 (제자리에서 바라보는 방향만 바꾸는 경우)
        if (character.dir != dir) {
            character.dir = dir;
            character.dirtyBit |= 0x40; // dir
        }

        // zone 경계로 클램프. 경계를 넘는 입력은 치트가 아니라 정상 흐름에서도 발생하므로 거부하지 않고 잘라낸다.
        // 클램프는 거리를 줄이기만 하므로 아래 예산 검사를 우회할 수 없다.
        if (x < m_area.x_min) x = m_area.x_min;
        else if (x > m_area.x_max) x = m_area.x_max;
        if (y < m_area.y_min) y = m_area.y_min;
        else if (y > m_area.y_max) y = m_area.y_max;

        const float dx = x - character.x;
        const float dy = y - character.y;
        const float distSq = dx * dx + dy * dy;
        if (distSq == 0.0f)
            return; // 이동 없음

        float dist = std::sqrt(distSq);
        const float budget = character.moveBudget + MOVE_BUDGET_EPSILON;
        if (dist > budget) {
            // 초과분은 거부하지 않고 예산만큼만 이동시킨다.
            // 거부한다면 간격이 MOVE_BUDGET_CAP을 넘는 순간 이후 모든 이동이 영구히 거부된다.
            const float scale = budget / dist;
            x = character.x + dx * scale;
            y = character.y + dy * scale;
            dist = budget;

            // 다만 어떤 타이밍으로도 나올 수 없는 거리는 조작으로 본다.
            // 틱 지연으로 생기는 일시적 초과는 여기 걸리지 않는다.
            if (distSq > MOVE_BUDGET_CAP * MOVE_BUDGET_CAP) {
                gameLogger->LogInfo("zone state", "move exceeds budget", "sessionID", sessionID,
                    "dist", std::sqrt(distSq), "budget", character.moveBudget);
                m_cheatList.push_back({ sessionID, 1 });
            }
        }

        character.moveBudget -= dist;
        if (character.moveBudget < 0.0f)
            character.moveBudget = 0.0f;

        const bool movedX = (x != character.x);
        const bool movedY = (y != character.y);
        character.x = x;
        character.y = y;
        if (movedX)
            character.dirtyBit |= 0x80; // x
        if (movedY)
            character.dirtyBit |= 0x100; // y

        auto [newX, newY] = GetCell(character.x, character.y, m_area);

        uint16_t oldX = character.cellX;
        uint16_t oldY = character.cellY;
        uint16_t oldIdx = character.cellIdx;
        // cell 변경 발생할 경우만
        if (newX != oldX || newY != oldY)
        {
            RemoveFromCell(character);
            AddToCell(character, newX, newY);
        }
    }

    static const uint8_t NONE_SKILL = 255;
    void ZoneState::Skill(uint64_t sessionID, uint8_t skillSlot) {
        if (skillSlot == NONE_SKILL) {
            return;
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessionToIndex.find(sessionID);
        if (it == m_sessionToIndex.end()) {
            return;
        }
        auto& character = m_chars[it->second];
        if (skillSlot > (character.skillSlotCnt - 1)) {
			gameLogger->LogInfo("zone state", "invalid skill slot", "sessionID", sessionID, "skillSlot", skillSlot);
            m_cheatList.push_back({ sessionID, 1 });
            return;
        }
        auto& skill = character.skillSlot[skillSlot];
        const Data::SkillData& skillInfo = Data::skillList[skill.skillID];

        if (skill.skillCoolDownTick != 0 || character.mp < skillInfo.mana) // 스킬 시전 불가능한 상태
            return;
        character.mp -= skillInfo.mana;
        if (skillInfo.mana != 0) {
            character.dirtyBit |= 0x02;
        }
        skill.skillCoolDownTick = skillInfo.coolDown;
        m_cells[character.cellY][character.cellX].activeSkills.emplace_back(ActiveSkill{
            0, sessionID, character.zoneInternalID, 0, skillSlot, character.dir, character.x, character.y, skill.skillID, 0, 0
        });
    }

    void ZoneState::DirtyCheck(uint64_t sessionID) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessionToIndex.find(sessionID);
        if (it == m_sessionToIndex.end()) {
            return;
        }
        auto& character = m_chars[it->second];
        m_cells[character.cellY][character.cellX].dirtyChar.push_back(character.zoneInternalID);
    }
}
