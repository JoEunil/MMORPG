## Skill 구현

## 1. 개요
이 문서는 게임 로직 중 Skill 구현에 대해 설명합니다.

## 2. 구현 범위

스킬 사용 주체 및 대상은 다음과 같다.
- 몬스터 → 캐릭터
- 캐릭터 → 몬스터
즉, 몬스터와 캐릭터 간의 스킬 사용을 중심으로 한다.

스킬은 다음과 같은 프로세스로 구성된다:
__Phase/ Range__  
캐릭터는 __Action 패킷__ 입력을 기반으로 스킬을 사용하며,  
몬스터는 __Monster Data에 정의된 AI 동작__ 을 통해 스킬이 시전된다.  
스킬의 __준비 모션(캐스팅)__과 타격 모션을 분리하여 처리하기 위해 SkillPhase를 사용한다.
- Ready (준비 동작)
- Hit (타격 판정)

보스 몬스터의 경우 Ready → Hit → Ready → Hit와 같이 여러 Skill Phase가 순차적으로 구성된다.

__SkillSlot / CoolDown__  
스킬 입력은 SkillSlot으로 전달되며, 해당 슬롯의 스킬이 시전된다.
스킬 시전과 동시에 CoolDown이 시작되며, CoolDown은 틱 단위로 감소한다.

__Active Skill Queue__  
Action Packet으로 들어온 요청을 정의된 Phase 순서에 따라 처리하기 위해 ActiveSkill 큐를 사용한다.
각 Phase가 도달해야 하는 Tick에 도착하면 해당 동작(Ready, Hit)이 실행되고, 서버는 ActionResult 패킷을 전송한다.
클라이언트는 ActionResult 패킷을 통해 애니메이션, 이펙트를 재생한다.

__Hit 판정__  
```cpp
void ZoneState::ApplyHit(std::optional<std::reference_wrapper<CharacterState>> c, ActiveSkill& skill, int idx) {
	~~~
	if (skill.casterType == 0) { // 캐릭터 -> 몬스터
		auto& caster = c.value().get();
		for (auto& cellIdx : AOI[idx]) {
			auto& cell = m_cells[cellIdx / CELLS_X][cellIdx % CELLS_X];
			for (uint16_t mon : cell.monsterIndexes)
			{
				if (m_monsters[mon].hp == 0)
					continue;
				if (phase.range.InRange(skill.dir, skill.x, skill.y, m_monsters[mon].x, m_monsters[mon].y))
				{
					~~~
					// 스킬 처리
					~~~
				}
			}
		}
	}
}
```
스킬 Hit 판정은 스킬의 AOI(Area of Interest) 범위 내의 타겟을 순회하며 처리한다.
- 단일 대상 스킬: 범위 내 첫 번째 타겟에게만 피격 처리
- AOE(Area of Effect) 스킬: 범위 내 모든 타겟에게 피격 처리
- 게임 서버에서 __CPU 부하가 가장 큰 부분__ 이다.
Monster AI 로직에 대한 추가 내용은 [Monster.md](Monster.md)을 참고한다.


## 3. 시연
![gif 로드 실패](images/SkillAOI.gif)
![gif 로드 실패](images/BossMonster.gif)
- 스킬 시전 위치는 캐스터의 움직임을 따라가지 않고, 최초 시전 위치를 기준으로 모든 SkillPhase가 수행된다.
- 몬스터는 10 FPS로 갱신되며, 클라이언트에서는 보간만 적용되기 때문에 서버 위치와 화면상의 위치 간 오차가 발생한다.다.

## 4. 참고 

[SkillData.h](CoreLib/SkillData.h)  
[SkillData.cpp](CoreLib/SkillData.cpp)  
[SkillData_Range.cpp](CoreLib/SkillData_Range.cpp)  
[ZonsState_Skills.cpp](CoreLib/ZonsState_Skills.cpp)  
[ZonsState_Actions.cpp](CoreLib/ZonsState_Actions.cpp)  