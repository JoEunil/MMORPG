# R01-CLOSE-03/r1 — 늦은 AUTH·ENTER 완료의 종료 경계 후보

2026-10-18. 서버 변경 요청 역할의 분석·승인 후보이며 **제품 적용·빌드·실행·배포 승인이 아니다.** 이 역할은 이 문서와 docs 안의 `R01_CLOSE_03_candidate.patch`만 작성한다. 제품·테스트·Assets·설정·DB·서비스·Slack·Git은 변경/실행하지 않는다. 다른 작업자의 승인된 reset·friend·fixture 변경을 보존한다. Coordinator가 정확한 후속 범위를 요청하고 독립 리뷰한다.

## 확인된 실패와 참조 버전

HEAD `1d5a3d69074a1de6bafde30f1ec4dfe5cac1a951`. R01 reset 후보와 추가 fixture 접근점이 작업 트리에 있다. 직접 읽은 `UnityClient/Captures/r01-late-completion-regression.txt`는 별도 tester가 고정 UnitTests.exe `F60BFB7D520C7E8FA3DDD5AABA57D78904D6D7A06A1914402833DA9248DA0E5C`를 한 번 실행한 결과다. exit1, gtest 5ms, 기존 3개 통과·늦은 완료 2개 실패다. 이 역할이 테스트를 실행한 것은 아니다.

- AUTH: 실제 worker의 요청/PacketView 반환 → 실제 Disconnect 완료 → 저장된 실제 ResponseSession callback을 재개하면 session42가 zone0/user6982로 다시 만들어진다. 기대값 zone-1/user0 실패다.
- ENTER: 실제 인증/ENTER 요청 복사·PacketView 반환 → 실제 Disconnect 완료 → 실제 MessageQueueHandler의 성공 로드 응답 뒤 lobby character42가 다시 생긴다. 부재 기대가 실패다.
- 이전 Net19 통과는 별도 단계의 결과다. 이 두 실패를 통과로 바꾸거나 실 SQL/Unity/실행 서버 검증으로 확대해 해석하지 않는다. 기존 실행 MainServer.exe는 `C0F689BD0E58D3A20282143D5DD5C0C15514F42F192A6FF70568C2DDF312E825`이며 후보 exe와 다르다.

읽은 제품 기준 SHA256: StateManager.h `835DF6B2927908BD154BB1057BEED64EC5D597A291C87D65E23F47E5E8ED3777`, LobbyZone.h `A0FDA201C94FB15328D129B27218668DB8149E2F079654B414E94B0A7E80B1DD`, NonZoneHandler.cpp `8FADE9AE7DBE53535CDAA4A9CD8033B087B4AC3D957AD7CB53A9F10AE42B8BEA`. 소스/현재 실행 이미지 전체 일치는 주장하지 않는다.

## 원인과 필요한 원자성

`NonZoneHandler.cpp`의 ResponseSession은 callback 때 AddSession을 새로 수행한다. AUTH를 처리하는 동안 Core 등록은 없으므로, PacketView 반환 뒤 transport 종료가 먼저 Core Disconnect를 완료하면 callback의 무조건 삽입을 막을 정보가 없다. IOCP 송신 성공 여부는 이미 추가한 상태의 소유권 경계가 아니다.

`MessageQueueHandler.cpp::CharacterStateResponse`는 성공 로드 시 `LobbyZone::ImmigrateChar`를 호출한다. 현재 함수는 lobby mutex 아래 map에 먼저 넣고 StateManager::SetZoneID를 호출한다. 이미 삭제된 세션은 SetZoneID가 무시하므로 orphan lobby가 남는다. 또한 lobby→session shard 순서가 Disconnect/CleanUp의 shard→lobby와 반대다. 단순 생존 선조회 뒤 삽입은 TOCTOU를 남긴다.

필요한 것은 transport 종료가 통지되기 전에 AUTH의 pending 소유권을 Core에 등록하고, callback과 Disconnect가 **동일한 session shard lock**으로 그 entry를 완료/삭제하는 것이다. ENTER는 동일 shard lock을 유지한 채 존재·인증·zone0을 확인하고 lobby mutex 아래 삽입한다. 송신이나 Redis/DB 대기를 이 lock 안으로 옮기지 않는다.

## 최소 제품 후보 — 3파일

구체 diff는 `R01_CLOSE_03_candidate.patch`다. 적용 전 원본/승인 범위를 다시 대조하며 기존 두 R01 후보를 덮어쓰지 않는다.

| 파일 | 정확한 변경 |
| --- | --- |
| CoreLib/StateManager.h | SessionData에 `authCompleted=false` 1개. 기존 AddSession 한 곳을 BeginAuth/CompleteAuth로 교체. BeginAuth는 기존 shard lock 아래 try_emplace만 수행하며 기존 entry는 건드리지 않는다. CompleteAuth는 같은 lock 아래 **존재하는 entry만** 최초 완료하고, 이미 완료된 entry는 유지한다. LobbyZone에 내부 friend 접근을 부여한다. |
| CoreLib/NonZoneHandler.cpp | CheckSession에서 복사된 callback data를 만든 후, 외부 auth 요청 전에 BeginAuth 호출. ResponseSession은 CompleteAuth가 entry 부재로 false이면 끝내고, 그 외 기존 resStatus 그대로 응답한다. |
| CoreLib/LobbyZone.h | 기존 ImmigrateChar가 StateManager의 해당 shard unique_lock을 먼저 잡고 존재/authenticated/zone0을 확인한 뒤 기존 lobby mutex 아래 중복 검사·삽입한다. 기존 SetZoneID(0) 재진입은 제거한다. |

StateManager.cpp·MessageQueueHandler.cpp·NonZoneHandler::ZoneChange·프로젝트 등록 파일은 변경할 필요가 없다. 새 추상화/콜백 잠금 API/전달 함수/새 queue/Net-Core 계약은 없다. **friend class LobbyZone은 테스트 seam이 아니라 기존 두 클래스의 내부 동기화 접근 확대**이며 그 범위를 승인·리뷰에 명시한다. 기존 fixture friend를 재사용하므로 새 테스트 friend는 필요 없다. 별도 구현/회귀 승인 요청은 위 3제품 파일과 기존 `UnitTests/CoreLib/NonZoneThreadPool.cpp` 확장, 격리 후보 빌드·유한 실행까지로 한정할 수 있다. 문서 갱신은 Coordinator 소유 작업 카드와 이 분석/리뷰에만 한다.

### AUTH의 순서와 기존 정책 보존

BeginAuth는 worker가 실제 AUTH PacketView를 보유한 상태에서 실행한다. `ClientContext::TryNotifyDisconnect`는 workingCnt=0 뒤에만 transport 종료를 통지하고, 마지막 PacketView는 handler 반환 후 reset된다. 따라서 정상 transport 종료의 Core Disconnect가 BeginAuth보다 앞설 수 없다. 같은 연결의 미처리 AUTH PacketView도 workingCnt에 포함되므로 그 작업들이 뒤늦게 pending을 재생성하지 않는다. 세션 ID는 Net에서 연결마다 증가하며 닫힌 transport의 새 요청을 재수용하는 경로를 추가하지 않는다.

- callback이 shard lock을 먼저 잡으면 기존 entry에 결과를 반영한다. 뒤의 Disconnect가 그 entry와 lobby를 지운다.
- Disconnect가 먼저 잡으면 pending entry를 지운다. callback은 부재에서 false를 반환하고 상태/응답을 만들지 않는다. tombstone 보관은 없다.
- CompleteAuth의 반환값은 **인증 성공이 아니라 현재 entry에 대한 callback 처리 허용**이다. 살아 있는 연결의 최초 실패도 authCompleted=true, authenticated=false로 기록하고 기존 실패 응답을 보낸다. 외부 서비스가 callback을 소유·해제하는 기존 계약은 유지한다.
- 기존 AddSession은 최초 callback만 상태를 정한다. 중복 AUTH의 나중 callback, 실패 후 재AUTH 성공, 다른 userID로 재AUTH 성공도 기존 상태를 덮어쓰지 않고 자기 응답만 보낸다. 후보는 이 동작을 그대로 보존한다. 실패 후 성공 응답과 Core 미인증 상태가 달라질 수 있는 기존 정책 문제를 이번 종료 수정에 섞어 고치지 않는다.
- pending은 uid0/zone0/authenticated=false지만 userID0을 sentinel로 판정하지 않는다. 유효 userID0 여부가 확정되지 않았고 실패 완료도 authenticated=false이므로, 최초 완료 여부를 구별할 독립 bool이 현재 필요하다. BeginAuth는 기존 인증·실패 entry의 UID/zone/character/cheatCount를 초기화하지 않는다. 최초 완료만 기존 AddSession과 같이 완료 시각·값을 설정한다.
- HealthCheck는 pending에서 EXIST/NOT_CHEAT만 반환하고 AUTHENTICATED를 반환하지 않는다. NetPacketFilter의 현행 분기상 AUTH/PONG 외 요청은 계속 거부된다. pending이 없던 때도 허용되던 AUTH/PONG의 종류는 같다. 미인증 진단 로그의 발생 시점은 앞당겨질 수 있다.
- CompleteAuth가 lock을 놓은 뒤 응답 전 Disconnect가 와도 Core 재삽입은 없다. 늦은 송신은 기존 IOCP sessionID 경계가 처리하며 송신을 원자적 상태 완료와 동일시하지 않는다.

### ENTER의 순서와 rollback

ImmigrateChar의 검사와 map 삽입은 같은 shard lock 구간이다. 완료가 먼저면 Disconnect가 기다렸다가 lobby를 제거한다. Disconnect가 먼저면 entry 부재로 삽입이 거부된다. `SetZoneID(0)`은 zone0 사전조건을 통과한 동일 entry에 중복 대입하던 호출이므로 제거해도 정상 상태는 동일하다. 같은 non-recursive shard mutex의 재획득도 없어진다.

두 호출점 모두 기존 ImmigrateChar를 그대로 호출하므로 빠지는 경로가 없다. MessageQueueHandler의 성공 로드 응답은 삽입 실패면 기존과 같이 응답 없이 종료한다. NonZoneHandler::ZoneChange(ENTER)는 lobby에서 꺼낸 뒤 Zone 진입 실패 시 같은 함수로 rollback한다. 정상 실패에서는 zoneID가 아직0이고 인증 entry가 남아 rollback된다. 이미 Disconnect가 entry를 지웠으면 rollback 삽입도 거부되고 기존 실패 처리의 Disconnect는 무효 동작이다. Zone 실패는 SetZoneID 전에 반환함을 실제 ZoneState::ImmigrateChar의 capacity/duplicate/ID-exhausted 분기에서 확인했다.

기존 lobby 중복 검사·로그는 유지한다. 없는/미인증/zone≠0 entry의 로드 삽입을 새로 거부하는 것이 정확한 동작 변화이며 새 프로토콜 상태코드나 소유권 정책은 추가하지 않는다. characterID/요청 세대 매칭, ENTER 중복/Zone 이동 전체의 재설계는 범위 밖이다.

## 검증 계획·비용·종료 조건

승인 전 제품/테스트 적용·후보 빌드를 하지 않는다. 승인 후 구현자는 기존 fixture 한 파일을 소유하고, 독립 리뷰 뒤 고정 exe를 별도 tester에게 인계한다. 목표는 아래 관찰이며 actual 실행 요약은 기존 관례대로 txt로 남긴다.

1. 기존 다섯 fixture를 기대 완화 없이 한 번 실행한다. 기존3 통과 유지·실증된 late2 부재 기대 통과를 요구한다. copied request의 `An auth-pending session does not yet exist` 주석만 새 pending 존재에 맞춰 갱신하고, 그 테스트의 Disconnect 후 부재 기대는 그대로 둔다.
2. 같은 fixture 접근점으로 pending HealthCheck의 미인증/UID0, 정상 최초 성공→Disconnect, 최초 실패 응답 유지, 실패→나중 성공 및 다른 UID callback의 **최초 완료 상태 유지**, userID0 최초 완료를 검사한다. 외부 auth stub은 기존 소유권에 맞게 각 요청 callback을 한 번만 호출한다. 실제 callback/worker 경로를 재사용한다.
3. 정상 성공 로드→lobby 존재→Disconnect→부재와 기존 close→load 부재 양쪽 순서를 검사한다. lobby 중복 삽입 거부, lobby Emigrate 후 zone0 rollback 성공, 삭제 후 같은 rollback 삽입 거부를 기존 함수로 검사한다. raw 필드 대입만으로 제품 경로 통과를 대신하지 않는다.
4. 동시 완료/Disconnect는 pending 또는 인증 session 준비 뒤 barrier로 두 실제 작업을 동시에 출발시켜 bounded join/최종 부재를 검사할 수 있다. 어느 lock 순서가 실제로 선택됐는지 모르는 실행을 결정적 양순서 증명으로 쓰지 않는다. 두 순서의 결정적 검증은 1~3과 소스 lock 검토가 맡는다. 새 제품 hook은 우선 추가하지 않으며 필요하면 정확한 seam을 별도 범위로 검토한다. 임의 sleep/무한반복/생존 선조회는 넣지 않는다.
5. 격리 출력 UnitTests 및 필요한 CoreLib 빌드, header 변경 영향의 기존 Net/SessionState/PacketView 19개를 한 번 확인한다. 실행 서버/MainServer 후보 실행은 제외한다. 현 build-only Isolated.targets의 PDB 우회는 기존 격리 설정이며 제품 설정을 수정하지 않는다. 빌드 시간은 미측정, 실행은 1회/60초 외부 제한, 내부 기존 2초 gate를 유지한다. 두 신규 실패의 이전 5ms는 향후 빌드/실행 시간 보장이 아니다.

리뷰 고정 버전에서 타깃/관련 회귀가 통과하면 이 후보의 정적·격리 검증을 종료한다. 실패는 원문을 보존하고 원인별2회 한도를 따른다. 테스트를 더 늘리거나 실제 서버 전환/SQL/Unity 입력으로 자동 확대하지 않는다. 이번 역할은 위 검증을 아직 수행하지 않았다.

## 호환성·복구·잔존 위험

wire/SQL schema/DB 데이터/Redis·외부 auth API/실행 서비스는 바꾸지 않는다. SessionData는 Core 내부 구조이며 bool은 wire에 직렬화되지 않는다. padding 포함 메모리 크기 변화는 측정 전 단정하지 않는다. pending entry는 연결이 살아 있는 동안 존재하고 정상 Disconnect에서 삭제되므로 종료 tombstone의 무한 저장은 없다. 외부 auth 요청 대기 자체를 취소하거나 timeout 정책을 새로 만들지는 않는다.

복구는 승인 후 적용한 이 3제품 파일의 정확한 hunk와 새 fixture 확장만 되돌린다. 기존 R01 reset·이미 승인된 friend·다른 사용자 변경을 보존한다. DB 복구/프로세스 종료/바이너리 교체는 없다. 실행 서버 전환은 별도 운영 요청·승인과 잔존 상태/저장 확인·보존 빌드 복구안이 필요하다.

남는 한계는 다음과 같다.

- 보장은 실제 transport 종료 통지와 그 뒤의 늦은 AUTH/ENTER 완료다. 검증 실패 등 NonZoneHandler가 직접 부르는 Core Disconnect는 transport를 닫지 않는다. 그 뒤 같은 transport에서 새 AUTH로 같은 sessionID pending을 재생성하면 이전 callback의 세대 구별은 이 후보에 없다. 그 경로의 일반 세션 재개/인증 정책을 해결했다고 하지 않는다. 필요하면 정확한 별도 요청이 필요하다.
- 기존 ZoneState::ImmigrateChar는 Zone mutex→SetZoneID shard 순서이고 StateManager::Disconnect는 shard→Zone mutex다. 이번 lobby 순서는 이 기존 Zone lock 역순을 해결하지 않는다. 일반 ZoneChange/중복 ENTER와 이동의 전체 원자성·세대 매칭도 보장하지 않는다. 정상 lobby rollback만 확인 대상이다.
- reset 이후 실제 SQL update 완료→다음 load 순서는 여전히 Cache/DB 여러 worker 사이에서 보장되지 않는다. 이번 late 완료 회귀는 실 SQL 저장/재접속 HP 보존 증거가 아니다.
- 서비스 종료 중 외부 callback과 전역 StateManager/writer 수명, 외부 auth의 기존 오류/재시도 정책도 별도다. 후보는 callback에서 packet memory를 다시 보유하거나 새 전역 수명을 만들지 않는다.

후속 요청 이름은 **R01-CLOSE-03/r1**, 목적은 확인된 두 종료 후 재삽입을 위 좁은 경계로 막는 제품 후보 구현·독립 리뷰·격리 회귀다. 이 문서 작성/분석자의 동의/회귀의 기존 실패는 사람 승인이나 배포 승인이 아니다. Coordinator가 지정 Slack 변경 스레드에서 이 정확한 범위를 승인 요청하며 플랫폼 managed sandbox/auto-review는 별도 통제다.
