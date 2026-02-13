#include "pch.h"
#include "ZoneState.h"
#include "IPacket.h"
#include "ILogger.h"
#include "PacketWriter.h"
#include "StateManager.h"

namespace Core {
    void ZoneState::Move(uint64_t sessionID, uint8_t dir, float speed) {
        if (speed <= 0)
            return;

        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessionToIndex.find(sessionID);
        if (it == m_sessionToIndex.end()) {
            return;
        }

        if (speed > 1) {
            m_cheatList.push_back({ sessionID, 100 });
            return;
        }

        float x = 0;
        float y = 0;
        switch (dir) {
        case 0: y = speed; break;
        case 1: y = -speed; break;
        case 2: x = -speed; break;
        case 3: x = speed; break;
        }

        auto& character = m_chars[it->second];
        // move 영역 제한
        if (character.x + x > m_area.x_max) x = 0;
        if (character.x + x < m_area.x_min) x = 0;
        if (character.y + y > m_area.y_max) y = 0;
        if (character.y + y < m_area.y_min) y = 0;

        character.x += x;
        character.y += y;
        character.dir = dir;
        character.dirtyBit |= 0x40; // dir
        if (x)
            character.dirtyBit |= 0x80; // x
        if (y)
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
            m_cheatList.push_back({ sessionID, 100 });
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
