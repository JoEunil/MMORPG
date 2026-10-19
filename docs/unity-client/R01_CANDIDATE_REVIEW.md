# R01 후보 구현 독립 리뷰

2026-10-18. **승인된 5파일 후보 구현 범위에는 코드상 차단 문제 없음. 배포는 계속 차단한다.** 최종 소스 hash/diff와 격리 빌드·후보 회귀 기록을 대조했다. 추가 late AUTH/ENTER 회귀는 seam 미승인으로 미검증이다. 리뷰어 소유권은 이 문서뿐이며 서버·테스트·Unity·DB·서비스·빌드 실행/변경은 하지 않는다.

## 승인과 검토 경계

최신 HANDOFF와 R01_PERSIST_REQUEST에 따르면 사용자가 현재 채팅에서 R01 후보 구현·격리 회귀를 직접 지시했다. 허용 범위는 NonZoneThreadPool.cpp의 reset, NonZoneThreadPool.h·NonZoneHandler.h의 동일 fixture friend, 신규 UnitTests/CoreLib/NonZoneThreadPool.cpp 및 UnitTests.vcxproj 등록의 정확한 5파일이다. Slack의 지정 사람 회신 원문/ID는 확인되지 않았으며 Slack 왕복 승인 검증을 주장하지 않는다.

이 승인은 실제 worker 수명 후보와 회귀검증용이다. AUTH/ENTER 늦은 완료의 상태 재삽입 위험이 해결됐거나 배포 가능한 상태라는 뜻이 아니다. 추가 seam과 제품 동작 보완·실행 서버 전환·DB 쓰기는 현재 범위 밖이다. 이전 R01_REVIEW의 배포 차단은 유지한다.

## 기존 초기화·접근 경계 점검

- 허용된 두 friend로 실제 NonZoneThreadPool의 Initialize/Start/Stop/WorkFunc와 concrete NonZoneHandler 초기화에 접근할 수 있다. 따라서 실제 AUTH Process를 제어 가능한 ISessionAuth에 넘긴 뒤 다음 enqueue 없이 PacketView 반환과 stop/drain을 검증하는 후보는 구성 가능하다.
- 늦은 실제 ResponseSession은 PacketWriter::WriteAuthResponse를 호출한다. PacketWriter의 pool 초기화가 private이고 writer 메서드는 비가상이므로 단순 writer mock 대체로 실제 callback 완료를 검증할 수 없다.
- ENTER 결과는 실제 MessageQueueHandler::Process에서 CharacterStateResponse→LobbyZone::ImmigrateChar로 간다. MessageQueueHandler의 의존성 초기화 및 LobbyZone의 StateManager 초기화는 private이다. MessagePool의 Acquire/Return은 public이며 InitializeForTest도 있으므로 이 경로 때문에 MessagePool에 추가 seam을 넣을 필요는 없다.
- StateManager{}의 기본 생성 shard/map와 public AddSession/GetUserID/GetZoneID/Disconnect는 사용할 수 있다. C++20 값초기화된 기본 상태에서 **아직 없는 세션의 Disconnect→첫 AUTH callback**만 검사한다면 private Initialize 없이 관찰 가능하다. 그러나 유효한 인증 세션의 실제 ENTER 요청→zone0 Disconnect→늦은 성공 응답 전체 순서는 Disconnect가 lobbyZone을 사용하므로 그 의존성 주입이 필요하다. 빈 세션에서 성공 응답만 인위적으로 주어 이 전제를 대신하지 않는다.
- Core::Initializer의 public InjectDependencies1/2는 해당 private 초기화의 기존 진입점이지만 recvMQ, Zone, broadcast, chat, perf 작업자와 Zone 상태까지 함께 준비/실행한다. 내부 관찰 대상도 private이고, 이번 좁은 worker/callback 회귀를 위해 전체 topology를 실행하는 것은 최소 대안이 아니다.
- ZoneBenchmarkAccess는 Benchmark/ZoneTick.cpp 내부 정의이며 공용 test helper header가 아니다. 테스트에서 이 이름을 재정의하거나 Core::Initializer를 별도로 정의하는 방식, private 매크로 해제·메모리 덮어쓰기는 사용하지 않는다. 기존 Net::NetworkRegressionTest friend는 Net 객체용으로 Core의 private 접근 권한을 주지 않는다.

## 제안하는 정확한 후속 seam 범위

Coordinator가 아래 **네 헤더에 `friend class NonZoneThreadPoolFixture;` 각 한 줄만** 추가하는 별도 범위를 요청하는 것이 타당하다.

| 추가 파일 | 현재 필요한 접근 |
| --- | --- |
| CoreLib/PacketWriter.h | mock IPacketPool로 실제 writer 초기화, 실제 AUTH/ENTER 응답 생성 경로 유지 |
| CoreLib/MessageQueueHandler.h | 실제 Process 경로의 writer/IOCP/lobby/MessagePool 의존성 초기화 |
| CoreLib/LobbyZone.h | 실제 LobbyZone에 StateManager 연결 |
| CoreLib/StateManager.h | 인증 세션 zone0 Disconnect를 위한 lobby 의존성 준비 |

같은 승인 fixture/테스트 파일을 확장하면 된다. StateManager 회귀는 필요한 lobby 의존성만 fixture에서 준비하고 public 관찰자를 사용하면, 전체 Initialize의 Zone 생성까지 요구하지 않는다. worker와 보류된 callback을 모두 끝낸 뒤 의존성을 정리해야 한다. 새 public API/가상 인터페이스/범용 초기화 계층·runtime 정책은 이 제안에 포함하지 않는다. 이 네 파일은 아직 변경 승인을 받은 것으로 취급하지 않는다.

추가 seam으로 실제 AUTH callback 보류→종료→재개와 실제 ENTER 요청/응답 보류→종료→재개의 결정적 순서를 검사해야 한다. 알려진 재삽입이 나타나면 실패로 보존하고 배포를 중단한다. assertion 완화·sleep·생존 선조회로 race가 해결됐다고 처리하지 않는다. 이 단계는 경합을 실증할 수 있는 최소 접근 범위 제안이며 제품 해결안 설계는 후속 작업이다.

### 추가 요청서 정적 판정

Coordinator의 `R01_LATE_COMPLETION_REQUEST.md`(R01-REGRESSION-02/r1)를 별도로 읽었다. 네 friend와 기존 테스트 확장·격리 회귀만 지정하고 StateManager의 접근을 lobby 의존성 준비로 한정한다. 실제 AUTH/ENTER 완료 경로와 종료 후 Core/lobby 부재 기대를 유지하며, 제품 경합 수정·운영·DB·Unity를 제외한다. **이 추가 seam 요청 범위에는 코드상 차단 문제 없음.** 이는 요청서 범위의 타당성 판정이고, 아직 사람 승인/실제 구현/회귀 통과 판정이 아니다. 당시 git 상태에서도 네 추가 헤더의 수정은 없었다.

## 다음 판정 조건

구현자가 허용된 5파일을 동결하면 실제 diff/hash와 격리 빌드·타깃 실행 결과를 대조한다. 실제 WorkFunc/Process를 사용했는지, 다음 요청 없이 반환·한 번만 Release·stop/drain·Context 종료 통지의 각각이 어떤 테스트에서 관찰됐는지 구분한다. 추가 seam 미승인으로 실행할 수 없는 pending AUTH/ENTER는 **미실행/차단**이며 통과로 쓰지 않는다. 현재 실행 서버 이미지/서비스·DB·Unity는 유지한다.

## 1차 구현 정적 검토 — 최종 실행 근거 대기

검토한 diff는 reset 한 줄, 같은 fixture friend 두 줄, 신규 테스트 파일 및 프로젝트 등록이다. 프로젝트 파일의 마지막 newline 외 관계없는 설정 변경은 없다. 이 **후보 구현 범위에는 코드상 추가 차단 문제 없음**이며 최종 동결/실행 결과 대조는 아직 남았다.

fixture는 실제 production WorkFunc를 한 consumer로 구동하고 실제 NonZoneHandler::Process(AUTH)를 제어 가능한 ISessionAuth로 연결한다. 복제 worker 루프나 가상 handler 대체가 아니다. 다음 enqueue/Stop 전 bounded Release 관찰, 보류 handler와 queued packet 세 개의 stop/drain·정확히 한 번 Release, 원본 packet body 덮어쓰기 뒤 복사된 AUTH 요청 보존을 각각 검사한다. 저장소에 있는 비밀이 아닌 합성 token만 사용하며 출력하지 않는다. ASSERT와 Release 기대를 완화한 경로는 없다.

PendingAuthOwnsCopiedRequestAfterPacketReturn은 없는 세션의 실제 Disconnect 이벤트를 drain한 뒤 복사값을 확인하지만 callback을 실행하지 않는다. 소스 주석도 이를 명시한다. 따라서 **늦은 AUTH 완료 뒤 상태 부재 검증이 아니며**, ENTER 결과 검증도 포함하지 않는다. Net Context의 workingCnt/pending IO/정확히 한 번 종료 통지는 별도 기존 NetworkRegression의 결과와 구분해야 한다.

1차 읽기 기준 SHA256: NonZoneThreadPool.cpp `CF81EF2917C1E11AC472E158D89F9D1F22778105125898E7C5AACA37CF576C04`, NonZoneThreadPool.h `95358A6C7CD255496E53634BBB8106E7F842B4DF4E25AFE719DA57395BD91616`, NonZoneHandler.h `A92F8EAECCDF05E2A94B950661B5C84D88D15710DA64E581D71535C1040703BF`, 신규 테스트 `3BB916217A4E3BEA45CDD7A2D629E9561CEE48326F198AEF8FA5A7F6C6F237E5`, 프로젝트 `26BB90C7619918C0DC7151740752A4C949F58F9FF508066F943D700F94BBC896`. 아직 최종 구현자 freeze 확인/빌드·실행 통과를 뜻하지 않는다.

구현자가 TEST_F 파생 클래스에는 friend 접근이 상속되지 않는 결함을 발견했다. 1차 정적 읽기에서 놓친 부분이다. 테스트 body의 private m_running 직접 조회를 friend fixture 본체의 WaitForStopStarted()로 옮긴 delta를 재검토했다. 같은 2초 대기·조건·Release 기대를 유지하고 파생 body의 private 접근은 제거됐다. 수정 테스트 SHA256은 `27A2666B0798B134D075136912B317BCFD1A6BCC3792F403FCF08D55A6710827`이며 위 테스트 3BB916… 기준을 대체한다. 다른 네 파일은 동일하다. 실제 컴파일 결과는 아직 대기한다.

## 최종 후보 동결·실행 근거 대조

구현자가 동결한 다섯 파일을 다시 hash 확인했다. 위 수정 테스트 27A2666B…와 나머지 네 hash가 모두 읽기 검토 기준과 일치한다. 최종 diff의 제품 동작 변경은 work.reset 한 줄이고, 승인 밖 추가 네 헤더 seam은 적용되지 않았다.

구현자/Coordinator가 격리 빌드 retry exit0을 보고했고 `UnityClient/Builds/R01-Candidate/unit-build-retry.txt`, `candidate-tests.txt`, 격리 exe hash를 읽어 대조했다. 실제 테스트는 **NonZoneThreadPoolFixture 3/3 PASS, 합계 2ms**다. 리뷰어가 직접 실행한 결과는 아니다. 실제 pending callback 완료/late ENTER 안전이나 Net Context 회귀 통과로 확대하지 않는다. 기존 Net 회귀 19개는 이 최종 판정 시점 다른 담당의 실행 예정이며 아직 통과로 기록하지 않는다.

격리 `UnitTests.exe` SHA256: `8D7B1AE14CEA3C34C9C4CC286E90D83C8A6DE59FA156ACC00B195637338DD8FB`. 출력/중간 경로는 R01-Candidate의 bin 및 프로젝트별 obj로 분리됐다. 초기 링크는 MSPDB140.DLL 버전 문제 LNK1101로 실패했고, Coordinator가 허용한 격리 targets의 PDB 생성 제외 후 retry했다. 제품 프로젝트 설정이나 시스템 VS 설치를 바꾼 것으로 보고하지 않는다. retry 로그의 기존 NetTimer atomic 정렬 형식 C4744 경고를 무경고로 표현하지 않는다. 이 후보 diff는 해당 NetTimer 소스를 변경하지 않는다.

기존 `x64/Release/MainServer.exe` SHA256도 읽기 확인하여 `C0F689BD0E58D3A20282143D5DD5C0C15514F42F192A6FF70568C2DDF312E825` 보존과 일치했다. 실행 서버 배포/서비스·DB·Unity 조작은 이번 검증에 포함하지 않는다. 다음 단계는 추가 seam의 정확한 별도 승인 후 알려진 두 비동기 완료 경합을 유한하게 검사하는 것이며, 그 전에는 R01 후보를 안전한 배포 완성안으로 분류하지 않는다.
