# 몬스터·사냥터 데이터

UTF-8 CSV이며 첫 행은 필드 이름이다. 열 순서는 바꿀 수 있다. 숫자 ID는 기존 서버의 의미를 유지한다.

| 파일 | 한 행의 의미 |
|---|---|
| `Monsters.csv` | 기본형 능력치·이동·추적·부활·보상. `monsterID`는 0=일반, 1=강화, 2=보스 |
| `MonsterPhases.csv` | 기본형의 HP별 phase. `(monsterID, phaseIndex)`로 식별 |
| `MonsterSkillSteps.csv` | phase 내 스킬 사용 순서. `(monsterID, phaseIndex, stepIndex)`로 식별 |
| `HuntingZones.csv` | 마을을 포함한 Zone별 레벨 범위·몬스터 HP·공격력·보상 |
| `HuntingSkillOverrides.csv` | 사냥터별 특정 스킬 단계의 `skillID`·`delayTicks` 교체 |
| `HuntingSpecies.csv` | Zone·기본형별 표시 이름과 스냅샷용 `speciesID` |

기본형 ID는 0부터, Zone ID는 1부터 연속이어야 한다. phase·step은 각 소유자 안에서 0부터 순서대로 정의한다. phase는 1~8개, 각 phase의 step은 1·2·4·8·16개다. `hpThreshold`는 (0, 1] 안에서 엄격히 감소해야 하며 실제 HP 임계값은 로드 시 계산한다.

스킬 ID는 `Data/Skills/MonsterSkills.csv`를 참조한다. 사냥터 교체는 실제 존재하는 phase·step만 대상으로 하며 중복 지정은 오류다. `bossHp=0`인 Zone에는 보스 표시 정보를 정의할 수 없다. 모든 생성 가능한 기본형에는 이름·species ID가 필요하다. species ID는 Zone 안에서 중복될 수 없으며, 같은 보스 ID를 서로 다른 Zone에서 재사용할 수 있다.

서버는 스킬 → 기본 몬스터 → 사냥터 순서로 로드하고, 전체 사냥터 검증이 끝난 뒤 데이터를 확정한다. 런타임 몬스터가 참조하는 데이터의 주소는 종료까지 유지한다. 빌드 시 실행 파일 옆 `Data/Monsters`로 자동 복사하며, 수정은 재빌드·재시작 후 적용된다. 월드 맵·스폰 배치와 AI 처리 방식은 기존 구조를 사용한다.
