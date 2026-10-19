# R01-CLOSE-03/r1 실제 구현 독립 리뷰

2026-10-18. **동결 소스의 승인 범위에는 코드상 차단 문제 없음.** 후속으로 담당자의 격리 빌드·회귀 타깃10/관련19 통과 기록과 고정 산출물 일치를 확인했다. 실제 서버 배포·HP 영속 검증 판정은 아니다. 리뷰어는 이 문서만 작성하며 제품·테스트·빌드·Unity·DB·서비스를 변경하거나 실행하지 않는다.

## 범위와 승인 인계

Coordinator는 최신 사용자 채팅의 정확한 후보 구현·회귀 직접 지시를 승인으로 수용했다. 이번 R01-CLOSE-03 Slack 지정 사람 원문/회신 ID는 확인되지 않았으므로 Slack 승인 확인이나 자동 재개 왕복을 주장하지 않는다. 이전 HANDOFF/요청서의 승인 대기 문구는 당시 이력이다.

허용 범위는 StateManager.h·NonZoneHandler.cpp·LobbyZone.h의 제품 후보 patch `E3D832EA214D27FC0985816E8E91E527E38BDABD47FB637BFB1ED687ADCA7D7B`, 기존 UnitTests/CoreLib/NonZoneThreadPool.cpp 확장, 격리 UnitTests/참조 라이브러리 및 MainServer build-only다. 기존 reset·승인된 fixture friend·다른 작업자의 변경을 보존한다. MainServer 후보 실행/배포·현재 실행 이미지 덮어쓰기·운영·DB·Unity는 제외한다.

동결 후 실제 patch 정합성, 최초 callback의 UID/인증 상태 고정, 최초 실패와 duplicate 응답 보존, pending의 미인증 Net filter 경계, shard→lobby 검사/삽입 원자성·정상 rollback, 새 seam 유무와 fixture의 실제 경로/양순서 기대를 확인한다. 별도 tester가 리뷰한 고정 exe를 실행하며 리뷰어는 직접 실행하지 않는다.

정상 transport 종료의 두 경합에 한정한다. 직접 Core Disconnect 후 같은 transport 재AUTH 세대, 일반 Zone 동시성·SQL 완료 순서·서비스 종료 callback 수명까지 해결됐다고 분류하지 않는다. 앞선 추가 회귀의 기존3 pass/late2 fail 원문은 보존한다.

## 최종 동결 기준·제품 검토

다음 네 SHA256을 직접 계산하여 구현자 동결과 일치함을 확인했다.

| 파일 | SHA256 |
| --- | --- |
| CoreLib/StateManager.h | `7189CF42542E7A68A661203CB2CBEC1A25C725D77FC676A00CE7DC18E90C9F1F` |
| CoreLib/NonZoneHandler.cpp | `3B9A545D7D2B5044C660FF598E57C96A2E24952E6568F978F5A4E1AC907A51A7` |
| CoreLib/LobbyZone.h | `33476719BC72EF059EC0BD4F56DF53B40443282678D1F25F8ED5A7019E5E6B6B` |
| UnitTests/CoreLib/NonZoneThreadPool.cpp | `0D50C676E439B70F2D40E5ADE942855B95225A9E366BAAC8EDF7A2670C72F5E1` |

제품 동작은 승인 patch와 일치한다. BeginAuth는 PacketView를 보유하는 CheckSession에서 외부 auth 호출 전에 try_emplace하며 기존 entry를 초기화하지 않는다. pending UID는 미확정이고, CompleteAuth가 같은 shard lock 아래 존재하는 entry의 첫 callback UID/인증 상태만 확정한다. 종료로 entry가 없으면 false로 끝나 재삽입/응답하지 않는다. entry가 이미 완료됐으면 true를 반환하여 기존 실패·duplicate callback의 응답 status를 유지하고 상태는 덮어쓰지 않는다. authCompleted는 pending과 완료된 실패를 구분하므로 현재 필요한 별도 bool이며 UID0 sentinel을 만들지 않는다.

정상 transport 종료는 마지막 PacketView 반환 뒤 통지되므로 pending 등록과 Disconnect의 순서 근거가 유지된다. 상태 완료 lock 안에 외부 auth/송신을 넣지 않았고, callback 완료 뒤의 늦은 송신을 상태 재삽입과 혼동하지 않는다. pending HealthCheck에는 AUTHENTICATED가 없으며 NetPacketFilter의 기존 AUTH/PONG 분기는 그대로다. Net 필터 자체를 fixture가 실제로 실행한 것으로 주장하지 않는다.

LobbyZone::ImmigrateChar는 session shard→lobby 순으로 잠근 채 존재/인증/zone0을 확인하고 삽입한다. 검사 후 삽입 사이의 TOCTOU나 동일 shard의 SetZoneID 재획득이 없다. Disconnect/CleanUp과 순서가 같고 MQ/ZoneChange의 기존 호출 경계를 유지한다. 정상 Zone 진입 실패는 SetZoneID 전에 반환하므로 인증 zone0 rollback이 허용되며, 삭제된 entry는 rollback도 거부된다. 새 제품 friend는 승인된 LobbyZone 내부 접근 한 줄이며 새 테스트 seam/API/queue는 추가하지 않았다.

기존 reset CF81EF29… 및 NonZoneThreadPool.h 95358A6C…/NonZoneHandler.h A92F8EAE…/PacketWriter.h C47D0161…/MessageQueueHandler.h 6D6C007A…/프로젝트 26BB90C7…를 재대조하여 보존을 확인했다. LobbyZone/StateManager의 기존 fixture friend도 유지된다. 현재 BaseLib/NetLibrary 소스의 새 diff는 없고, 관계없는 제품 리팩터링을 찾지 못했다.

## fixture 정확성

기존 다섯 테스트의 다음 enqueue 없는 반환·stop/drain·복사 요청·late AUTH/ENTER 후 Core/lobby 부재 기대를 유지한다. 두 late 테스트에는 거부된 callback/로드 응답이 새 packet을 송신하지 않는 기대가 추가돼 강화됐다. 테스트를 통과시키기 위한 기대값 완화는 없다.

추가 다섯 테스트는 실제 WorkFunc/NonZoneHandler/저장 callback과 실제 writer 응답을 사용하여 다음을 검사한다.

- pending의 UID0/미인증 health mask, 최초 성공 뒤 다른 UID·실패/성공 callback의 상태 불변과 각 응답 status 보존.
- 요청 순서와 다른 최초 실패 callback으로 첫 완료 UID/미인증을 확정한 뒤 나중 성공 callback도 상태를 덮어쓰지 않으며 실패·성공 응답은 그대로 전달됨.
- UID0의 최초 성공도 완료 상태로 취급하여 나중 UID가 이를 덮어쓰지 않음.
- 실제 ENTER load 완료→lobby 존재→실제 Disconnect의 반대 순서, 중복 lobby 거부·Emigrate 후 zone0 재삽입·종료 후 재삽입 거부의 component 경계.
- pending/완료된 실패/zone≠0의 lobby 삽입 거부. zone guard만 public SetZoneID로 준비하는 component 테스트임을 명시함.

기존 close→late 완료 두 테스트와 새 정상 완료→close 테스트로 양순서를 구분한다. callback은 각 보관 요청당 한 번만 호출하고, 첫 완료 순서와 요청 순서가 다른 사례를 포함한다. worker join/조건변수·기존 2초 한도로 순서를 정하며 임의 sleep·생존 선조회·제품 상태를 직접 성공시키는 주입은 없다. 실제 ZoneChange/ZoneState 진입 실패 전체나 모든 동시 interleaving을 실행하는 fixture라고 주장하지 않는다.

TEST_F 파생 body의 private 직접 접근은 없고 필요한 내부 접근은 승인된 fixture 본체에 있다. callback 데이터/로드 Message는 소유 복사되며 worker를 종료한 뒤 storage/의존 객체가 해제된다. 비밀 파일·실계정 token을 읽거나 출력하지 않는다. 새 기록은 합성 패킷의 실제 AUTH_RESPONSE status 확인에 한정한다.

## 빌드·실행 인계와 제한

현재 구현자가 격리 UnitTests/참조 library/MainServer build-only를 진행 중이며 최종 artifact hash/성공 결과는 이 판정에 없다. source hash가 위 기준과 같고 빌드가 성공한 고정 exe를 Coordinator/tester가 대조한 뒤 타깃10개와 관련 Net19를 정한 예산으로 실행한다. 리뷰어는 빌드나 테스트를 실행하지 않았다. 이후 소스 수정이 생기면 영향 delta를 다시 검토한다.

기존 `x64/Release/MainServer.exe`는 읽기 hash 확인 시 `C0F689BD0E58D3A20282143D5DD5C0C15514F42F192A6FF70568C2DDF312E825` 그대로다. 후보 build-only는 별도 OutDir/프로젝트별 IntDir에서 이루어져야 하며 실제 서버 실행·배포·정상/강제 종료·DB 복구는 이 리뷰로 허용되지 않는다. MainServer의 기존 post-build가 $(OutDir)로 DLL을 복사할 뿐 실행하지 않는다는 이전 검토와 정합한다.

구현 정적 판정의 통과를 일반 Core 재AUTH 세대·Zone 락 역순·SQL 저장 순서·서비스 종료 callback 수명 또는 실서버 HP 보존 통과로 확대하지 않는다. 정상 close의 두 경합도 실제 새 회귀 결과를 받아야 검증 상태를 갱신한다. 별도 운영안은 당시 잔존 세션/저장·이미지·복구 근거를 갖추어 분석·리뷰·승인해야 한다. 리뷰어는 추가 실행 없이 역할을 반납한다.

## 담당자 실제 실행 결과 수용 — 후속 갱신

`UnityClient/Captures/r01-close-candidate-regression.txt`를 읽고 위 네 소스 hash와 고정 UnitTests.exe `5504D57C743A78A174C5B5D2C296E352CBE4A3E6B68A45692D2AF9749A94B08F`, build-only MainServer.exe `01C6261AB5C3448E2BA5F336CE70FB20E4DBA4B9ED47AD70002F22C88594D108`를 직접 다시 계산해 일치함을 확인했다. 구현자는 두 격리 빌드 exit0을 보고했고, 별도 tester가 동일 UnitTests exe를 다음 두 명령으로 각각 한 번 실행한 기록이다. 리뷰어 직접 빌드/실행은 아니다.

- NonZoneThreadPoolFixture 10/10 PASS, exit0, gtest8ms/프로세스1.360초. 기존 두 late 완료 부재 기대도 통과했다.
- NetworkRegressionTest12+SessionStateRegressionTest3+PacketViewTest4 합계19/19 PASS, exit0, gtest1ms/프로세스0.364초. 각60초 외부 예산 안이며 retry/기대변경은 없다.

정한 격리 회귀 종료 조건을 충족했다. 정상 transport 종료 두 경합에 대한 source/fixture 근거이며, 실 TCP→SQL 저장→재접속 HP 보존·전체 ZoneChange·일반 세션 세대·종료 중 외부 callback/운영 안전의 검증으로 확대하지 않는다. MainServer 후보는 여전히 build-only이며 이 결과가 실제 전환 승인이나 실행 증거가 아니다.
