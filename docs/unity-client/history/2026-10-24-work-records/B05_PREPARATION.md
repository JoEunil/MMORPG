# B05 준비 — R01 저장 관찰 + V04 월드 입력

최신 실행 완료(2026-10-19): root가 helper finalFAF29C…로 컴파일 통과 후 한 배치를 실행했고 phase1/READ ONLY/phase2 모두 PASS. 실제 첫 raw Full 재접속 위치·HP·MP 일치와 외곽 이동·캡처를 확인했다. `UnityClient/Captures/b05-batch-final.txt`, `B05_REVIEW.md` 참조. 아래 준비/미실행 표현은 작성 시점의 이력이다.

상태: B05 helper 저작 완료, root 코드 리뷰·컴파일 대기. helper 작성 중 Unity Play·native 입력은 실행하지 않았다. 후보 서버 readiness와 시작은 Coordinator가 별도로 확인했다.

## 검증 카드

- **목표:** R01 정상 종료 뒤 추가 게임 접속 없이 캐릭터 저장을 관찰하고, 재접속 첫 Full과 대조한다. 같은 로그인 흐름에서 V04 길·외곽 이동, 시야, HUD도 확인한다.
- **대상:** R01 승인된 candidate 서버로의 정상 전환 및 그 서버가 준비 완료된 뒤, 동결된 C05/V04 Unity 변경과 지정 테스트 계정.
- **포함:** fresh Full 기준값 확인 → native 길/외곽 이동 및 기존 HUD 관찰 → 정상 연결 종료 → 추가 접속 없이 최대 10초 저장/로그 관찰 → 허용된 READ ONLY 조회 → 재로그인/ENTER 첫 Full 대조 및 캡처.
- **제외:** 추가 HP0 유도, HP/DB 상태 주입·복구, 직접 SQL 쓰기, 계정 변경, 추가 서버나 DB/로그인 서비스 재시작, 강제 종료. candidate 전환 또는 검증이 실패하면 B05 실입력은 시작하지 않는다.
- **기대 결과:** 끊긴 연결의 캐릭터 변경 저장 요청을 추가 접속 없이 관찰하고, 재접속 첫 raw Full 및 READ ONLY 행의 HP/MP/좌표와 맞춰 기록한다. 월드 입력은 서버 확정 위치와 시야/HUD가 일치해야 한다. 저장 요청·DB 행·재접속 값이 다르면 실패 또는 미확정으로 기록한다. 현재 B05는 MP를 이동 전후 값으로 비교하지만 스킬 입력 1회의 MP 변화를 만들지는 않는다.
- **비용/상한:** 실제 로그인 120초, 이동/시야 확인 120초, 무접속 저장 관찰 최대 10초, 재접속/첫 Full 120초, 캡처 1회. 동일 원인 수정 2회 안에 해결되지 않으면 Coordinator에게 넘긴다.
- **종료 조건:** 지정 관찰과 대조가 끝나고 disconnect/Play 종료/입력 정리·Recorder 종료가 확인되면 끝낸다. 실패, 관찰 누락, 환경 차단은 성공으로 바꾸지 않는다.

## C05 실입력 helper 재사용 판단

- 기존 실입력 helper는 [`UnityClient/Assets/Tests/C02/C05LiveBatchUiTests.cs`](../../../../UnityClient/Assets/Tests/C02/C05LiveBatchUiTests.cs)다. 최종 동결본 SHA256은 `AC7ACEE8969BA1C43514C204FB217FE35DD905E7F91E2254C7FE45ECAEE2A8B2`.
- 실제 실행 진입점은 `C05LiveBatchUiTests.StartAccountOneCombat()`, `StartAccountTwoLogin()`, `StartAccountTwoBossAggro()`, `StartAccountTwoHpZero()`, `StartAccountOneHpZero()`다. Unity가 Play 중일 때 해당 static 진입점을 실행하면 공통 `C01.Tests.C01EditorInputRun.Start(IEnumerator, outputPath, passMessage)`가 Editor update loop에서 native InputSystem 키/마우스 동작을 진행하고 `UnityClient/Captures/c05-*.txt`에 결과를 남긴다. 이들은 테스트 method/menu가 아니라 Play 중 호출하는 helper 진입점이다. 이번 준비 단계에서는 호출하지 않았다.
- B05에서 재사용하는 코드: 기존 C02 `RealCharacterSelectionAndEntry_AndCapture`의 실제 UI 로그인/목록/선택/ENTER와 C01 `C01EditorInputRun` Editor-update runner를 그대로 호출한다. 별도 raw observer는 scene 로드 직후 `ClientChanged`에 붙어 Zone 내부 ID 및 해당 캐릭터의 **첫 raw Full**을 수집한다. C05의 native `ClickAt`, 화면 투영·시야 검사, `RecordApproachClick` raycast/HUD 겹침 진단 및 `WaitFor`를 재사용하고, B05 자체 bounded loop가 HP0 이벤트를 매 클릭/서버 진행 대기 뒤 검사해 다음 클릭을 중단한다. 마지막 Visual 대조는 보간 settling 뒤 0.02-unit 허용오차다.
- C02 진입 helper는 세 개 고정 산출물(`c02-live-character-list.png`, `c02-live-entry.png`, `c02-live-entry-ui.txt`)을 덮어쓸 수 있다. B05는 실행 전 존재하는 파일을 run/phase 고유 이름으로 CreateNew 복사해 보존한다. B05의 PNG/TXT/baseline도 기존 파일을 덮어쓰지 않는다.
- `UnityClient/Tools/C04Peer/Program.cs`의 별도 ClientCore 연결은 unity_ai_02 캐릭터 목록/AUTH/ENTER/Zone/본인 Full까지만 확인한다. native Unity 입력이나 실제 UI/HUD를 대체하지 않는다.
- `UnityClient/Tools/Inspect-R01Persistence.cjs`는 캐릭터 5011/5012의 제한된 READ ONLY 스냅샷에 맞춘다. R01 운영 점검용이며 테스트 도중 호출하도록 만든 helper는 아니다. 승인된 운영 전환 절차의 snapshot/receipt를 사용하고 범위 확장이나 SQL 쓰기를 하지 않는다.
- `Switch-R01LocalServer.ps1`는 정상 종료·candidate 기동·조건부 복구의 운영 도구다. B05 테스트 helper로 실행하지 않는다. 승인 R01-DEPLOY-04/r1 범위의 운영 판단과 재개는 Coordinator가 맡으며, 이 문서 작업에서는 재실행하지 않았다.
- 새 `UnityClient/Assets/Tests/C02/B05LiveBatchUiTests.cs`는 `StartPhaseOne()` 및 `StartPhaseTwo(runId)`를 제공한다. phase1은 native UI 로그인/선택/ENTER의 첫 raw own Full을 저장하고 HP>0일 때만 C05 visible-point native 클릭 이동으로 (-4,-4) 외곽에 접근, HUD 포함 PNG, 정상 UI disconnect와 10초 무접속 관찰을 한다. fresh/이동 중 HP0이면 추가 이동 없이 정상 disconnect 경로를 택한다. 이동 후 HP/MP/좌표를 baseline으로 남긴다.
- Coordinator가 phase1 완료 후 baseline/서버 로그와 허용된 READ ONLY snapshot을 대조한 뒤 `StartPhaseTwo(runId)`를 실행한다. phase2는 동일 계정으로 native UI 재접속/ENTER를 거쳐 raw observer가 잡은 첫 own Full과 phase1 최종 HP/MP/좌표를 비교하고 PNG·결과 txt를 쓴 뒤 정상 disconnect한다. Helper는 DB 저장 성공을 자체 판정하지 않고 Coordinator의 READ ONLY 증거를 요구한다. 스킬 1회/MP 감소 경로는 별도 검증하지 않으므로 MP 영속성은 현재 값 비교에 한정된다.
- `StartPhaseOne`/`StartPhaseTwo`는 Play 중 Unity CLI C# 실행으로 부르는 static entry point이며 공통 Editor update runner가 각 phase를 최대 120초로 제한한다. 두 phase 사이에는 Unity 게임 모델이 없어야 한다. Helper는 테스트 코드만 수정하며 제품/서버/DB/UI handler를 직접 호출하거나 패킷을 주입하지 않는다. 현재 저작 버전은 root review/compile 대기이며 실제 Play/native 입력은 미실행이다.

## 현재 차단과 증거 한계

- V04 입력 재개는 이미 수용됐으며 다시 요청할 필요가 없다. Coordinator가 후보 서버 readiness/start를 별도로 확인했으며 B05 입력은 helper review/compile 뒤 root가 실행한다.
- 이전 tester 세션의 프로세스 조회는 접근 제한으로 결론을 내지 못했다. 현재 후보 서버 readiness와 실행 상태는 Coordinator가 별도 확인한 최신 상태를 따른다.
- Unity 입력/Play/컴파일은 미실행이며 코드 리뷰와 root의 compile 확인을 기다린다. 신규 helper 및 전용 meta만 작성했고 다른 Assets/C05/제품/Scene/ProjectSettings 파일은 수정하지 않았다.
