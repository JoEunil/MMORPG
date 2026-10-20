# 성장·사냥터 콘텐츠 다음 작업 후보

조회 대상 소스의 절대 루트: `C:/Users/joeun/Desktop/workspace/Game-UnityClient/UnityClient/Builds/C10-Delivery/source`.

## 권장 후보

다음 최소 사냥터 콘텐츠 작업은 **기존 5×5 배치 형상을 유지하면서 Zone별 스폰 밀도 계수를 두는 것**이다. 현재 C15 셀별 112마리 프로필은 6개 Zone 모두에 그대로 반복 적용된다. 구역마다 별도의 25셀 배열을 복제하는 6×25 일반화는 같은 경로 모양을 여섯 번 중복 저장하므로 지금 단계에는 필요하지 않다.

C15에 선택적 6개 Zone density-percent 배열만 추가하는 방안을 권한다. 비어 있으면 100%로 계산해 현재 개체 수를 그대로 유지하고, 설정된 Zone에서는 기존 각 셀 수를 백분율 적용 후 정수 내림해 산출한다. 보스는 중앙 셀 1마리로 고정한다. 첫 pass는 낮은 density로 시작해 후속 Zone의 몬스터 수를 늘릴 수 있으나, 실제 백분율/마릿수는 살아 있는 캐릭터의 교전·HP·EXP 관찰 전 확정하지 않는다. 이는 사냥 위험도/동선 구성이지 레벨업 규칙은 아니다.

## 실제 서버 계약과 근거

- `CoreLib/CharacterState.h`에는 `exp`와 `level` 필드가 따로 있고, `Resources/DB/init.sql`의 characters 기본값은 level 1 / exp 0이다. 저장값을 읽고 전파하는 경로가 있다.
- `CoreLib/ZoneState_Skills.cpp`의 캐릭터가 몬스터를 처치하면 `MonsterData.reward.exp`만 누적한다. 경험치 threshold나 level 증가 계산은 해당 경로에 없다. 클라이언트는 서버의 exp/level 값을 보여야 하며 레벨업 수치를 자체 산출하지 않는다.
- `UnityClient/Exports/ServerData/Monsters.inc`는 몬스터 0/1/2의 기존 HP·공격·리스폰·속도·스킬 단계와 보상 10/30/300을 정한다. `Skills.inc`에는 실제 스킬 0–4가 있으며 캐릭터 스킬 슬롯은 3개다. 이번 후보에서 종류, 스탯, 스킬, 보상은 바꾸지 않는다.
- `CoreLib/ItemData.h`의 HP/MP 포션은 ID·이름·무게만 있다. `CoreLib/NonZoneHandler.cpp`의 실제 opcode 분기에 인벤토리 조회는 있지만 아이템 사용 분기는 없다. `ZoneState_Skills.cpp`는 캐릭터 HP를 0으로 제한하며 검색한 서버/DB 경로에 부활이나 포션 회복 동작은 없다. 기능을 새로 제안하지 않는다.
- 현재 `UnityClient/Assets/C02/Content/C15MonsterSpawns.cs`의 regular/strong 셀 배열과 `C15MonsterSpawnExport.cs` 검증/export는 단일 25셀 프로필이다. `Exports/ServerData/MonsterSpawns.inc`는 (2,2) 보스 예약, 시작 및 인접 시작 셀 비움, regular 94 + strong 17 + boss 1 = Zone당 112마리다. `CoreLib/ZoneState_Monsters.cpp`는 이 배열을 모든 Zone에 반복 사용한다. `CoreLib/Config.h`는 `ZONE_COUNT=6`, `MAX_MONSTER_COUNT=1000`이다.
- 기존 Zone 전환은 StateManager/ZoneState와 실제 클라이언트 `UnityClient/Assets/C02/C02WorldScreen.cs`의 서버 상태 갱신으로 처리된다. 후보는 전환·좌표·Zone ID·패킷·DB를 변경할 필요가 없다.

## 최소 구현 경계와 검증

데이터 변경 파일은 `UnityClient/Assets/C02/Content/C15MonsterSpawns.cs`, 기존 C15 exporter/Inspector, 생성 산출물 `UnityClient/Exports/ServerData/MonsterSpawns.inc`, `CoreLib/ZoneState_Monsters.cpp` 및 직접 영향 native 검사로 제한한다. Exporter는 배열을 비웠을 때 기존 112 fallback을 만들고, 설정 시 정확히 6개 Zone, 50–200% 범위, 계산 후 Zone별 1000 이하, 중앙 보스 고정을 검증한다. 서버 초기화는 현재 Zone에 대응하는 density만 적용한다. 셀별 y-major 형상은 기존 25개 배열을 재사용한다. density 미설정/100이면 기존 스폰 순서·위치 난수 입력 분포·마릿수 결과가 유지되어야 한다.

자동 검증은 두 시나리오로 제한한다.

1. C15 export/validation: legacy fallback과 100%가 동일한 112를 출력하는지, 6개 값이 아닌 입력·음수/범위 오류·Zone 예산 초과를 거부하는지 확인한다.
2. 서버 초기화: Zone별 계수 적용, 시작 안전 셀 비움, 셀 좌표 및 중앙 보스 1마리, 기존 monster ID/type·기본 보상 보존을 확인한다. EXP는 처치한 기존 몬스터의 10/30/300 값만 더해지는지 확인하고 level 자동 변경이 없음을 보존한다.

자동 검증 후 동결된 전체 작업은 살아 있는 전용 캐릭터 한 명으로 실제 입력 1회만 수행한다: 시작 주변 안전 확인 → 일반/강한 몬스터 구간에서 공격·HP/EXP 변화 관찰 → 한 단계 높은 density Zone으로 실제 이동. 테스트 계정이 HP 0이면 이 생존 검증은 미실행으로 기록한다. 소스에서 기존 부활·회복 경로가 확인되지 않았으므로 새 회복 기능이나 추정 밸런스 결과를 추가하지 않는다.

## 범위 및 복구

백분율 방식은 기존 셀별 밀도 형상을 보존하고 Zone당 한 개 값으로만 난이도 입력을 조정한다. Zone별 25셀 배열은 서로 다른 지형/스폰 형상이 실제 기획으로 승인될 때까지 미룬다. 0%에 가까운 값은 시작 목적/보스 고정과 어긋나므로 50–200%를 authoring 검증 범위로 제한하고, 실제 콘텐츠 비율은 생존 입력 관찰 후 안전한 작은 폭으로만 조정한다. 문제가 있으면 C15의 Zone density 배열을 비워 기존 112 fallback으로 복귀한다. DB 수정·초기화·마이그레이션은 없다.
