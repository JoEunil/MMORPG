# 사냥터·전투·포션 밸런스

사용자가 추천안 전체 적용을 승인했다. 기준 PR67 병합 fcd12f4에서 codex/hunting-balance를 분기했다. 계획은 HUNT_BALANCE_PLAN.md. 제품 코드는 데이터·스폰·표현 연결에 한정하며 사용자 몬스터 AI/state/배회/추적/phase 순서·성장표·DB 스키마·패킷은 변경하지 않았다.

## 구현
- 플레이어 Q/E/R 계수30/120/180, MP0/100/160. 기존 범위·phase 시점·쿨20/120/80tick 유지.
- 별도 monsterSkillList는 같은 ID를 유지하며 기본계수5/강화특수15/보스100. 기존 AI의 공격 선택 순서는 그대로다.
- 서버 HuntingZoneData는 구역별 HP/공격/EXP/골드를 정본으로 가지며, 프로세스 수명의 immutable MonsterData 복사본을 스폰에 주입한다. 처치 보상은 캐릭터의 현재 구역이 아니라 처치된 몬스터의 gold를 전달한다.
- 일반/강화/보스 총수는 Z1=0/0/0,Z2=108/12/0,Z3=116/16/0,Z4=123/20/1,Z5=132/24/0,Z6=139/28/1. Unity 저작의 기존25셀 가중치와 빈 셀을 보존해 결정적으로 분배하고 서버가 export 행렬을 직접 소비한다. 기존 중앙 보스 공간을 유지한다. 모든 구역의 실제 이동/난이도 완성으로 주장하지 않는다.
- HP1500/MP500, 공용120tick(6초), 처치 HP/MP각1 확정 정책 유지. 성장 상한100/승급HP+500 유지.
- 서버 JSON format3에 monsterSkills/huntingZones를 추가하고 기존 importer로 실제 asset에 반영했다. 기존 wire maxHp로 몬스터 바를 표시하며 구역을 추측해 HP를 바꾸지 않는다. World씬에는 기존 ServerContent GUID를 연결했다.

## 리뷰·검증
독립 리뷰 P2: legacy spawn 저작이 boss 없는 구역 보스를 허용하는 불일치 발견. 원담당자가 fallback과 명시 저작 검증을 Zone4/6으로 제한하고 fixture/Inspector를 정합시켰다. 같은 리뷰 담당 재검토 차단0.

서버/MSVC와 UnitTests Release 빌드 완료. 첫 제한 환경 MSBuild worker 연결은 진행이 없어 우리 빌드만 취소하고 사용자 환경 단일 worker로 수행했다. 새 구역명 header의 UTF8 인코딩 컴파일 오류는 원담당자가 명시 UTF8바이트 literal로 수정했고 재빌드 성공했다. 전체 프로젝트의 문자셋 옵션은 변경하지 않았다.

직접 영향 서버4검사 통과: HuntingDataKeepsCombatShapeAndUsesZoneStatsAndRewards, StarterLayoutLeavesEntryCellsEmptyAndBuildsAllServerReferences, MonsterRewardUsesKilledMonsterGoldInsteadOfCharacterZone, ItemUseCommitsOnceAndRejectsWithoutConsumption. Unity 콘텐츠2/스폰1 검사 통과. 모드 조회 전 EditMode 선택0 결과는 검증 통과로 세지 않았고 등록 목록을 읽어 PlayMode로 정확히 실행했다. 기존 통과 검사는 반복하지 않았다. 범위는 서버 데이터·저작/소비·비동기 보상·아이템 소비 계약이 분리되어 서버4/Unity3으로 제한했다.

## 실제 실행 관찰
새 MainServer와 Windows Player를 분리된 HuntingBalance 폴더에 만들고 기존 ItemsCaster 실행본을 보존했다. test1 캐릭터11/Lv4는 저장 위치 Zone5(383.835,338.842)로 정상 진입했다. E MP10000→9900, 권장16~20레벨 안내(최초 한글 glyph는 깨짐)를 확인했다. 교전 후 사망해 정상 Return to town으로 Zone1(10,10)/HP11500/MP10000으로 귀환했다. 이를 전체 난이도 적합성 통과로 표현하지 않는다.

마을에서 E/R 두 차례 사용 후 MP9480, MP포션 사용으로9980(+500)·Cooldown6.0s를 실제 화면에서 확인했다. 귀환 직후 슬롯 수량은 '?'였고 첫 클릭은 재조회, 조회 뒤 두 번째 클릭으로 실제 소비됐다. 기존 warmup 수명 문제는 후속 이슈로 구분하며 자동 소비 재시도는 추가하지 않았다. HP1500 효과와 구역별 보상/스폰 총수는 자동 검사 근거이며 모든 구역의 실전 처치를 수행하지 않았다.

실전에서 발견한 한글 glyph 문제는 원담당자가 기존 ChatScreen과 같은 Windows OS Malgun Gothic 폰트 생성·수명 정리를 WorldScreen에 최소 적용했다. 폰트 파일을 배포하거나 새 FontManager를 만들지 않았다. 최종 Player 표시 재확인은 아래에 추가한다.

근거: ROOT/UnityClient/Builds/ClientGrowthHUD/verification/hunting-balance. server-tests.xml, unity-content-tests.json, unity-spawn-test.json, player-build.json, mp-before-500.jpg, mp-after-500-cool6.jpg, overlevel-zone-death.jpg. PC 입력 허용을 확인한 배치이며 다른 창 입력 감지 때는 중단하고 재허용을 받았다. DB 직접 수정·리셋·강제 종료·정책 우회 없음.

최종 한글 폰트 보완 Player 빌드 Succeeded/오류0. 기존 데이터/스폰/소비 검사는 반복하지 않았다. 재시작 후 창목록 조회에서 사용자 물리 Escape로 Computer Use가 중단돼 이번 턴의 추가 입력은 중지했다. 따라서 보완본의 실제 한글 렌더는 미확인이다. 해당 제한을 검수·인계에 유지하며 PR 게시 승인 요청은 최종 확인 뒤 준비한다.

2026-11-05 입력 재개: 최종 Player로 test1 자동 로그인·저장 마을 위치(10,10)/캐릭터11 Lv4 진입. 실제 좌상단 한글 마을/몬스터 없음 안내 정상 렌더 확인. MP9980·HP11500·HP포션22/MP2·Gold100020의 이전 상태 유지 확인. 화면 korean-label-final.jpg 기록 후 게임·서버·로그인 정상 종료. 신규 수치/소비/스폰 검사는 반복하지 않았다. 전체 구역 추천 난이도 실전평가까지 완료한 것은 아니다.

최종 종합 검수 통과·필수0. 최종 한글 화면·MP회복/6초·자동 검사·실제 적용 범위를 대조했다. 모든 구역 난이도/보스 체감 미평가와 귀환 후 첫 클릭 조회 이슈를 제한으로 명시하며 게시할 수 있다. 기존 통과 검사를 반복하지 않았다.

게시 준비: 구현후보4e66f7e(42파일), HUNTBAL01/r1. Unity Coordinator 앱 U0C9ZRTQ6F8 발신, 부모1793873237.268829/요청1793875453.089969. 실제 receiver pid31992/listening/health9.3초·등록스레드 확인 후 요청. 현재 회신 미수신·미소비, push/PR게시 없음.

사용자가 현재 채팅에서 승인. 승인 대상 HUNTBAL01/r1 구현후보4e66f7e+기록0c505ff를 그대로 push하고 PR68 https://github.com/JoEunil/MMORPG/pull/68 게시. 원격 master incoming0 확인. Slack 회신 수신을 가장하지 않았으며 사용자 병합/Pages 배포는 수행하지 않았다.

후속 사용자 변경: 몬스터 처치 시 HP·MP 포션 자동 지급을 제거했다. 경험치와 몬스터별 골드는 유지, 기존 보유 포션의 조회·소비·HP1500/MP500·6초도 유지한다. 다른 보상 기능이나 후속 기능 계획은 추가하지 않았다. 기존 보상 fixture는 골드 메시지와 포션 지급0건을 함께 확인하도록 바꿨다.

처치 포션 제거 검증: MainServer/UnitTests Release 빌드 통과, MonsterRewardUsesKilledMonsterGoldInsteadOfCharacterZone 1개Passed(골드 유지·포션지급0). 기존 검사/Unity입력 반복없음. stopped상태에서 바탕화면BAT의HuntingBalance 서버exe를 새 산출물로 교체, 새세션미기동. 도중 PR68 병합9f12768을 확인해 동일소스기준 codex/remove-kill-potions 분기. 앞선 포션 지급 기록은 과거 검증이며 현재 동작이 아니다.

처치 포션 제거 최종 게시검수 통과·필수0. 새 실전 처치는 미실행으로 구분하며 승인된 기존 보상·포션소비 보존을 확인했다.
