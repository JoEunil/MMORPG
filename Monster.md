# Monster 구현

## 1. 개요
이 문서는 게임 로직 중 Monster 구현에 대해 설명합니다.

## 2. 구현 범위  

### 몬스터 기능
- __피격 및 데미지 처리__  
	- 몬스터는 공격을 받으면 데미지를 계산하여 HP를 감소시키고, 필요 시 사망 처리한다.
- __이동__  
	- 몬스터는 자신의 Cell 내부에서 현재 바라보는 방향으로 이동하며, Cell 경계에 도달하면 이동을 멈춘다.
- __Aggro 및 추격 / 공격__
	- 몬스터가 피격될 경우 공격자를 Aggro 대상으로 설정하고 해당 대상을 추적한다.
	- Aggro 대상이 Cell 범위를 벗어나거나 일정 시간이 지나면 Aggro는 해제된다.
	- 공격 가능 범위에 도달하면 공격을 수행한다. 공격 주기는 틱기반으로 설정한다. 
- __Respawn__
	- 사망한 몬스터는 설정된 Respawn tick에 도달하면 해당 위치에서 다시 부활한다.

__서버 업데이트 주기__  
캐릭터는 20 FPS로 업데이트되지만,
몬스터는 부하를 줄이기 위해 **10 FPS**로 Delta 패킷을 전송하도록 구현하였다.
> 패킷 전송만 10FPS로 처리되고 내부 로직은 20FPS로 처리된다.
	
__몬스터 종류__  
현재 구현된 몬스터는 다음 세 종류이다.
* 일반 몬스터 (고양이 캐릭터)
* 강화형 일반 몬스터 (소 캐릭터)
* 보스 몬스터

__보스 몬스터 스킬 처리__  
보스 몬스터 스킬은 한 번의 시전으로 여러 Phase가 존재하며,
Phase마다 반복적으로 Hit 판정이 발생한다.

또한 보스 몬스터는 HP 구간(Phase) 에 따라 행동 패턴이 달라지며,
각 Phase별로 스킬 시전 간격, 반복 횟수, 공격 강도 등을 다르게 설정하여
난이도 및 전투 패턴이 변화하도록 구성하였다.

## 3. 한계
__상태 머신 기반으로 확장 필요__  
현재는 MonsterAI 내부의 단일 메서드에서 순차 로직으로 처리하고 있으나,
향후 확장성을 위해 __상태 머신 기반 구조__ 로 전환하는 것이 바람직하다.

```cpp
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
            ~~~
        }

        void Respawn() {
            ~~~
        }
    };
```
> Monster State 구조체

현재 MonsterState는 데이터 필드와 기초적인 초기화 로직(Initialize, Respawn)만을 포함하는 데이터 컨테이너에 가깝다.  
하지만 몬스터의 행동 패턴이 복잡해짐에 따라(추격, 이동, 공격 등), 기존의 MonsterAI 내 단일 메서드 기반 순차 로직은 코드의 가독성을 해치고 조건문(if-else)의 비대화를 초래한다.

상태 기반 행동 위임: 몬스터의 상태(State)를 정의하고, 현재 상태에 매핑된 전담 메서드를 호출하여 로직을 처리한다.
```
enum class State { IDLE, CHASE, ATTACK, DEAD };

// 몬스터 AI 메인 루프 (Tick마다 호출)
void UpdateMonster(Monster& monster) {
    // 1. 공통 상태 체크 (예: 사망 확인)
    if (monster.hp <= 0 && monster.state != State::DEAD) {
        ChangeState(monster, State::DEAD);
    }

    // 2. 상태 기반 행동 위임 (State-based Delegation)
    switch (monster.state) {
        case State::IDLE:   HandleIdle(monster);   break;
        case State::CHASE:  HandleChase(monster);  break;
        case State::ATTACK: HandleAttack(monster); break;
        case State::DEAD:   HandleDead(monster);   break;
    }
}

// 개별 상태 전담 메서드 (Delegated Methods)
void HandleIdle(Monster& monster) {
    ~~~
}

void HandleChase(Monster& monster) {
    ~~~
}

void ChangeState(Monster& monster, State newState) {
    ~~~
}
```
> 상태 머신 의사 코드

## 4. 시연
![gif 로드 실패](images/SkillAOI.gif)
![gif 로드 실패](images/BossMonster.gif)

## 5. 참고 

[MonsterData.h](CoreLib/MonsterData.h)  
[MonsterState.h](CoreLib/MonsterState.h)  
[ZonsState_Monsters.cpp](CoreLib/ZonsState_Monsters.cpp)  
