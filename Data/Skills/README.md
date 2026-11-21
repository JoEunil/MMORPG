# 스킬 데이터

플레이어는 `PlayerSkills.csv`·`PlayerSkillPhases.csv`, 몬스터는 `MonsterSkills.csv`·`MonsterSkillPhases.csv`를 사용한다. 두 카탈로그의 ID는 별개다. UTF-8 CSV이며 첫 행은 필드 이름이다.

- 기본 정보: `skillID,flags,mana,coolDown`. `skillID`는 0부터 중복·누락 없이 오름차순으로 정의한다.
- 단계 정보: `skillID,phaseIndex,tick,actionID,rangeType,range1,range2,attack`. `skillID`는 같은 카탈로그의 기본 정보를 참조한다. `phaseIndex`는 각 스킬마다 0부터 순서대로 정의하며, 모든 스킬에 적어도 한 단계가 있어야 한다.
- `flags`: `0 = 없음`, `1 = 치명타`, `2 = 광역`, `3 = 치명타 + 광역`.
- `actionID`: `0 = Charging`, `1 = Hit`.
- `rangeType`: `0 = None`, `1 = Circle`, `2 = Rectangle`, `3 = Boss1_1`, `4 = Boss1_2`, `5 = Boss2`.
- `range1`·`range2`는 유한한 음이 아닌 실수다. None은 두 값이 0이고 Charging이어야 한다. 다른 범위는 `range1`이 양수이며, Rectangle·Boss1_1·Boss1_2는 `range2`도 양수여야 한다.
- `tick`은 직전 단계 처리 후 대기할 틱 수다. `mana`·`coolDown`·`attack`은 기존 서버 필드의 음이 아닌 정수다.

빌드 시 실행 파일 옆 `Data/Skills`로 자동 복사한다. 서버는 스킬 카탈로그를 먼저 확정하고, 이후 몬스터 데이터를 로드할 때 스킬 참조를 검증한다. 수정은 재빌드·재시작 후 적용된다.
