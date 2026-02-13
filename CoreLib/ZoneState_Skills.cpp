#include "pch.h"
#include "ZoneState.h"
#include <optional>
#include <iostream>


namespace Core {
	void ZoneState::SkillCoolDown() {
		std::lock_guard<std::mutex> lock(m_mutex);
		for (auto& character : m_chars) 
		{
			for (int i = 0; i < character.skillSlotCnt; i++) {
				if (character.skillSlot[i].skillCoolDownTick != 0)
					character.skillSlot[i].skillCoolDownTick--;
			}
		}
		for (int i = 0; i < CELLS_Y; i++)
		{
			for (int j = 0; j < CELLS_X; j++)
			{
				auto& cell = m_cells[i][j];
				for (auto& skill : cell.activeSkills)
				{
					skill.currentTick++;
				}
			}
		}
	}

	void ZoneState::ApplySkill() {
		std::lock_guard<std::mutex> lock(m_mutex);

		for (int i = 0; i < CELLS_Y; i++)
		{
			for (int j = 0; j < CELLS_X; j++)
			{
				auto& cell = m_cells[i][j];
				ProcessCellActiveSkills(cell);
			}
		}
	}
	void ZoneState::PopSkill(int idx, std::vector<ActiveSkill>& list) {
		std::swap(list[idx], list.back());
		list.pop_back();
	}

	void ZoneState::ProcessCellActiveSkills(Cell& cell) {
		for (int k = 0; k < cell.activeSkills.size(); k++)
		{
			auto& skill = cell.activeSkills[k];
			auto& skillData = Data::skillList[skill.skillId];
			if (skill.currentTick < skillData.phases[skill.currentPhase].tick) {
				continue;
			}
			std::optional<std::reference_wrapper<CharacterState>> caster;
			if (skill.casterType == 0) {
				auto it = m_sessionToIndex.find(skill.sessionId);
				if (it == m_sessionToIndex.end()) {
					PopSkill(k, cell.activeSkills);
					continue;
				}
				caster = m_chars[it->second];
			}
			else {
				if (m_monsters[skill.monsterId].hp == 0) {
					PopSkill(k, cell.activeSkills);
					k--;
					continue;
				}
			}
			auto actionID = skillData.phases[skill.currentPhase].actionID;
			if (actionID == 1)
				ApplyHit(caster, skill, cell);
			cell.actionResults.emplace_back(ActionResult{
			skill.casterType,
			skill.zoneInternalId,
			skill.monsterId,
			skill.skillSlot,
			skill.dir,
			skill.x,
			skill.y,
			skill.skillId,
			skill.currentPhase,            
				});
			if (++skill.currentPhase >= skillData.phases.size()) {
				PopSkill(k, cell.activeSkills);
				k--;
				continue;

			}
			skill.currentTick = 0;
		}
	}
	void ZoneState::ApplyHit(std::optional<std::reference_wrapper<CharacterState>> c, ActiveSkill& skill, Cell& cell) {
		
		auto& skillData = Data::skillList[skill.skillId];
		auto& phase = skillData.phases[skill.currentPhase];
		bool AOE = skillData.flags & 0x02; // 광역기
		if (skill.casterType == 0) { // 캐릭터 -> 몬스터
			auto& caster = c.value().get();
			for (uint16_t mon : cell.monsterIndexes)
			{
				if (m_monsters[mon].hp == 0)
					continue;
				if (phase.range.InRange(skill.dir, skill.x, skill.y, m_monsters[mon].x, m_monsters[mon].y))
				{
					auto damage = caster.attack * phase.attack;
					if (skillData.flags & 0x01) {
						// critical 적용..
						damage *= 2;
					}
					m_monsters[mon].hp -= damage;
					if (m_monsters[mon].hp <= 0) {
						m_monsters[mon].hp = 0;
						// 일단 막타 친 사람만 보상.. 
						// 보스몬스터의 경우 hitter list로 보상 분배하도록 처리해야됨.
						caster.exp += m_monsters[mon].data->reward.exp;
						caster.dirtyBit |= 0x10; // exp
						cell.dirtyChar.push_back(caster.zoneInternalID);
					}
					m_monsters[mon].aggro = caster.zoneInternalID;
					m_monsters[mon].aggroTick = 0;
					m_monsters[mon].dirtyBit |= 0x01; // hp
					if (!AOE)
						break;
				}
			}
		}
		else { // 몬스터 -> 캐릭터
			auto& caster = m_monsters[skill.monsterId];
			for (auto session : cell.charSessions) {
				auto it = m_sessionToIndex.find(session);
				if (it == m_sessionToIndex.end())
					continue;
				auto& character = m_chars[it->second];

				if (phase.range.InRange(skill.dir, skill.x, skill.y, character.x, character.y))
				{
					auto damage = caster.data->attack * phase.attack;
					if (skillData.flags & 0x01) {
						// critical 적용..
						damage *= 2;
					}
					character.hp -= damage;
					if (character.hp <= 0)
						character.hp = 0;
					else 
						caster.aggroTick = 0; // 공격시에도 어그로 틱 초기화.
					character.dirtyBit |= 0x01; // hp
					cell.dirtyChar.push_back(character.zoneInternalID);
					if (!AOE)
						break;
				}
			}
		}
		
	}

}
