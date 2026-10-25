# R01-PERSIST-01/r1 후보 구현·회귀

2026-10-18. 소유: 구현 담당. Coordinator가 최신 채팅의 직접 구현·회귀 지시를 배정했다. Slack 사람 원문 ts 확인은 Coordinator가 별도로 기록한다.

- 바꿀 동작: 실제 NonZone worker가 동기 Process/Disconnect 이후 마지막 PacketView를 다음 Dequeue 대기 전에 반환한다.
- 최소 수정: NonZoneThreadPool.cpp reset 한 줄, NonZoneThreadPool.h/NonZoneHandler.h 같은 fixture friend 각 한 줄, UnitTests/CoreLib/NonZoneThreadPool.cpp 및 프로젝트 등록.
- 목표/기대: 다음 enqueue 없이 실제 worker+실제 handler AUTH 처리 후 Release 정확히 1회, stop/drain 이후 중복 반환 없음. 기존 ClientContext 마지막 참조·pending IO·disconnect 회귀를 타깃 실행한다.
- 포함: 제어 가능한 ISessionAuth mock, in-memory packet view, 격리 x64 Release 빌드. 제외: Unity/실서버 통신/서비스 조작/DB/배포/커밋.
- 비용/종료: 단일 실제 worker fixture 및 관련 기존 gtest만 실행. 각 대기는 2초 한도, 무한 sleep 없음. 격리 빌드/타깃 결과와 SHA를 기록하면 종료하며 신규 실패가 없으면 반복하지 않는다.
- 비동기 종료 조건: AUTH→disconnect 완료→늦은 callback, ENTER→disconnect 완료→늦은 load response에 Core/lobby 재삽입이 없어야 배포 가능하다. 추가 private 초기화 seam이 필요하면 승인 범위를 늘리지 않고 미검증·차단으로 기록한다. 현재 PacketWriter/MessageQueueHandler/LobbyZone/StateManager 초기화가 승인된 두 friend에서 접근되지 않아 추가 검토 중이다.

모든 출력·중간 산출물은 UnityClient/Builds/R01-Candidate/로 격리한다. x64/Release/MainServer.exe SHA256 C0F689BD0E58D3A20282143D5DD5C0C15514F42F192A6FF70568C2DDF312E825 유지가 필수다.

결과: 격리 UnitTests/참조 Core 라이브러리 빌드 exit0, 후보 실제 worker 3/3 통과. 초기 linker LNK1101 실패는 보존하고 build-only 최종 link PDB 생성 제외로 재시도했다. 제품/fixture 최종 SHA와 명령·출력은 UnityClient/Captures/r01-candidate-regression.txt. 기존 Net19회귀는 Coordinator의 별도 tester에게 고정 exe로 인계했으며 구현자가 실행하지 않았다. MainServer 후보 exe 빌드/실행은 하지 않았다. AUTH/ENTER late-completion 회귀는 추가 seam 권한 대기로 미검증/배포 차단이다. 실행중 이미지 SHA는 그대로다.

## R01-REGRESSION-02/r1 추가 종료 회귀

Coordinator가 지정 사람 U0C9QEUHPMK의 Slack 회신1792328764.170199(V04부모1792326340.552749)의 R01-REGRESSION-02/r1 추가회귀 재개 원문을 확인·수용하고 배정했다. 최신 채팅의 직접 지시도 확인했다.

- 목표: AUTH 요청→참조 반환→실제 worker Disconnect 완료→보류 성공 callback, 이미 인증된 session의 실제 ENTER 요청→참조 반환→실제 Disconnect 완료→보류 성공 load response 뒤 Core/session 및 lobby 부재를 검사한다.
- 최소 수정: PacketWriter.h/MessageQueueHandler.h/LobbyZone.h/StateManager.h에 같은 fixture friend 각1줄, 기존 NonZoneThreadPool.cpp test 확장. StateManager의 lobby 의존성만 주입하며 실제 Zone/전체 topology는 초기화·실행하지 않는다.
- 포함: 기존 실제 worker/handler/callback/MessageQueueHandler 경로와 제어 가능한 in-memory auth/queue/packet pool/IOCP mock. 기존 후보3 유지. 제외: 경합 제품 fix/인증 정책/기타 서버 파일/Unity/DB/실네트워크/서비스 조작/배포/커밋.
- 기대: 위 두 late completion 뒤 Core/lobby 상태가 없어야 통과한다. 현재 제품의 ghost 상태 예상 실패는 실패로 보존하며 기대 완화·sleep을 넣지 않는다.
- 비용/종료: 각 대기2초 한도, 단일 worker만 실행하는 기존 fixture 확장2개. 격리 Release 빌드 후 source/exe hash를 동결하고 독립 리뷰·별도 tester 실행으로 인계한다. 구현자는 이번 단계를 직접 실행하지 않는다. 추가 seam 필요 시 중단·정확 범위를 보고한다.

추가회귀 준비 결과: 승인4 friend와 기존fixture 확장2개를 동결했고 격리 Release UnitTests 빌드 exit0. fixture SHA57362DEFBD45C6F5DF21874121F170F7196DF196844629E02EA740ECD51059FF, exeF60BFB7D520C7E8FA3DDD5AABA57D78904D6D7A06A1914402833DA9248DA0E5C. 제품경합 fix는 없다. 독립 source 리뷰 완료 보고를 받았으며 구현자는 새테스트를 실행하지 않았다. 별도 tester에게 NonZoneThreadPoolFixture.* 총5개 1회 실행을 인계한다. 빌드/전체hash/명령은 Captures/r01-late-completion-build.txt 및 r01-late-completion-ready.txt. 원래 서버 이미지 C0F689BD… 보존. 늦은완료 안전성은 실제 실행 전까지 미검증/배포차단이다.

## R01-CLOSE-03/r1 정상 TCP 종료 경계 후보

최신 채팅의 직접 후보 구현·회귀 지시를 Coordinator가 정확 범위 승인으로 수용해 배정했다. 새 Slack 사람 회신 ts는 미확인으로 구분하며 만들어내지 않는다.

- 바꿀 동작/최소 위치: 승인 patch E3D832EA214D27FC0985816E8E91E527E38BDABD47FB637BFB1ED687ADCA7D7B의 StateManager.h/NonZoneHandler.cpp/LobbyZone.h 제품 hunk만 적용한다. AUTH pending 등록/존재 entry 최초 완료와 기존 callback 응답 정책 보존, lobby의 shard→lobby 검사·삽입만 바꾼다. 기존 reset/friend/등록 및 타 작업자의 변경을 보존한다.
- 검증 범위/기대: 기존5 기대 유지. 실제 AUTH/callback으로 pending 미인증·최초 성공/실패 응답·실패/성공/다른UID callback의 최초 결과 유지·UID0, 실제 ENTER/load 완료 후 종료와 종료 후 완료 양순서를 검사한다. lobby 중복/Emigrate→rollback은 기존 component 경계이며 실제 ZoneChange 전체 통과가 아니다.
- 제외/한계: 새 seam·인증 정책 확대·직접Core삭제 후 sameID 재AUTH 세대·기존 Zone 동시성·SQL update/load 순서·서비스 종료 callback 수명은 별도다. 실서버 통신/후보MainServer실행/서비스시작종료/DB/Unity입력/Assets/원본Game쓰기/커밋 없음.
- 비용/종료: 각 gate2초·외부실행60초 한도. 구현자는 격리 UnitTests/참조lib 및 MainServer 후보 build-only 뒤 source/exe/목록·DLL hash를 동결한다. 독립 리뷰 후 별도 tester가 타깃/관련Net19를 각1회 실행한다. 구현자 직접 test 실행0, 기대 완화·sleep·실패 은폐 없음. 범위 불일치/추가 seam 필요 시 즉시 보고한다.

후보 작성/빌드 결과: 승인 제품 patch의 hash 확인·apply 및 역방향check 통과. source4 동결 후 격리 UnitTests/참조lib와 MainServer build-only 모두 exit0. 기존5 기대유지·새5로 fixture10, 관련Net19의 source-derived 목록을 고정했다. 구현자 실행0이며 독립 리뷰 후 별도 tester로 각1회 실행을 인계한다. exe/제품/목록/OutDir DLL 전체hash와 정확명령은 Captures/r01-close-candidate-ready.txt, 빌드로그 r01-close-unit-build.txt/r01-close-main-build.txt. MainServer 후보 SHA01C6261AB5C3448E2BA5F336CE70FB20E4DBA4B9ED47AD70002F22C88594D108, UnitTests5504D57C743A78A174C5B5D2C296E352CBE4A3E6B68A45692D2AF9749A94B08F. 원래 실행이미지 C0F689BD… 유지. 아직 새 회귀 실행/실SQL/실서버/Unity/운영전환 통과가 아니다.
