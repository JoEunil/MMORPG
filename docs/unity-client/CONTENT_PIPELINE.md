# 게임 데이터 공유 경로

현재 게임 규칙의 정본은 서버 C++ Data다. 맵·배치의 정본은 Unity다. 서로 다른 소유권을 같은 방향으로 강제하지 않는다.

| 대상 | 정본과 방향 | 출력·반영 |
| --- | --- | --- |
| 스킬·몬스터 능력/phase | `CoreLib/Skills.inc`, `MonsterSkills.inc`, `Monsters.inc` → 실제 C++ Data 테이블 → 독립 exporter → Unity importer | `UnityClient/Exports/ClientData/ServerContent.json`, formatVersion4. `skills`와 `monsterSkills`를 분리하고 기존 `ServerContent.asset` 갱신. |
| 사냥터 능력·보상 | `CoreLib/HuntingZoneData.h` → 같은 exporter/importer | `huntingZones` 6행. 권장 레벨·HP·공격·EXP·골드를 공유한다. 실제 몬스터 체력바는 기존 wire `maxHp`가 정본이다. |
| 성장 규칙 | `CoreLib/GrowthData.h`, `Growth.inc` → 기존 exporter → 기존 Unity importer | 동일 JSON의 `growth`: 상한100, 레벨별 누적 EXP·최대 HP 증가량100행. `CharacterState::AddExperience`가 승급에 사용하며 현재 검증 범위는 HANDOFF를 따른다. |
| 아이템 정보·소비 효과 | `CoreLib/ItemData.h` → 기존 exporter → 기존 Unity importer | 동일 JSON의 `items`: 서버 itemMap의 ID·분류·이름·무게·HP/MP 회복량·쿨타임·무기 공격 보너스. 아이콘은 기존 Unity 에셋을 사용한다. 미등록 ID는 추측하지 않는다. |
| 패링 시간 | `CoreLib/SkillData.h` → 기존 exporter → 기존 Unity importer | 동일 JSON의 `parry`: 방어창5틱·쿨60틱·1회. 실제 수락·남은 틱은 서버 응답이 정본이다. |
| 맵·구역·충돌 | Unity 씬/MapAuthoring → WorldMapExport/WorldLayoutExport → 서버 | `CoreLib/Generated/WorldMap.inc`, FormatVersion/ContentVersion 기존 계약 유지. |
| 몬스터 스폰 배치 | Unity MonsterSpawns → MonsterSpawnExport → 서버 | 기존 `MonsterSpawns.inc`의 구역별 일반/강화 6×25 행렬과 보스 수. 총수 저작을 결정적으로 분배하며 몬스터 AI와 구분한다. |

이번 미게시 후보는 format4의 items로 통합했으며 weaponAttackBonus도 같은 경로로 공유한다. 이전 format3 JSON은 importer에서 명시 거부하고 마지막 유효 에셋을 보존한다. 새 exporter 결과를 기존 GUID 에셋에 가져와 함께 적용한다.

## 서버 규칙 수정과 로컬 테스트

서버 정본 `.inc`는 이제 자동 생성물이 아니라 개발자가 관리하는 C++ 초기값이다. 스킬·몬스터 Data 소비처는 Unity 파일을 include하지 않는다. SkillData.cpp와 MonsterData.h가 서버 정본을 포함하므로 서버 빌드·테스트에는 Unity 실행이 필요 없다. 서버 Data 타입/저장소를 조회하며 처치 때마다 새 Data 객체를 만드는 구조가 아니다.

해당 저장소 체크아웃에서 실행한다.

```powershell
./tools/Export-ServerContent.ps1 -OutputPath ./UnityClient/Exports/ClientData/ServerContent.json
```

기존 MSVC로 exporter만 빌드하고 실제 `Data::skillList`, `Data::monsterSkillList`, `Data::monsters`, `Data::huntingZones`, 정적 `Data::growthLevels`를 읽는다. 서버·DB를 시작하거나 별도 런타임 .dat 로더를 사용하지 않는다. 규칙 변경을 게임에 적용하려면 서버를 재빌드·정상 전환한다.

Unity에서 `Tools → Client → Content → Import Validated Server Content`를 실행한다. 기존 에셋 GUID/Scene 참조는 보존하고 값만 가져온다. Unity에서 서버 원본을 덮어쓰는 메뉴·API는 제거했다. Unity의 직렬화 타입/기본 생성값은 화면/fixture 호환용이며 서버 규칙의 정본이 아니다.

현재 작업은 Git 정본과 실행 Editor 사본이 다르다. Git 정본은 HANDOFF의 CAND이며 실제 Editor는 ROOT/UnityClient다. 이때 위 OutputPath에 실제 Editor의 `Exports/ClientData/ServerContent.json` 절대 경로를 주거나 검증된 산출물만 복사한다. 사용자 작업 중인 다른 서버 사본을 자동 덮어쓰지 않는다.

## 통일한 규칙과 남은 차이

- 데이터마다 정본과 전달 방향 하나를 명시한다. 생성 JSON과 Unity 에셋을 별도 원본으로 편집하지 않는다.
- 반영 전에 전체 ID·참조·수치·phase·형식 버전을 확인한다. JSON은 UTF-8·ID 순서·float roundtrip으로 결정적으로 출력한다.
- 스킬·몬스터·성장·아이템 정보·패링 시간은 JSON 한 파일로 함께 전달한다. 같은 디렉터리 임시파일을 완성한 뒤 동일 볼륨 rename으로 교체해 부분 JSON을 노출하지 않는다.
- Unity importer는 모든 값을 검증한 뒤 기존 에셋에 대입한다. 잘못된 입력은 기존 값·파일·GUID·dirty 상태를 보존한다. 에셋 저장 오류 시 live 객체를 복원하지만 Unity의 디스크 저장 전체가 원자적이라는 보장은 하지 않는다.
- 맵·스폰은 기존 단일 `.inc` 임시파일 교체를 그대로 사용한다. 포맷/버전/교체 구현이 하나의 framework로 완전히 같아진 것은 아니다. 스폰 출력에는 아직 별도 schema version이 없다.

아래 문단은 최초 경로 전환 때 보존한 범위의 기록이다. 이후 포션·패링 등 현재 소비 데이터는 위 표와 HANDOFF를 따른다.

최초 경로 전환은 5개 스킬·3개 몬스터 값을 그대로 보존했다. 몬스터 AI/이동·전투 판정·패킷·DB·맵·스폰은 바꾸지 않는다. MonsterData 생성자의 phase1~8 검사만 추가해 서버 수동 저작 오류가 고정 배열 복사 전에 차단되게 했다. 새 ID/콘텐츠 구조를 추가할 때에는 exporter와 importer의 지원 범위를 함께 갱신한다.

## 성장 데이터 도입 당시 기록

아래의 미실행 표기는 도입 시점의 기록이며 현재 승급·실행 상태는 HANDOFF와 관련 완료 이력을 따른다.

성장은 level·requiredTotalExp·maxHpIncrease 세 필드의 정적 표다. 새 Manager·상태 계층·런타임 로더 없이 기존 공유 경로에 추가했다. Unity는 빈 직렬화 구조에 서버 값을 가져오며 자체 성장 곡선을 만들지 않는다.

100행의 초기 누적 EXP는 50×L×(L−1)/2, Lv2부터 HP 증가량500으로 채웠다. 공식은 초기 데이터 작성에만 사용했으며 실제 소비자는 행을 조회한다. 이는 초기 튜닝안이고 균형 평가는 하지 않았다. 데이터 공유 PR59와 구분되는 후속 브랜치에서 막타 EXP 지급을 승급·HP 증가·100레벨 상한에 연결했다. 기존 상태 전파와 정상 저장 경로를 재사용하며, 집중2검사와 CoreLib빌드는 통과했다. 실서버 적용·사냥·저장 재접속은 미실행이다. 기존 스킬·몬스터 값과 GUID를 보존했다.
