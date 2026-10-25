# C03 독립 코드 리뷰

2026-10-18 KST. 최종 판정: **코드상 차단 문제 없음**. 최초 네 지적은 아래 최종 delta에서 해소됐다. C03_TASK/CONTRACT/SPEC, AGENTS.md와 reviewer 역할 지침을 읽고 고정 코드를 검토했다. 리뷰어 소유는 이 문서뿐이며 제품/서버/DB/Scene/Art/Editor/테스트를 수정하거나 실행하지 않았다. Unity 입력 보류를 유지한다.

## 최초 검토 기준과 지적

최초 C02WorldScreen SHA256 `7EA7D3E7DD07261EF514572D8FE96798321A6D5B287936AD2E2C26D79CF179F9`, ViewModel `ABF46F52AE399C3C9FA9859EE6C7E7D7EB31D09764D35256611220A7AD03DF47` 기준이다. 기존 미커밋 C01/C02/서버 승인 변경을 C03 신규 변경으로 오인하지 않았다.

1. **P2 — C02WorldScreen.TryMoveCandidate.** absY>=absX 분기는 대각 동률을 Y로 보낸다. C03_SPEC는 동률 X를 명시했다. 예를 들어 (10,10)→(100,100)은 dir3이어야 한다. 최소 수정은 비교를 strict >로 바꾸고 해당 합성 기대를 계약의 X축 선택으로 고정하는 것이다.
2. **P2 — C02WorldScreen.ShowPosition.** V02 Hero는 local -Z를 향하지만 최초 yaw는 dir0=0/dir1=180/dir2=270/dir3=90이라 서버 이동 방향과 모델 facing이 네 방향 모두180도 반대다. 최소 mapping은 dir0=180/dir1=0/dir2=90/dir3=270이며 Scene/Art 변경 없이 runtime에서 교정한다.
3. **P2 — C02WorldScreen.Update UI 클릭 차단.** GraphicRaycaster hit만 거부하지만 기존 WorldHud Image/WorldStatusText는 raycastTarget=false다. HUD 패널이나 좌표 글자 위 클릭도 지면으로 통과해 이동 목표를 만든다. UI 패널 클릭은 이동하지 않는다는 수용 조건을 위반한다. 최소 수정은 활성 worldHud RectTransform 내부 screen point를 거부하는 등 해당 기존 UI 영역을 검사하는 것이다.
4. **P1 — ViewModel.SendUpdate/TCPSocket.ReceiveLoopAsync 락 역순.** 최초 SendUpdate는 _sessionGate를 잡고 Action→TCPSocket.Send의 _gate를 기다린다. 수신 loop는 _gate를 잡은 채 Handler→EnterReceived의 _sessionGate를 기다린다. ACTION과 늦은/중복 Enter 응답이 겹치면 교착 가능하며 이후 Dispose/종료도 막힌다. 최소 수정은 상태/취소 검사와 snapshot만 _sessionGate 아래에서 하고 실제 Action은 밖에서 호출하는 것이다. 이미 ClientTick이 가져간 최대 한 segment를 소급 취소할 수 없다는 계약과 함께 다룬다.

총괄은 위 네 건을 수용해 구현 담당에게 좁은 수정을 배정했다. 위 내용은 최초 고정본 지적이며 최종 해소 여부와 기준은 아래와 같다.

## 나머지 검토 근거

- 기존 UpdateMove/ViewData lock/ClientTick/NetworkService 경계를 재사용한다. 최초 skill255, dir/NaN/Infinity 입력 거부, pending CancelMove, Action 예외 전파와 dispatcher 연결 실패 경로는 현재 필요에 맞는 최소 변경이다. C01에는 Data property1개, IViewDataUI에는 CancelMove1개, runtime asmdef에는 기존 InputSystem 참조1개만 추가한다.
- 이동 후보는 최신 ownFull 좌표에서 최대1단위, 시간 간격50ms, 긴 프레임 catch-up 없이 만든다. 목표3초 무진전/15초 총 deadline·도착·교체·focus 상실·상태 변경·연결 해제·disable 정리가 있다. candidate가 아닌 서버 수신값만 Transform/HUD에 반영한다.
- Full/Delta의 본인 dir/유한 좌표/Zone 범위를 확인하고 Delta packet의 x/y를 모두 합친 뒤 한 번 표시한다. 다른 ID의 잘못된 값은 본인 상태에 반영하지 않는다. sequence/ACK를 새로 가정하거나 ActionResult를 이동 승인으로 쓰지 않는다.
- 기존 C02 state fixture의 성공 조건 Zone5/y=-6.5는 실제 계약상 불가능해 Zone1/y=-4.5로 조정했고, 기존 ID/순서/좌표/월드 gate assertion을 유지한다. invalid Zone/좌표/dir 거부 조건을 별도로 추가했다. 실제 UI helper는 변경하지 않았다.
- C03 순수 fixture는 ACTION18/offset/255, 마지막 값, invalid 입력/취소, 실패 dispatcher/추가 송신 억제, dispose, 후보 거리/방향/bounds를 다룬다. 최초 internal PacketBuilder 직접 호출의 컴파일 실패는 기록됐고 기존 reflection 경계로 변경한 delta를 읽었다. 합성 값만 assertion하며 비밀번호·토큰 출력은 없다. 실제 입력/서버 통과를 주장하는 fixture가 아니다.
- ShowPosition의 fieldPlane은 Renderer가 숨겨진 기존 collider reference다. 실제 V02 texture 지면은 고정 C02World/V01Outpost/TexturedGround 아래에 있어 이 참조 이동으로 텍스처를 따라 움직이지 않는다. 이동하는 기존 클릭 collider와 고정 시각 장식의 범위 차이는 서버 지형 충돌/새 맵 규칙과 구분한다. 시각 지면을 재배치하는 별도 수정은 요구하지 않았다.

실행 상태: 총괄은 최초 수정 전 Unity 컴파일0/순수5개 통과를 확인했다고 전달했다. 이는 위 수정 뒤 최종 기준의 통과가 아니며 리뷰어는 실행하지 않았다. 실제 클릭 이동·UI/텍스트 focus·속도/수명·새 AUTH 서버 본인 회귀는 INPUT_TEST_BATCH의 사용자 재개 후 검증 대상이다. 새 wire/서버/Scene/Art 변경은 검토 범위에 없다.

## 최종 delta와 해시

네 지적 모두 코드상 해소됐으며 추가 차단 결함을 발견하지 못했다. SendUpdate는 조건/GetActionState snapshot만 _sessionGate 아래에서 수행하고, 실제 I/O 전에 취소/실패를 다시 확인한 뒤 lock 밖에서 Action을 호출한다. session/socket 락 역순을 제거하며 이미 ClientTick에 전달된 최대 한 segment 취소는 보장하지 않는다. 후보 동률은 strict > 비교로 X를 택한다. ServerFacing의 네 yaw와 Quaternion×Vector3.back 회귀가 authored local -Z와 실제 이동 방향을 맞춘다. 활성 worldHud의 RectTransform 안 클릭을 별도 거부해 non-raycast 패널도 이동으로 통과하지 않는다.

최종 테스트 delta는 internal PacketBuilder reflection 접근, 계약상 tieX 기대 정정, 실질적 네 방향 facing assertion 추가다. 기존 거리/offset/취소/오류 조건을 삭제하거나 완화하지 않았으며 RunPureChecks는6개 assertion 경로를 실제 호출한 뒤에만6/6 문자열을 반환한다.

아래 SHA256은 리뷰어가 각 현재 파일에서 직접 읽었다.

| 파일 | 최종 SHA256 |
| --- | --- |
| ClientCore/Interfaces.cs | `A048FF6D5476293FF585D210A550959DE536D10A353E3D442318EC64F2BD93D2` |
| ClientCore/ViewData.cs | `335EF1BF015887AAA574653B2B742712DE18C274ED17C497471BCFCE34355CE9` |
| ClientCore/ViewModel.cs | `3C05890C3B6B98556B09A8C88713211247FA2510D172A81A7A3DAA111F27C631` |
| ClientCore/Services/NetworkService.cs | `EB5581C84F200DD1C912217844D3D6741073E3F7058BAAAEF7462225F0624DBB` |
| Assets/C01/C01LoginScreen.cs | `B4732E655CAF345A65600CEBACA6FB3D4D8493300E297596D7DC80A37E880DAB` |
| Assets/C02/C02WorldScreen.cs | `E6710A42FE61A09273DF511821333A00C23F0AC012E3450C2F3BA61AF9C1D682` |
| Assets/C02/C02.Runtime.asmdef | `9DDBF08DB9DEC05EDE25C6AD056A97A35BFA2423D9AE4049A655932EF6FF485C` |
| Assets/Tests/C02/C02EntryStateFixtureTests.cs | `FC8D65385307D3CA4F115423B157B988BAA0762C202F36CB5198E89A45885D56` |
| Assets/Tests/C02/C03MovementTests.cs | `71454FABAB2B30E25AE66AED8939EFE753C747306D70EADCB2DFC891155F65D4` |
| Assets/Tests/C02/C03MovementTests.cs.meta | `6D07DA4CCAE55BE2E2F68B7CBB1D9D26BA74644B8DF1D7A81B859E81C2A374C4` |
| Assets/Plugins/ClientCore/ClientCore.dll | `0945C02A2A4C306DC1F521ED746C8356A4937CF1A0C861D48ACAC66CD43D1DFA` |
| ClientCore/bin/Release/netstandard2.0/ClientCore.dll | `0945C02A2A4C306DC1F521ED746C8356A4937CF1A0C861D48ACAC66CD43D1DFA` |

V02 Scene `786860...`와 builder `1FE57B...`도 보존됐다. 원본 DLL과 Unity DLL이 일치하며 별도 ClientCore source 사본은 만들지 않았다. 최종 DLL은 기존 Build-ClientCore.ps1 exit0/오류0, 기존 NU1900 audit 네트워크 조회 경고2개라는 총괄 인계를 받았다. 과거 C02 BuildProvenance.txt는 C03 source/DLL 출처로 재해석하지 않는다.

최종 검증 인계: `Captures/c03-implementation.txt` SHA256 `A9E3117777AAAE1CCF12889D6284CEB8FBF5D895E736C6E69DFE4624633303B9`를 읽고 위11개 source/Unity DLL/test/meta 해시와 대조해 일치함을 확인했다. 해당 기록과 총괄 전달은 최종 Unity recompile_status completed/failed=false/errors=[]/compilationFailed=false, Edit Mode 직접 RunPureChecks6/6 통과, playing=false/paused=false/compiling=false/scriptCompilationFailed=false를 명시한다. 리뷰어가 재실행한 결과는 아니다.

직접 호출6/6은 NUnitRunner/PlayMode/실제 UI/서버 통과가 아니다. 첫 internal 접근 컴파일 실패와 최초5개 통과 이력은 별도 시도로 보존한다. 수정된 C02 scene/state fixture는 이번 게이트에서 미실행이고 입력/Play/서버 이동/캡처도 미실행이며 B02 배치로 유지한다. 코드 리뷰 통과가 이 실행 검증 통과나 사용자 입력 재개를 뜻하지 않는다.
