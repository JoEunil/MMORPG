# C05 제품·입력 도우미 독립 리뷰

**최신 판정: 최종 수정 delta 기준 코드상 차단 문제 없음.** 초기 지적·해시·실패 이력은 보존한다. 최신 기준과 실행 한계는 문서 끝의 재리뷰를 따른다. 실제 전투·HP0 통과 판정은 아니다.

2026-10-18 KST. **초기 판정: 차단 문제 있음.** 제품의 원자 취소·게임 UI 문구와 아래 다섯 도우미 검증 경계를 Coordinator에게 보고했고 최소 수정 재배정이 수용됐다. 이 문서만 작성했으며 제품/테스트/Scene/Editor/빌드/Play/입력/서버/DB/Slack를 조작하지 않았다. 다른 작업자의 누적 C01~C04 변경을 새 C05 변경으로 다루지 않았다. 수정 중인 runtime/Core는 초기 동결 기준으로만 판정하며 새 해시 delta 재리뷰가 필요하다.

## 제품 수정 필요

1. **P2 — HP0/disable 취소 사이에 미소비 행동이 송신될 수 있음.** C02WorldScreen.StopActions(336–340) 및 C05CombatScreen.OnDisable(168–169)은 UpdateSkill(255) 뒤 CancelMovement/CancelMove를 호출한다. ViewData.UpdateSkill은 기존 `_lockMove`에서 slot255뿐 아니라 dirty=true를 만든 뒤 lock을 푼다. 그 사이 ClientTick.GetActionState가 들어오면 취소 완료 전에 아직 소비되지 않았던 이동 좌표 또는 마지막 좌표를 dirty=true/slot255로 소비하고 ACTION을 송신한다. 서버는 HP0 이동을 거부하지 않으므로 실제 영향이 있다. 취소 전에 이미 소비된 in-flight 한 요청을 소급 취소할 수 없다는 기존 한계와 별개다. 최소 수정은 같은 `_lockMove`에서 waitSkill255+dirtyfalse를 원자로 지우는 작은 CancelAction 경계를 사용하는 것이다. 독립 pending cast를 보존하는 기존 CancelMove 의미를 바꾸거나 새 범용 queue/gate를 만들 필요는 없다. HP0/disable 두 경로와 취소 후 GetActionState 비송신 회귀가 필요하다. 현재 순차 fixture는 두 호출 사이 소비를 다루지 않는다.
2. **P2 — 게임 HUD에 내부 계약을 노출.** C05CombatScreen의 `[server]`, 숫자 dir/축, requested 뒤 phase/MP·reuse silent rejection 설명, result의 phase 번호/좌표/피해 ACK 설명은 플레이어 행동 결정에 불필요한 구현 문구다. 새 C05 영역만 `Ring requested`, phase0 `Ring cast`/phase1 `Ring released`, `Line aim: South` 등 기능적 방향으로 최소 보정하고 설명은 문서로 옮긴다. 서버 확정과 요청 상태를 구분하고 원래 직사각형 N/S 의미는 유지한다. 피해/처치/쿨다운 성공을 추가로 확정하지 않는다. 기존 C02 debug status 전체 리팩터링은 범위 밖이다.

## 도우미 수정 필요

기준은 `Assets/Tests/C02/C05LiveBatchUiTests.cs` C09526A2…다. 아래는 제품 결함과 구분되는 테스트 전제/관찰 문제다.

1. **P2 — 기본 공격의 특정 ID 명중을 보장하지 않는 oracle.** AccountOneCombatAndMonsterRespawn은 type0 raw 위치로 한 번 접근한 뒤 키1의 특정 선택 ID HP 감소를 필수로 기다린다. 몬스터는 이동하며 ACTION에는 targetID가 없고 basic range2의 비AOE break도 cell별이다. 실제 서버가 정상 시전해 다른 대상을 맞혀도 실패할 수 있다. 키1은 자기 phase·독립 서버 MP/표시 일치를 필수 근거로 삼고 실제 피해는 관찰된 ID만 기록한다. 필수 damage 근거 자체는 제거하지 말고 Ring AOE의 실제 type0 HP 감소로 추적 ID를 고정한다. 이후 유한1~2회 Ring 재사용으로 같은 ID HP0을 확인하는 최소안이 낮은 damage50 기본 공격10회를 강제하는 것보다 계약에 맞다. 총 시전/시간 예산을 늘리거나 피해를 주입하지 않는다.
2. **P2 — HP0 시각을 늦게 잡고 이미 살아난 ID도 반복 Full로 재등장 PASS 가능.** deathObservedAt은 raw HP0 callback이 아니라 key3/button·후속 공격을 마친 뒤 현재 dictionary의 HP0을 찾았을 때 설정한다. 공격 중 먼저 사망하면 시각이 늦어진다. earlyAliveFullBeforeRespawnThreshold=true여도 이후14초를 넘은 반복 alive Full에서 PASS하므로 첫 재등장 약15초를 확인한 근거가 아니다. 추적 ID의 실제 첫 HP0 수신 시각을 callback에서 확보하고, 이후 최초/이른 alive Full 시각과 실제 Visual 복원을 구분해 기록한다. Ring 뒤 추적 ID가 정해져도 이미 받은 같은 ID의 HP0 시각을 놓치지 않아야 한다. wire 순서/세대 한계가 있어 정밀15초 서버 생성 시각을 보장하지는 않는다.
3. **P2 — Visual 존재 판정이 dictionary만 확인.** MonsterVisualExists는 Contains(id), HasRespawnFull은 dictionary/state/LastFullSeen만 읽는다. Visual GameObject가 없거나 비활성인데도 시각 복원=true를 기록할 수 있다. entry.Visual의 Unity-null 및 activeInHierarchy를 확인해 실제 생존·표시 활성 상태를 대조한다. HP0 제거 기대와 살아있는 Full 복원 기대를 그대로 유지하며 렌더/애니메이션 완성까지 추정하지 않는다.
4. **P2 — 정상 전투 HP/MP의 독립 raw/제품/HUD 대조 누락.** firstMp 등의 값이 World.OwnFull에서만 오고 정상 구간의 C05 vitalsText와 독립 players[ownID] HP/MP를 비교하지 않는다. HP0 branch에서만 UI 일부를 검사하므로 정상 HUD 숫자 갱신이 깨져도 통과할 수 있다. 같은 main-thread callback들이 끝난 시점에 독립 raw HP/MP/max, World.OwnFull, C05 vitals를 한 번 비교하면 충분하다. collector가 사용하지 않는 좌표/dir까지 확장할 필요는 없다.
5. **P2 — 계정2가 관찰 시작 전에 정상 피격으로 HP0이면 진입 조건에서 거부.** AccountTwoHpZeroAndDisconnect는 world.CanAct를 요구한다. 별도 BossAggro 단계와 HP0 관찰 시작 사이에도 보스가 공격하므로 이미 HP0이 된 정상 상태는 즉시 assertion 실패한다. 이 관찰 단계는 account2/5012·Zone1·InWorld·HasOwnFull을 요구하되 HP0도 허용하고 독립 raw 수신을 기다린 뒤 기존 HP0 표시/버튼/입력 차단 검사를 적용한다. phase entry 당시 이미0이면 그 사실을 표시하며 이 단계에서 새로 사망했다고 주장하지 않는다. 제품 CanAct=false 기대는 그대로다.

비차단 캡처 메모: Capture는 기존 고정 PNG를 삭제하지 않아 재실행 시 이전 파일의 존재·길이만으로 성공 판정할 수 있다. 해당 C05 고정 캡처만 새 요청 전에 지우는 기존 C02 패턴 또는 생성 시각 확인이 최소 보완이다. 첫 실행에서 파일이 없었다면 그 자체가 제품 실패는 아니다.

## 확인한 코드와 통합 경계

- UpdateAction은 dir0..3/slot0..2/finite 좌표를 변형 전에 확인하고 기존 `_lockMove` 안에서 dir/x/y/slot/dirty를 함께 기록한다. GetActionState는 같은 lock에서 snapshot 및 slot255/dirtyfalse 소비를 수행한다. 별도 UpdateMove→UpdateSkill 분할 시전은 추가하지 않았다.
- Core ActionResultReceived는 enqueue 시 delegate snapshot과 enqueue 전/실행 시 lifetime guard를 갖는다. C05 소비자도 source/generation/활성 world/current own ID를 확인하며 현재 플레이어 slot0..2/phase0..1만 표시한다. monster 결과의 다단계 범위를 잘못 제한하는 구현이 아니다. 지원 caster/ID/slot/skill/phase/dir/finite/count 경계가 있다.
- C02의 readonly OwnFull/CanAct가 실제 확정 HP/MP 및 움직임을 기준으로 한다. HP0 뒤 키/버튼/지면 입력을 막고 기존 disconnect를 남긴다. Full·Delta 데이터 검증을 제거하지 않았다. 부활 버튼·피해 예측·서버 보안 정책·새 wire/DB/서버 변경은 추가하지 않았다. Full/Delta의 도착 순서/세대 없음에 따른 한계는 기존 계약을 유지한다.
- C05의 이벤트/버튼 listener는 disable 시 해제하고 UI를 비운다. ActionResult의 새 모델 구독에 과거 queued 결과가 전달되지 않는 Core 경계를 유지한다. 새 DI/Manager/queue/프레임워크는 없다.
- helper는 actual MouseState/KeyboardState를 Player frame에 나눠 전달한다. 제품 handler/Transform/행동 API를 호출해 live 결과를 주입하지 않는다. 고정 계정1/2 selector만 허용하고 계정 전환 환경은 finally에서 복구한다. DPAPI 비밀을 메모리로 읽고 실제 UI 목록의5011/5012를 확인한다. assertion actual/expected·로그에 비밀을 출력하는 경로는 발견하지 않았다.
- 각 phase는 기존120초 runner를 재사용한다. HP0 관찰85초, 유한 공격 예산과 연결 종료가 있으며 NOT OBSERVED를 성공으로 바꾸는 전체 PASS 문구 대신 완료/개별 관찰을 구분한다. 새 phase count로 key3와 별도 버튼의 실제 시전 관찰을 구분한다. HP0 영속 가능·자동 DB 복구/부활 없음·실제 미관찰은 유지해야 한다.
- 최종 전달 Builder2D79166F…/SceneE5F857DF…에서 enabled C05 component1의 login/world/버튼3/vitals/status/aim 참조와 Scene 내부 fileID 누락0을 파일로 확인했다. 기존 character content topstretch와 sizeDeltaX0를 유지한다. 새 HUD panel의 실제 renderer/input 가림과 실사용성은 입력 배치 대상이며 정적 참조 확인만으로 통과하지 않는다. 진행 중이던 V03 미술의 최종 PNG는 Coordinator 검수 근거와 별개다.

## 검토 기준 및 실행 근거

제품 초기 동결은 `Captures/c05-implementation.txt`의 handoff 기준이다. root가 재배정한 수정 이후 제품 해시는 다시 받아 해당 delta만 재리뷰한다.

| 제품/도우미 | 기준 SHA256 |
| --- | --- |
| C05CombatScreen.cs | `010FB6A8F056A29D3EEFC5D06B15B7BF2E64B09F7848F21E375FC112957E6D0B` |
| C02WorldScreen.cs | `CB804D5C367E19F7848CF9C5A3981F625C341EF27CF04865E82978F6C1BD40D3` |
| ClientCore DLL | `25C030AE5428A989207817C3ABC962378774238D914DFAC2FFFF1E60883D39C7` |
| C05CombatTests.cs | `DCA0BADCD459024A5E20B351A114F345418D369DC42D988F948ACDDAA7CD5251` |
| C05LiveBatchUiTests.cs | `C09526A25AF3247547E274D6BB2CC2C3093C1C38752D163410BB999F8119DF82` |
| C01LoginFlowTests.cs selector | `0BF179C7105F33C5029B580F299C1EF74A5DB501AE53D1C12D2BFF279E57372D` |
| C02EmptyListUiTests.cs expected account | `A31D4620150263B409412A30E2E38035E048B9142EC46408D5D8F120DAB0C125` |
| C02SceneBuilder.cs final art/HUD | `2D79166FD5CDBA1454AB9018BBB5126952699D71AA23D83C7F1A441BE9EA91D5` |
| C02.unity final art/HUD | `E5F857DF74F1982B837CDBC7CC32C9D2084BF7A11B23F93D02C3E09921D6A7FD` |

helper/selector/expected account/최종 Builder·Scene의 전체 해시는 리뷰어가 직접 읽었다. Core Interfaces705A39C1…/ViewDataEDC71C00…/ViewModel180AE1BA… 및 원본/Unity DLL은 초기 구현 기록을 따른다. 전체 HEAD diff는 C01~C04 누적 변경을 포함하므로 C05 범위 밖 리팩터링/원복을 요구하지 않았다.

구현 담당은 Core build0오류·기존 NU1900경고1, Unity compile0, C05 detached direct fixture6/6 및 C03 pure6/6을 보고했다. 초기 C05 fixture lifecycle 실패·CS0120 helper 컴파일 실패와 수정 이력은 보존한다. C04를 EditMode 직접 호출한10개 Destroy 오류/CloseWorld 기대 실패는 잘못된 실행 방식이며 C04 통과가 아니다. 적절한 PlayMode runner 회귀가 남아 있다. 초기 helper 준비 기록의 no compile 상태와 후속 담당 compile 결과도 구분한다. 아직 실제 C05 NUnit/Play/전투/HP0/재등장/캡처는 미실행이다. 위 제품·helper 최소 수정 후 동결 해시/영향 검증을 받고 재리뷰한다.

## 최종 수정 delta 재리뷰

**코드상 차단 문제 없음.** 초기 제품2건·helper5건과 비차단 캡처 메모를 수정한 동결 파일을 읽었다. 리뷰어의 쓰기는 이 문서뿐이고 빌드/Editor/Play/입력/테스트는 실행하지 않았다.

- CancelAction은 기존 `_lockMove` 하나에서 slot255와 dirtyfalse를 함께 지운다. C02 HP0와 C05 disable 모두 이 경계를 쓰고 뒤의 CancelMovement는 새 dirty 동작을 만들지 않는다. CancelMove의 독립 cast 보존 의미는 유지한다. 새 단일 API는 실제 경합을 고치는 최소 경계이며 gate/queue/범용 계층을 추가하지 않는다. 소비 이전 취소와 이미 소비된 한 in-flight 요청의 한계를 구분한다.
- Barrier rendezvous2000회로 취소/소비를 경합시킨 fixture는 취소 전에 소비된 원래 cast를 허용하면서 spurious dirty+NONE_SKILL 이동만 거부한다. 취소 후 clean state, 새 이동/시전 및 CancelMove의 cast 보존을 확인한다. 기존 HP0/disable·lifetime/원자 snapshot/invalid callback 기대도 유지했다.
- C05 HUD는 HP/MP 숫자와 Line aim South/North/West/East, Strike/Ring/Line requested·cast·released로 바뀌었다. 요청과 서버 결과를 구분하며 phase/dir/축/MP-reuse/피해ACK 내부 설명을 게임 UI에서 제거했다. HP0/stopped/disconnect 및 실제 server rectangle N/S 의미는 유지한다. fixture 문구 기대도 의미 있는 cast/released로 바꾸고 필터·세대 차단 기대는 보존했다.
- helper basic key1은 새 자기 phase와 독립 raw MP 감소 및 raw/C02/C05 vitals 일치를 요구한다. 특정 basic target 명중을 약속하지 않고 관찰한 HP 감소 ID만 기록한다. Ring은 활성 Visual을 가진 살아있는 baseline의 실제 type0 HP 감소를 필수로 요구해 nullable ID0 포함 추적 ID를 고정한다. 동일 ID HP0은 최대2회 추가 Ring/6.2초 간격·유한 시간/MP 예산 안에서 정상 입력으로만 확인한다. damage 검증을 없앤 수정이 아니다.
- raw MonsterDelta callback에서 ID별 최초 HP0 수신 시각을 확보하고 이후 최초 alive Full의 시각/상태와 실제 활성 Visual 최초 관찰을 보존한다. 반복 Full로 첫 시각/상태를 덮지 않는다. 최초 alive가14초보다 이르면 TIMING UNCONFIRMED로 분리하고 나중 Full을 최초15초 재등장으로 가장하지 않는다. raw HP0/제거/alive Full/활성 Visual 관찰과 정확한 서버 생성 시각은 구분한다.
- Visual 판정은 실제 GameObject의 Unity-null/activeInHierarchy와 live displayed state를 확인한다. 독립 raw/World/UI HPMP를 같은 프레임에서 대조한다. 이미 HP0인 계정2도 account2/Zone/InWorld/HasOwnFull과 fresh raw Full을 요구한 뒤 기존 HP0 입력 차단을 검증하고 phase entry 당시 이미0임을 기록한다. 새 death transition을 만들어내지 않는다.
- 캡처는 이 helper 전용 PNG3개만 허용하고 이전 파일 삭제 후 새 생성/길이를 확인한다. selector와 finally 복구·신규 phase-count·기존 native 입력 경로·NOT OBSERVED 분류·정상 disconnect를 유지한다. 제품 handler/Transform/서버 상태 주입은 추가하지 않았다. detached RunOracleChecks는 ID0 damage 선택·최초 zero/early alive 보존·inactive 거부/active 복원을 합성 시각으로만 검사하며 live 증거라고 주장하지 않는다.

직접 재확인한 최신 SHA256:

| 파일 | SHA256 |
| --- | --- |
| ClientCore/Interfaces.cs | `7492F09FFD7C6F2ADCADA51221AE961DBA2D4D1DC93B001BD16B194FA48034F7` |
| ClientCore/ViewData.cs | `C9662AE5F0A5C24755C6BBE2FEA23E701105A3C82304AB0933B69F908F3A624A` |
| ClientCore/ViewModel.cs | `180AE1BA4E307A27BD42C241088E0EAF618104EFC56F53F0363CA4F1D40CA4B6` |
| 원본 Release/Unity ClientCore.dll 양쪽 | `1D10D5AEF0D81622F29BE55E0796A1E27B679980EE664BADAD20BFA5A726063E` |
| C02WorldScreen.cs | `96C857D1DCB8EE4302494578EE5BBD477E7973EE79E212742752C2652E45EDD1` |
| C05CombatScreen.cs | `270EFD4E83EBA5901A15CE4AF2723DDD72CC6A2BFF77B9447A474753F653B98F` |
| C05CombatTests.cs | `4E4928CFE94EF18DE26749DBE1356BEE279420E63AA4837A258D15265853E30F` |
| C05LiveBatchUiTests.cs | `FC681760076F5B081D865D604C21FD650971A391FB918E7DFF15227D0CC0990A` |

Builder `2D79166F…`/Scene `E5F857DF…`/C01 selector `0BF179C7…`/C02 expected-account helper `A31D4620…`도 직접 해시를 확인해 이전 기준과 같았다. 새 서버/wire/DB/인증 변경은 이 수정 범위에 없다.

`Captures/c05-implementation.txt` 최종 revision의 담당 실행 근거: Core Release build exit0/errors0·기존 NU1900경고1, Unity recompile completed/failedfalse/errors[], API/수정 assembly 로드 후 C05 detached7/7+C03pure6/6+helper detached oracle checks 통과, Play/compiling/Scene dirtyfalse·console sample errors0/warnings0. compiling=true 동안 반환된 옛6/6 응답은 revision 근거에서 제외했다고 명시했다. 이는 리뷰어 직접 실행이 아니며 C04의 잘못된 EditMode 실행 실패를 통과로 바꾸지 않는다. **실제 NUnit/Play/전투/몬스터HP0·재등장/계정2HP0/네이티브입력/캡처는 아직 미실행**이다. 실제 배치에서 개별 NOT OBSERVED·TIMING UNCONFIRMED·alreadyHP0를 전체 통과로 합치지 않는다.

## 실제 첫 배치 실패 및 안전한 진단 delta

그 후 테스트 담당이 올바른 PlayMode 관련 fixture29/29와 B01 계정1 실제 로그인/목록 선택/서버 진입을 통과했다고 보고했다. 첫 AccountOneCombat은 UTC10:59:56의 WaitFor AssertionException으로 실패했다. `c05-account1-live-attempt1-fail.txt`에는 entry PNG 보존 뒤 첫 key1 PASS 전 실패만 있고 공유 runner는 보안상 label/예외 상세를 저장하지 않는다. raw 준비·첫 이동·키1 phase·MP 대기 중 어디가 실패했는지는 미확정이다. `c05-account1-b04-attempt1-diagnostic.txt`와 실패 Recorder73프레임을 보존하고 정상 disconnect/Playstop 후 진단만 추가했다. 현재 전투·HP0·재등장 통과가 아니며 계정2는 미시작이다. 실패 뒤 위치(39,39)/C04몬스터81/MP변화 없음은 특정 gate를 입증하지 않는다.

진단 helper 최종 SHA256 `BBDFA4A7B9D7CF9BB118B73139BFB5B154BF41FDFFD32D351A0A6AA8A55B1A76`를 직접 확인했다. **이 delta의 코드상 차단 문제 없음.** instance WaitFor의 begin/pass/timeout, 화면 접근 점/GraphicRaycaster hit 유무·수/HUD rect 포함 여부를 기록한다. 모든 label 호출은 정적 소스 문자열이고 상태는 숫자·불리언이다. 자격증명/모델 문자열/예외 원문·stack/패킷 body를 출력하지 않는다. 공유 C01 runner의 보안 정책은 변경하지 않았다.

리뷰어가 파일 내용을 메모리에서만 역변환해 새 using·진단 호출/함수 및 WaitFor 로깅을 제거했을 때 이전 helper `FC681760…`의 SHA256과 정확히 일치했다. 따라서 그 외 assertion·조건·예산·입력/월드 좌표 경로가 바뀌지 않았음을 확인했다. RaycastAll은 hit 조회이며 handler·선택·transform을 조작하지 않는다. 진단 추가 자체는 실패 원인을 고쳤다고 주장하지 않는다.

제품 Combat `270EFD4E…`/World `96C857D1…`/DLL `1D10D5AE…`도 직접 해시를 재확인해 유지됐다. 구현 담당은 수정본 recompile completed/failedfalse/errors[]·진단 assembly 로드·console11:05:50 errors0/warnings0·Play/compiling/dirtyfalse·Editor 반납을 보고했다. 리뷰어 실행 검증은 없다. **새 진단 helper의 실제 경로는 아직 미실행**이며 담당자가 영향 live 경로만 재실행한 결과로 원인을 분리한다. 이전29/29 및 B01 통과와 미완료 전투를 분리한다.

## 계정1 초기 HUD 위 이동 제거 delta

이후 진단 attempt2의 보존 로그 `c05-account1-live-attempt2-fail.txt`를 읽었다. B01과 독립 raw 준비는 통과했고, own(39,39)/HPMP10000/player1/monsters81 상태에서 접근점(397.5,362.7)의 GraphicRaycaster hit=true·moveTarget=false였다. 서버 좌표가 바뀌지 않은 채 progress12초 timeout으로 종료했다. 이 실행은 helper가 UI 위를 월드 이동점으로 클릭한 전제 오류이며 제품의 UI 입력 차단을 해제해야 한다는 근거가 아니다. 첫 gate 미확정 실패와 달리 이번 실패 위치는 확인됐고 전투 key 단계에는 도달하지 않았다. 반복 시도를 멈추고 정상 정리·Editor 반납 후 helper만 수정했다.

최종 helper SHA256 `F6BC07B43EC4716C07191EEED56088D5056DCF32A6CA1574FAED4B55835C1A08`를 직접 확인했다. **해당 한정 delta의 코드상 차단 문제 없음.** 계정1 초기에만 camera/own 조회와 첫 MoveAlong 호출을 stationary Ring 거리 로그로 대체했다. 변경 블록을 메모리에서 이전4줄로 복원했을 때 `BBDFA4A7…` 해시와 정확히 일치해 다른 변경이 없음을 확인했다.

원래 FindTypeZeroMonsterWithin7 및 생존 전제는 유지된다. Ring 실제 범위8 안에서 이미 시작할 수 있으므로 이 접근은 전투 검증에 필수 이동이 아니었다. 기본 공격의 특정 대상 명중을 주장하지 않는 기존 경계, 키1 phase/MP, Ring의 실제 피해 선택·고정 ID/HP0·최초 alive/실제 활성 Visual 검증은 그대로다. 정지 로그는 숫자 좌표/거리/ID만 남기며, 실제 Ring 피해가 없으면 기존 assertion이 계속 실패한다. Boss 접근 두 MoveAlong 호출과 함수는 변경하지 않았다. 제품 UI 차단이나 이동 기대를 느슨하게 바꾼 수정이 아니다.

구현 담당은 Unity compile completed/failedfalse/errors[]·assembly method loaded·Play/compile/dirtyfalse·console11:11:16 errors0/warnings0·Editor 반납 및 제품/Builder/Scene 해시 유지(Combat270E…/World96C8…/DLL1D10…/Builder2D79…/SceneE5F8…)를 보고했다. 리뷰어는 문서 외 수정/Editor/실행을 하지 않았다. 기존29 fixture 통과는 유효하고 **이 수정본의 실제 전투 경로는 아직 미실행**이다. 영향 helper 경로만 후속 실행 대상으로 남긴다.

## 실제 전투 결과와 이미 HP0인 계정1 확인 wrapper

그 후 attempt3의 보존 live 로그를 읽었다. 키1 phase/독립MP·vitals, Ring 실제 type0 HP1500→500 및 고정 ID64, 키3와 실제 버튼의 새 phase, ID64 HP0·visual 제거, 최초 alive Full/활성 visual15.26초(early=false)가 관찰됐다. 이는 F6BC07B4… helper로 담당자가 실행한 실제 전투 근거이며 리뷰어 실행이 아니다. 반면 계정1도 정상 몬스터 피격으로 예상 밖 HP0이 됐다는 Coordinator 보고가 있다. 원래 HP0 유도는 계정2만 계획했으므로 계정 보호가 미흡했던 이력을 남기며 이를 원래 계획 준수·승인으로 바꾸지 않는다. Coordinator는 해당 사실을 기록/Slack 공개했고 DB 복구를 하지 않았으며 계정2 추가 HP0 유도를 생략했다.

최종 HP0 wrapper delta helper SHA256 `0274B1BD574EB2ACCC21B75BE8DBFFB4C9469E41DB18770DF801E4C739B21EEC`를 직접 확인했다. **해당 한정 delta의 코드상 차단 문제 없음.** 기존 계정2 body를 두 실제 소비자가 사용하는 private OwnHpZeroAndDisconnect로 묶고 expected ID5011/5012만 허용한다. label은 fixed wrapper의 account1/account2 리터럴뿐이다. 계정1은 fresh 독립 raw HP0을 필수로 확인하고 alive라면 assertion으로 중단해85초 추가 피격 대기로 들어가지 않는다. 이미0/no-new-death-transition을 명시한다. 새 공격·HP 주입·회복·계정 로그인 우회는 없다.

같은 raw/UI vitals/CanAct=false/세 버튼 비활성·실 native key/보이는 지면 클릭·MP/좌표/새 phase 무변화·기존 disconnect와 finally 정리는 유지한다. 계정2 wrapper의5012 경로도 보존한다. 새 StartAccountOneHpZero와 결과 txt, 고정 `c05-hp0-account1.png` 한 개를 전용 캡처 whitelist에 추가했고 그 외 파일 삭제 범위를 늘리지 않았다. 이는 이미 관찰된0 상태의 입력 차단·종료 검증이며 새 death transition 실검증이 아니다.

Coordinator는 수정본 compile0/errors[]·wrapper assembly 로드·Play/compile/dirtyfalse·console11:17:40 errors0 및 기존 A31… B01 helper/제품/DLL/Builder/Scene 보존을 보고했다. **이 새 wrapper의 live 입력 검증은 아직 미실행**으로 유지한다. 이전29 fixture·실전투 성공 범위와 예기치 않은 계정1 HP0/보호 미흡·DB 복구 없음·계정2 계획 변경을 구분했다. 리뷰어는 문서 외 변경/Editor/실행을 하지 않았다.

## account2 접근의 자연 idle/투영 정착 delta

`c05-account2-boss-approach-attempt1-fail.txt`를 읽었다. 첫 접근 도중 moveTarget=true인 상태로 다음 화면점을 계산했고 요청(46.2,51) 뒤 서버(46.491,53.986)에서 progress timeout이 났다. GraphicRaycaster/HUD hit는 false이고 HPMP10000·aggro 키 미입력 상태였다. 이는 아직 끝나지 않은 이동·추적 카메라를 기준으로 다음 투영을 만든 helper 전제 문제와 일치하며 실제 HP0/aggro 통과 근거가 아니다. Coordinator는 계정2 실제 로그인/외곽(-4.75,-4.66)·Visual 대조·복귀 통과를 별도 보존하고 정상 정리했다.

최종 helper SHA256 `521F80BA4A22033FF1E399C5037B072C426ECFBFC42D9AA0428654BAA6B8673F`를 직접 확인했다. **이 한정 delta의 코드상 차단 문제 없음.** MoveAlong 진입·각 step 이후·최종 return 전에 moveTarget 자연 종료를 기다리고, 다음 투영 전에 PlayerFrame2개를 기다린다. helper는 field를 조회할 뿐 CancelMove/handler/Transform/서버 상태를 변경하지 않는다. progress0.75·최종거리1.8·최대12step/한 번5unit·각 대기12초 상한을 유지하고, 반복 중 대기는 method120초 deadline의 남은 시간으로 제한한다. 전제/진행 조건을 느슨하게 바꾼 수정이 아니다.

추가된 대기·프레임·deadline 제한만 메모리에서 제거했을 때 이전 `0274B1BD…` SHA256과 정확히 일치해 그 외 코드 변경이 없음을 확인했다. 기존 전투/HP0/raw/UI/Visual/정상 disconnect 및 두 계정 selector는 유지된다.

구현 담당은 compile completed/failedfalse/errors[]·신규 assembly 로드·console11:28:01 errors0/warnings0·Play/compile/dirtyfalse·Editor 반납 및 제품/아트 동결 보존(Combat270E…/World96C8…/DLL1D10…/Builder2D79…/SceneE5F8…)을 보고했다. 리뷰어는 실행하지 않았다. **새 helper live는 미실행**이며 영향받는 account2 로그인→접근→동일 연결 HP0 guard만 후속 대상이다. 완료된 계정1 전투/외곽 및 관련29 fixture를 근거 없이 반복할 필요는 없다.

## boss aggro 입력 한 곳의 Ring 전환

521F80BA…의 후속 실행 로그에서 접근·자연 idle·최종 위치(42.764,49.472) 검사는 통과했으나 basic 뒤 boss damage/aggro는 NOT OBSERVED로 남았다. Coordinator는 당시 boss(43.4,50)/MP9999·bossHP10000을 보고했다. ACTION의 특정 대상 명중을 보장하지 않는 기존 basic 계약과 구분하며, 이 결과를 aggro 통과로 바꾸지 않는다.

최종 helper SHA256 `AC7ACEE8969BA1C43514C204FB217FE35DD905E7F91E2254C7FE45ECAEE2A8B2`를 직접 확인했다. **해당 한정 delta의 코드상 차단 문제 없음.** BossAggro의 한 native key만 Digit1→Digit2로 바꾸고 고정 로그에 Ring key2를 명시했다. 이 키와 두 로그 문구를 메모리에서 이전 내용으로 복원했을 때 `521F80BA…` 해시와 정확히 일치해 다른 변경이 없음을 확인했다.

이미 실전투에서 확인한 기존 원형8 AOE 입력을 사용하는 것이며 boss 실제 HP 감소3초 조건/미관찰 분류·접근·HP0 경계·예산을 유지한다. 특정 basic 대상 명중을 강제하거나 공격 API/서버 상태를 주입하지 않는다. native Ring 뒤에도 HP 감소가 없으면 NOT OBSERVED로 남기며 그 자체를 서버 aggro 성공으로 추정하지 않는다.

담당자는 Unity compile0·신규 assembly 로드·console11:33:42 errors0/warnings0·Play/compile/dirtyfalse·Editor 반납 및 제품/Core/Scene 불변을 보고했다. 리뷰어는 실행하지 않았고 **AC7 수정본 live는 아직 미실행**이다. 이전 접근 정착 성공 및29 fixture/계정1 완료 범위를 구분한다.

## Coordinator 실제 검증 수용

리뷰 후 동결 제품을 유지하고 테스트 담당이 실제 배치를 완료했다. proper PlayMode29/29와 전투·실몬스터HP0/15.26초 재등장, 최종AC7 helper의 Ring boss피해·rawHP0 UI/nativeguard/normaldisconnect, V03 외곽PNG를 Coordinator가 근거와대조했다. HP0 phase 시작때이미0이므로 새로운사망전이는그phase미관찰이며 재접속HP보존원인도미확인이다. 리뷰어가실행했다는뜻이아니다. 제품/Scene최종SHA보존·cleanup확인, Captures/c05-batch-final.txt 참고.
