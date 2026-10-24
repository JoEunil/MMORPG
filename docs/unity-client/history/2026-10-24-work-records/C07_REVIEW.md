# C07 독립 코드 리뷰

2026-10-19. 결론: **코드상 차단 문제 없음**. 재현 가능한 P1/P2 결함이나 현재 요구사항에 불필요한 새 구조를 확인하지 못했다. 제품·테스트·Scene 수정, Editor 조작, 서버 전환, 테스트 실행은 수행하지 않았다. 이 결론은 실행 검증 통과를 뜻하지 않는다.

## 범위와 판단

- `CoreLib/LobbyZone.h`, `CoreLib/PacketWriter.cpp`, `CacheLib/Handler_Inventory.cpp`, `UnitTests/CoreLib/NonZoneThreadPool.cpp`의 HEAD 대비 diff와 소유권/요청/DB 조회의 주변 경로를 읽었다. 성공한 서버 캐릭터 조회 결과만 기존 shard→lobby 잠금 아래 세션에 연결하며, 0 ID·미인증·다른 Zone·중복·닫힌 세션을 거절한다. 같은 shard mutex 재진입이나 새 세션 생성은 없다. 응답 body 초기화가 요청 body를 덮기 전에 characterID를 확보하며, 실패/잘못된 count의 송신은 고정 81바이트의 0 초기화된 실패 응답으로 끝난다.
- ClientCore의 Config/Interfaces/Handler/Packet/NetworkService/ViewData/ViewModel을 검토했다. opcode20 요청과 opcode21의 고정 길이·magic·flags·status·count·슬롯 범위/중복 검증이 실제 계약과 맞는다. count 이후의 미사용 배열 항목을 노출하지 않는다. 단일 pending과 dispatcher 안의 수명 검사를 유지하며 실패/종료 시 목록을 지운다. 반환 목록은 복사이므로 UI가 원본을 변경할 수 없다.
- `C07InventoryScreen.cs`, `Editor/C07InventorySetup.cs`를 검토했다. 기존 모델/이벤트/Canvas를 재사용하며 서버 수치만 표시한다. 요청 중 중복 방지, 실패 후 수동 재조회, timeout 후 같은 연결 재요청 금지와 새 모델에서만 해제가 계약과 맞는다. 닫기만으로 outstanding을 취소하지 않는다. 새 Panel/Viewport Image가 기존 GraphicRaycaster 기반 필드 클릭 차단 경로에 참여한다. 입력 배치에서 실제 동작을 확인해야 한다.
- C07 wire/lifetime/component/native UI 테스트와 기존 C02/C04/C05/C06 fake network의 인터페이스 보완을 읽었다. 기존 assertion 완화는 없다. UI fixture의 `ClientCore.Services` using 추가 및 native 재열기에서 세 번째 실제 응답을 기다리는 최종 보완을 확인했다. fixture는 실제 입력/서버 성공의 증거로 분류하지 않는다.
- `Switch-R01LocalServer.ps1`의 C07 mode diff 및 종료·기동·복구 경로를 검토했다. 기존 mode 기본값을 보존하고 C07의 고정 R01 PID/시작 시각/이미지, 필수 SHA256, DLL/무접속/정상 종료 확인을 사용한다. child quit helper에도 같은 mode/hash를 전달한다. 실패 시 강제 종료하지 않으며 정상 종료·비어 있는 포트·저장 관찰이 확인된 경우에만 원본 후보로 복구한다.
- `Inspect-C07Inventory.cjs`는 지정 두 캐릭터의 행 존재/길이만 parameterized SELECT로 조회하며 READ ONLY transaction을 rollback한다. 비밀 값을 출력하지 않는다. DB 행 생성/변경은 없다.

Scene은 Coordinator가 보고한 블록 ID 비교(기존 Canvas GameObject/RectTransform만 변경, 삭제 없음, 신규 57블록)와 Setup의 생성 범위를 검토했다. Scene YAML 전체 재직렬화나 Grass 블록의 텍스트 이동 자체를 별도 아트 변경으로 판정하지 않았다. DLL/실행 서버/Scene이 검토 소스와 같은지는 Coordinator의 최종 빌드·동결 기록과 배치 증거로 확정해야 한다.

## 남은 실행 확인과 보고 한계

최종 컴파일 보완 delta도 읽었다. UI fixture의 잘못된 인스턴스 접근 `fixture.SetPrivate(...)` 3곳을 기존 outer static `SetPrivate(...)` 호출로 바꿨으며 assertion/시나리오는 동일하다. C07 운영 도구의 후보 경로는 `final/bin/MainServer.exe`와 일치한다. ignored `Builds/C07-Candidate/Isolated.targets`는 OutDir/IntDir를 격리하고, 실제 C++ 항목 기본 메타데이터인 `ItemDefinitionGroup/ClCompile/DebugInformationFormat=OldStyle`로 `/Z7`을 지정했다. 기존 PropertyGroup 값만으로 컴파일러 옵션이 바뀌었다고 보지 않는다. 이는 빌드 산출물 경로/디버그 정보 설정 변경으로 제품 로직을 바꾸지 않으며, 실제 빌드 성공 및 최종 DLL/실행 이미지 일치는 Coordinator의 빌드 결과로 확인해야 한다.

실행은 구현·리뷰 완료 후 Coordinator의 한 배치에서 수행한다. 서버 세션 귀속/실패 송신 회귀, Unity 관련 회귀, 실제 가방 열기→수동 재조회→닫기/재열기→정상 종료와 캡처 결과를 기록한다. 확인된 테스트 계정에는 inventory DB 행이 없으므로 실서버 status0을 정직하게 실패로 표시하는 것이 현재 live 기대 결과다. 성공/빈 성공/최대 10행의 결정적 fixture 통과를 실서버 인벤토리 성공으로 보고하면 안 된다.

wire에 요청 ID가 없으므로 timeout 뒤 재요청을 잠그는 제한은 의도한 안전 정책이다. 기존 socket/로그 경로 전체, 모든 DB 캐시 경합, 다른 해상도의 UI 가독성·스크롤, 실데이터 성공 목록과 재접속 후 성공 목록은 이 정적 리뷰로 검증됐다고 주장하지 않는다. 변경 묶음 완료 후 필요한 영향 범위만 확인하며, 전체 기능 테스트의 반복 실행을 요구하지 않는다.

## R01 정상 종료 시 중단 원인 추가 분석

전환 receipt `r01-switch-20261019-141012-671-12908.txt`와 보존된 before/stopped 로그를 독립 대조했다. 기존 6개 error 행에 **GQCS 실패995 8개와 AcceptEx 실패10038 8개**만 추가됐으며 모두 timestamp `1792386619527`로 system.log의 `IOCP CleanUp`과 같은 시각이다. 새 캐릭터 상태 저장 실패 로그는 없고 game.log의 전후 SHA256은 동일하다. READ ONLY before/post-stop의 두 캐릭터 `characters` 값도 같으며, 보존된 redo/backpressure WAL은 각각 0바이트다. system.log에는 IOCP worker 종료8개, cache MQ 종료3개, DB worker 종료4개 및 WAL manager 종료가 있다. 이 기록을 개별 SQL 커밋 영수증이나 전역 데이터 무손실 증명으로 확대하지 않는다.

변경되지 않은 `NetLibrary/IOCP.cpp`에서 CleanUp는 running을 false로 바꾸고 listen socket을 닫는다. 이미 GQCS 안에서 대기하던 worker는 취소 completion을 받은 뒤 running을 다시 확인하지 않고 ACCEPT 실패 분기에서 무조건 PostAccept를 호출한다. 닫힌 listen socket에 AcceptEx를 다시 등록하다 실패/fatal 로그를 쓰는 경로가 이번 종료 시각·코드·개수와 일치한다. 따라서 관찰한 추가 로그는 기존 정상 종료의 accept 취소/재등록 처리로 설명되며, C07 후보 기동 실패나 이번 인벤토리 변경으로 인한 캐릭터 저장 실패의 증거가 아니다. 제품 IOCP 수정은 이번 변경에 추가하지 않는다.

기존 전환 도구가 error.log 증가 guard에서 중단한 결과는 보존한다. 그 guard를 완화하지 않는다. 이후 root가 별도로 준비한 `Start-R01Candidate.ps1`의 start-only C07 delta는 고정 `final/bin` 경로와 필수 64자리 SHA256, 기존 R01 승인 ID 혼용 거절, 기존 R01 기본 gate 보존으로 확인했다. 기존 무프로세스/빈 포트/TEST 미설정/WAL 보존 및 hash/READ ONLY 안정성/readiness/error0 검사를 바꾸지 않아 코드상 차단 문제는 없다. 종료 증거와 현재 DB 안정성·빈 WAL을 다시 확인한 뒤 현재 허용된 정상 로컬 기동 범위로 다룬다. 리뷰어는 종료·기동·DB 조회를 실행하지 않았으며 보존된 실행 증거와 소스만 읽었다.

## 실제 입력 도우미 보완 리뷰

실입력 A/B 로그 `c07-live-c07-20261019-a.txt`와 `-b.txt`는 모두 첫 TypeText 단계 AssertionException으로 실패했다. 이 실패 원문은 보존하며 로그인·인벤토리 실검증 통과로 세지 않는다. Coordinator는 synthetic mouse 큐가 실제 player frame 대기만으로 반영되지 않는 상황을 probe로 확인하고, 입력 설정을 원복한 뒤 공유 `C02EmptyListUiTests.Click`에서 각 QueueStateEvent 직후 `InputSystem.Update()` 3곳만 추가했다.

해당 diff와 설치된 Pipeline 패키지 `RuntimeInputCommand.cs::SimulatePointer`의 QueuePointer→InputSystem.Update 경로를 직접 대조했다. 다만 이후 C 입력 재검증도 TypeText 단계에서 실패했으므로 단순 flush를 최종 해결로 보지 않는다. C의 실패 원문도 보존한다. 제품/서버/DLL이 같다면 이미 완료한 관련 단위 회귀를 반복하지 않고 실패한 실제 입력 배치만 다시 검증한다.

최종 도우미 delta는 직접 flush 3곳을 제거하고 `QueuePlayerMouse` coroutine에서 공개 `InputSystem.onBeforeUpdate`를 구독해 `InputState.currentUpdateType == Dynamic`인 경우에만 한 번 QueueStateEvent를 보낸다. 3초 제한/실제 player frame 확인/finally 구독 해제가 있고 내부 UI handler를 호출하지 않으며 assertion은 유지한다. 설치 InputManager 소스에서 onBeforeUpdate listener는 해당 update의 state buffer 전환과 InputUpdate.OnBeforeUpdate 뒤에 호출되므로 이 Dynamic gate가 의도한 player update를 구별함을 확인했다. Editor.update에서 무인자 InputSystem.Update를 호출하는 앞선 수정과 다르게 실제 Dynamic update 안에서 이벤트를 큐에 넣는다. 코드상 차단 문제는 없지만 결과 성공을 보장하는 정적 결론은 아니다. 같은 원인 보완이 다시 실패하면 반복하지 않고 진단·실패를 인계한다. 이 리뷰 시점의 최종 실입력 성공 결과는 아직 확인하지 않았다.

이후 Coordinator는 합성 마우스 보완을 최종 후보에서 모두 제외하고 공유 C02 helper를 HEAD 그대로 되돌렸다(diff0 확인). C07 live helper는 Windows 실제 마우스 클릭을 수동 관찰하는 작은 harness로 교체했다. 이미 실제 클릭으로 선택된 TMP 필드만 native GameView 문자 이벤트를 허용하고 입력값 일치를 비밀 값이 노출되지 않는 bool assertion으로 확인한다. DPAPI account는 finally에서 정리한다. 실제 5011의 InWorld/Full 모델에만 관찰자를 연결하고 세 번의 서버 status0·빈 목록·위치 불변·닫기·정상 disconnect를 검증한다. 이 교체 delta에도 코드상 차단 문제는 없다. Capture는 비동기이므로 Coordinator가 실제 PNG 생성 완료·내용을 별도로 확인해야 하며 입력 실패 시에도 관찰자/클라이언트/Play 정리를 수행해야 한다. 앞선 합성 입력 실패와 실제 마우스 시나리오 결과는 구분한다. 제품 및 기존 회귀 코드가 같아 관련 단위 회귀의 반복을 요구하지 않는다. 실제 마우스 최종 실행 결과는 이 정적 리뷰 시점에 아직 미확인이다.
