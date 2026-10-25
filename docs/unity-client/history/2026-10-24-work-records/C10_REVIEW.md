# C10 서버·ClientCore 독립 리뷰

2026-10-19. 결론: **코드상 차단 문제 없음**. 이는 동결된 서버·ClientCore 후보의 정적 검토 결과이며 기능 시험·Unity 입력·실DB·UI 완료 판정이 아니다.

## 대상과 근거

- 격리 후보 `UnityClient/Builds/C10-Delivery/source`, 부모 `0e2adcf`. 루트 HANDOFF/AGENTS/역할 reviewer·coordinator, 후보 C10_CONTRACT/TASK/SERVER_IMPLEMENTATION/CLIENTCORE_IMPLEMENTATION을 읽고 실제 diff 및 새 파일과 대조했다.
- 서버 제품·fixture 17파일, ClientCore 제품 6파일, 기존 INetworkService fake 10곳의 stub 변경, 새 C10BazaarMyListTests 및 meta를 검토했다. 기존 C09 Search/P28 주변 코드와 양쪽 InMemoryQueue·DBWorker·MessagePool·실제 TCP 수신/dispatch 경로를 함께 읽었다.
- 코드·fixture·Assets·설정 수정, 빌드·기능 시험·DB·서버·Unity 조작, Git 통합·게시·Slack 발신은 수행하지 않았다. 이 리뷰 문서만 작성했다.

## 확인한 경계

| 항목 | 코드상 확인 결과 |
| --- | --- |
| 인증과 소유권 | StateManager.h:178의 GetFieldCharacterID가 단일 session shard shared lock 아래 authCompleted/authenticated/userID/characterID/field zone을 확인한다. NonZoneHandler.cpp:196은 정확한 12바이트 요청에서 requestID만 읽으며 소유자는 이 스냅샷에서 얻는다. MessageQueueHandler.cpp:92와 요청 queue 실패 응답도 현재 field 캐릭터 일치를 재검사한다. |
| Wire와 용량 | 요청28은 header8+ID4/flags0, 응답29는 header8+ID4/status1/count2+39×count이며 최대1185바이트다. 내부 자연 정렬 행을 wire에 memcpy하지 않고 PacketWriter.cpp:10에서 필드별 packed 행을 쓴다. count<=30와 양쪽2048 메시지 sizeof guard, packet capacity 검사, TCP 길이·flags 검사가 있다. |
| DB 결과 | query11의 seller 필터/CANCELLED 제외/LIMIT30과 SELECT 경로를 유지한다. CASE만 상태0/1/2/255를 구분한다. BazaarMyListResult.h:14가 NULL 결과·열/예외/부분 읽기/축소 범위 초과/잘못된 owner·ID/31행을 전체 실패로 만들고 ID·owner를 보존한다. 정상0행은 성공이며 실패의 잔여 행은 초기화된다. |
| 비동기 소유권 | Handler_Bazaar.cpp:17은 요청을 값으로 복사한 뒤 소유 메시지를 비동기 작업 또는 즉시 거절 경로에 넘긴다. DBWorker는 Queued 작업 실패 때만 callback을 호출하며 Rejected에서는 호출하지 않는다. 호출부의 거절 처리와 중복되지 않는다. 양쪽 InMemoryQueue는 내용을 복사하므로 enqueue 후 원본 Return이 맞으며 Handler::Process의 추가 Return은 msg=nullptr로 방지한다. 응답 queue 거절은 로그 후 반환한다. |
| ClientCore 수명 | ViewModel.cs:310/344가 단일 pending ID와 Search/MyList 종류를 함께 검증한다. 취소 후 ID를 재사용하지 않고 uint 고갈도 거부한다. 수신 배열을 dispatcher 전에 복사하고 메인 스레드 반영 시 lifetime/world/종류/ID를 다시 검사한다. 기존 CancelBazaar가 닫기·탭·timeout 및 ZoneChange/Error/Dispose 정리 경계로 남아 있다. socket I/O 중 session lock을 유지하지 않는다. |
| 엄격한 수신 | Packet.cs:657은 실제 header와 전달 header의 일치, ID/status/count/정확한 길이/비영 행 ID·수량/중복 매물/상태를 검증한다. MyList 상태0~2와 Search 상태0을 구분한다. Handler는29를 simulation 경로 전에 처리하고 malformed 예외를 TCPSocket disconnect 경계로 전달한다. 기존 TCP frame 분할·결합 처리를 재사용한다. |
| 범위와 단순성 | 가격·매매·재화·procedure/schema/outbox 변경이 없다. SOLD/CLAIMED에서 정산 가능 여부를 합성하지 않는다. 기존 Search 및 P28 제품 경로를 제거하거나 완화하지 않았다. 새 Manager/서비스/타이머/범용 프레임워크가 없으며 공통 parser·request 시작·수신 helper는 실제 두 종류가 공유한다. |

기존 fixture 10개의 변경은 새 network 메서드 stub 추가와 해당 줄 분리뿐이며 assertion·실행 조건은 보존됐다. 서버 새11 case 및 C# 새11 case는 빈 성공/오류·범위/owner·정확한 wire·종류/ID 혼합·배열 snapshot·취소/수명·send 실패를 검사하도록 작성됐다. fixture 소스 검토를 실행 통과로 간주하지 않는다.

## 빌드 기록과 남은 검증

구현자의 빌드 로그 끝부분과 compile summary를 읽고 현재 산출물 SHA256을 직접 대조했다. 아래 해시는 구현 인계와 일치한다. 리뷰어는 빌드나 시험을 재실행하지 않았다.

- MainServer.exe: `3D1011A6B7C8E2FE6A0ECC3C3B739E65EBBC340F4F80B0DF8E213073CAAB402E`.
- UnitTests.exe: `E59EF6FA904557743E79BCFA1AB5FFA868BB29CE2EDD7FEB3C9B350398D87EE0`.
- ClientCore.dll(netstandard2.0): `CCD88B33B6A9A4923AD48FE816A62B4688F38018CA7AB7A2BE21C9490623F473`.

Release 컴파일 성공은 인계된 실행 기록이며 기존 C++ 경고·NUnit 참조 컴파일의 NU1900 endpoint 경고는 보존돼 있다. 후보 DLL의 Unity 적용, UI 연결, 새/영향 회귀 fixture 실행, TCP 실제 송수신, 실제 query11 SELECT, 두 전용 계정 관찰은 이번 검토의 실행 근거가 없다. 특히 현재 C10_TASK의 조사 단계 표기는 초기 카드 상태이며 구현 인계와 이 문서가 실제 후보 상태를 보충한다.

다음은 UI 최소 연결과 해당 독립 검토 후 한 번의 동결 배치다. 포함할 범위는 새11+11 case와 기존 C09/P28 영향 회귀, TCP29 분할·결합 및 malformed disconnect, 실제 MARKET→내 매물→새로고침→판매 검색 복귀→닫기/정상 퇴장이다. 실DB는 SELECT와 승인된 정상 로그인/퇴장만 사용하고 거래·seed·Outbox 시험은 제외한다. 실제 양수 행이 없으면 fixture 양수 결과와 실계정 빈 성공을 분리해 보고한다. 구현·리뷰·동일 후보의 배치 근거를 모두 갖춘 뒤 Coordinator가 PR 직전 Slack 최종 승인 절차를 진행한다.
