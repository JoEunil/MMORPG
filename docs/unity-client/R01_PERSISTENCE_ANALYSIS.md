# R01 — 재접속 HP·MP·좌표 불일치 원인과 최소 변경 요청

2026-10-18. 서버 변경 요청 역할의 읽기 분석이다. 이 문서만 작성했다. 서버/Core/ClientCore/DB/Unity/바이너리를 수정·빌드·실행하거나 서비스를 재시작하지 않았다. 다른 역할의 변경을 보존한다.

## 결론과 확정 수준

**현재 소스에는 비 Zone 작업자가 마지막 처리 패킷을 다음 요청까지 보유하는 결함이 있다. 이 참조가 TCP 종료 뒤 게임 상태 퇴장·저장을 막는다. 실제 C05 로그에도 종료 수분 뒤, 다음 접속 직후에만 이전 세션의 저장 큐 진입이 나타난다.** 따라서 새 접속이 이전 DB 값을 읽는 현상을 설명할 구체적인 원인을 확인했다. 재로그인 회복/부활 정책의 근거는 없다.

소스 결함과 로그의 강한 일치는 확인했지만, 현재 실행 바이너리의 해당 명령어를 역분석하거나 수정 후 재현하지 않았다. SQL 저장 완료/조회 시각과 행 내용까지 연결하는 계측도 없다. 그러므로 모든 HP 차이를 이 결함 하나만으로 확정하거나 수정으로 즉시 재접속의 저장 순서까지 보장한다고 주장하지 않는다.

기준 HEAD는 `1d5a3d69074a1de6bafde30f1ec4dfe5cac1a951`. `git status --short -- CoreLib CacheLib`에서 CoreLib 변경 없음, CacheLib/Config.h·Handler.cpp는 기존 C02 AUTH 소유권 조회 조건 변경만 있다. 현재 디스크 MainServer.exe SHA256은 `C0F689BD0E58D3A20282143D5DD5C0C15514F42F192A6FF70568C2DDF312E825`로 최종 C05 기록과 같다. 디스크 해시 일치는 실행 소스 전체 일치의 증명이 아니다. 본 분석은 프로세스·포트 조작을 하지 않았다.

## 실제 근거

`UnityClient/Captures/c05-account1-live.txt`, `c05-batch-final.txt`, `C05_CONTRACT.md` 마지막 절: 계정1/5011의 동일 연결에서 HP0/MP9919, native Disconnect 후 재진입 fresh Full HP9975/MP10000. 계정2/5012는 보스 접근·피해 후 다음 진입에서 초기 좌표(12,10)/HP·MP10000을 반복 관측했다(후자는 Coordinator 입력 자료 및 C05 진입/접근 캡처 기록). DB 복구를 실행하지 않았다.

현재 `logs/game.log` 원문을 읽어 TCP 종료와 저장 요청의 시각을 비교했다. epoch ms이며 SQL 완료 시각이 아니다.

| 이전 session | TCP Session disconnected | 다음 Session added | 이전 Enqueue Disconnect | TCP 종료→저장 요청 지연 | 다음 접속→저장 요청 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 9 | 1792321331109 | 10: 1792321682652 | 1792321684030 | 352.921초 | 1.378초 |
| 10 | 1792321774171 | 11: 1792322008444 | 1792322009826 | 235.655초 | 1.382초 |
| 11 | 1792322126499 | 12: 1792322380083 | 1792322381457 | 254.958초 | 1.374초 |
| 12 | 1792322429898 | 13: 1792322470306 | 1792322471717 | 41.819초 | 1.411초 |
| 13 | 1792322730911 | 14: 1792322992723 | 1792322994094 | 263.183초 | 1.371초 |
| 14 | 1792323078237 | 15: 1792323365827 | 1792323367210 | 288.973초 | 1.383초 |

session11/12는 로그 AddSession user6981, session13/14/15는 user6982다. Zone ownInternalID와 transport sessionID는 다른 계약이므로 숫자가 같다는 이유로 동일시하지 않는다. C05 순서·사용자·시간으로 대조한다. 최종 로그에는 session15 TCP 종료 `1792323430122` 뒤 Enqueue Disconnect가 아직 없다. 마지막 단계는 저장 완료 증거가 없다.

이 구간 `logs/error.log`에는 각 종료의 CancelIoEx error1168이 있으나 character state update failed/DB worker work failed는 없다. 1168만으로 저장 실패를 단정하지 않는다. 정상 TCP 종료 기록과 게임 퇴장/DB 완료는 구분해야 한다.

Coordinator가 2026-10-18 12:24:34Z에 별도로 실행한 읽기 전용 DB 조회 결과: 5011 HP9975/MP10000, 좌표(39.000019,39.000007); 5012 HP10000/MP9999, 좌표(42.764038,49.472141). 실제 최종 5012 연결 HP0/MP9980, 좌표(49.2,51)와 다르다. 최종 종료 뒤 최신 저장이 아직 도달하지 않았다는 로그 해석과 일치한다. 본 역할이 직접 DB 조회를 수행한 것은 아니다. Coordinator는 PID20024·실행 이미지 SHA C0F689BD… 유지도 읽기 확인했다.

## 지연의 정확한 소유권 경로

1. `NetLibrary/ClientContext.cpp:92`는 PacketView 생성 시 `m_workingCnt`를 증가한다. PacketView는 Context 버퍼의 참조를 소유한다.
2. `CoreLib/NonZoneThreadPool.cpp:35`는 `Event event`를 반복문 밖에 한 번 만든다. `:41`의 `handler->Process(event.work.get())`는 소유권을 넘기지 않으며 처리 후 `event.work`를 비우지 않는다.
3. 다음 `m_eventQueue.Dequeue(event)`는 `BaseLib/BlockingBackPressureQueue.h:59`의 semaphore Acquire에서 먼저 대기한다. 기존 out/Event를 초기화하지 않는다. 따라서 마지막 ENTER/목록/기타 비 Zone 패킷이 작업자마다 하나씩 남을 수 있다. PONG과 Zone ACTION은 이 작업자를 깨우지 않으므로 월드 플레이 중에도 남을 수 있다.
4. TCP 종료 때 `NetLibrary/ClientContext.h:151`의 Disconnect는 connected=false 후 TryNotifyDisconnect를 호출한다. 하지만 `ClientContext.cpp:188`은 workingCnt가 0이 아니면 즉시 반환한다. 아직 보유된 마지막 패킷 때문에 Core에 Disconnect를 통지하지 않는다.
5. 다음 접속의 비 Zone 요청이 같은 작업자에 도달하면 `BaseLib/LockFreeQueue.h:105`의 `out = std::move(...)`가 옛 unique_ptr를 해제한다. `IPacketView.h`의 PacketViewDeleter→PacketView::Release→`ClientContext.cpp:176` ReleaseBuffer로 간다.
6. 마지막 참조이면 `previous == 1`에서 TryNotifyDisconnect→NetPacketFilter→PacketDispatcher→NonZoneThreadPool의 Disconnect 이벤트가 비로소 생긴다. 정상 StateManager 퇴장/저장은 그 뒤다. 실제 다음 접속+약1.38초 패턴과 일치한다. 작업자가 3개이므로 어느 새 요청이 어느 옛 세션을 풀지는 고정 FIFO 계약이 아니다.

이전 연결의 플레이어가 TCP 종료 뒤에도 Zone에서 수분 동안 남을 수 있어 HP가 추가로 변할 수 있다. 표시 종료 시점 HP와 훗날 퇴장 시점 HP의 차이도 가능하다. HP0는 clamp 이후 그대로일 수 있지만 정상 생존 HP의 정확한 저장값은 최종 Emigrate 스냅샷 기준으로 검사해야 한다.

## 저장·로드 경로의 필드 점검

- `StateManager.cpp:59~87`: session→Zone 확인, `ZoneState.cpp:80~100`의 mutex 아래 현재 CharacterState를 out 복사·제거 후 `EnqueueDisconnectMsg` 호출. `StateManager.cpp:109~137`은 characterID, profileId, attack, level, exp, hp, mp, maxima, dir, x/y, lastZone을 MsgCharacterStateUpdateBody로 복사한다. HP를 회복시키는 분기는 없다.
- `CacheLib/InMemoryQueue.cpp:93~127`: 새 pooled Message로 길이만큼 memcpy, 따라서 StateManager의 stack Message 수명 종료는 이 경로의 원인이 아니다. UPDATE는 Important. 큐 거부/고갈은 별도 로그·durable 경로가 있으며 이번 반복 지연의 직접 근거는 없다.
- `MessageTypes.h`: 공통 MsgHeader/MsgStruct와 parseMsgBody가 같은 헤더 크기를 사용한다. 이번 구조는 body의 alignment가 header 크기와 맞는다. 확인한 x64 소스에서 body offset 불일치를 발견하지 못했다. 바이너리 ABI까지 계측한 것은 아니다.
- `CacheLib/Handler.cpp:160~174`: `const auto state = *body` 뒤 lambda가 state를 값 캡처한다. 원래 Message는 Process에서 pool 반환되지만 SQL 작업은 body 포인터를 보유하지 않는다. 이 UPDATE 경로를 read 요청의 포인터 캡처 수명과 혼동하지 않는다.
- QUERY4의 12개 위치는 `attack, level, exp, hp, mp, max_hp, max_mp, dir, last_pos_x, last_pos_y, zone_id, char_id`. ExecuteUpdate 인자도 정확히 같은 순서다. uint16·uint32·int·uint8·float·uint64에 DBConnection::BindOne 오버로드가 있으며 x/y는 setDouble, charID는 setUInt64다. printf/가변인자 SQL 포맷 경로가 아니므로 포맷 지정자 불일치 근거는 없다.
- `DBConnectionGame.h:67~87`는 stmt4를 실제 executeUpdate한다. QUERY4에 HP/MP 복원·기본값 삽입은 없다. 현재 소스에서 autoCommit을 끄는 코드도 발견하지 못했으나 실행 연결의 autocommit을 실측하지는 않았다.
- QUERY3는 저장 hp/mp/maxima/좌표/zone을 직접 SELECT한다. Handler.cpp:140~150→MessageQueueHandler.cpp:54~76→LobbyZone→ZoneState::ImmigrateChar는 이를 복사하며 명시적 회복은 없다. 최초 Full HP9975는 로드 후 이미 피격된 값일 수도 있으므로 ENTER 응답/DB 행과 분리해 확인한다.

**남는 별도 위험:** Cache MQ 작업자3개와 game DB 작업자2개는 세션별 SQL 저장 완료→조회 순서를 보장하지 않는다. 패킷 참조 지연을 고쳐도 즉시 재접속의 모든 SQL race를 해결했다고 단정할 수 없다. 이를 근거 없이 광범위한 큐·락·인증 재설계로 확대하지 않는다. 먼저 아래 단일 수명 수정과 유한한 재접속 검증으로 잔여 실패 여부를 판단한다.

## 보호 영역 변경 요청 후보 R01-PERSIST-01/r1

승인 전 적용하지 않는다. 제품 대상은 **CoreLib/NonZoneThreadPool.cpp의 WorkFunc 한 곳**이다. 기존 동기 handler 호출이 끝나는 즉시 PacketView를 반환한다. 비동기 handler 입력은 이미 Message/SessionCallbackData/ChatEvent로 필요한 값을 복사한다. 비동기 body 포인터를 새로 허용하지 않는다.

```diff
@@ void NonZoneThreadPool::WorkFunc()
         while (m_eventQueue.Dequeue(event))
         {
             if (event.type == EventType::Disconnect)
                 handler->Disconnect(event.sessionID);
             else
                 handler->Process(event.work.get()); // handler에서 비동기 요청은 복사해서 처리.
+            event.work.reset(); // 다음 요청을 기다리기 전에 수신 버퍼 참조를 반환한다.
         }
```

큐 구조·락 순서·프로토콜·DB 스키마·SQL·HP 회복/부활 정책·계정 데이터 보정·콘텐츠 값은 바꾸지 않는다. 작업 종료 직후 반환이 목적이며 Process 실행 중 반환하지 않는다. handler 예외 시 기존 프로세스 동작을 새로 정의하지 않는다. 빈 work의 reset은 무효 동작이다. 실제 새 코드는 승인된 구현 역할이 담당하며 요청 담당은 구현하지 않는다.

관련 회귀 테스트 추가는 승인 범위에 **UnitTests/CoreLib/NonZoneThreadPool.cpp(신규)**와 필요할 때 UnitTests.vcxproj의 해당 파일 등록만 명시하는 방안을 권장한다. 기존 mock 접근 구조로 실제 WorkFunc를 구동할 수 있는지 구현 담당이 확인한다. 불필요한 public API/가상 인터페이스를 만들지 않는다. 테스트 seam이 더 필요하면 정확한 추가 파일/동작을 Coordinator가 검토한 뒤 범위를 결정한다.

### 검증과 종료 조건

1. 실제 비 Zone 작업자에 Release 카운터를 가진 PacketView 1개를 주고 handler 완료 뒤 **다음 Enqueue 없이** Release가 일어나는지 bounded wait로 검증한다. 수정 전에는 대기하고 수정 후에는 반환돼야 한다. 실제 worker 경로를 사용하며 별도 복제 루프만 검증하지 않는다.
2. 마지막 비 Zone 패킷을 처리한 Context를 Disconnect하고, 추가 요청 없이 Disconnect 통지가 정확히1회 발생하는지 검사한다. 기존 pending RECV/마지막 PacketView/Context 재사용 회귀를 유지한다. 루프 종료/이중 종료에서 이중 Release가 없어야 한다.
3. 독립 코드 리뷰·관련 빌드·타깃 회귀를 통과한 빌드의 SHA를 고정한다. 이 단계는 현재 실행 서버를 바꾸지 않는다.
4. 배포는 **별도 정확한 운영 승인** 후 접속0 확인→현재 로컬 게임 서버만 정상 q 종료(30초 초과 시 강제 종료 없이 중단)→기존 실행 파일 보존→새 빌드9999 준비 확인. 로그인/DB/Redis는 유지한다. 새 준비 실패 시 새 프로세스 정상 종료 확인 후 보존 빌드 복구. 이미 종료한 PID/기존 단발 전환 스크립트를 그대로 재사용하지 않는다.
5. 배치 실제 검증은 전용 계정 하나로 안전한 외곽에서 fresh Full/MP/좌표 기록→native 이동·스킬1회→서버확정값 기록→normal Disconnect→**추가 접속 없이** 짧은 bounded 구간 내 해당 session Enqueue Disconnect 확인→읽기 전용 SELECT로 저장값 확인→재접속 ENTER/첫 Full을 대조한다. 피격되는 위치를 피하고 SQL 저장 완료 확인과 TCP 종료 확인을 구분한다. 새 HP0 재현은 저장검증 필수조건으로 두지 않는다. HP0 세션이 자연히 존재하면 별도 데이터 수정 없이 같은 경로를 확인한다.
6. 즉시 재접속에서 여전히 실패하면 read/update 완료 순서를 실제 로그로 분리한다. 실패를 숨기거나 임의 sleep을 제품에 넣어 해결했다고 하지 않는다. 원인이 새 race라면 별도 최소 요청을 만들고 이번 범위의 성공을 과장하지 않는다. 같은 원인 수정2회 한도와 테스트 종료 조건을 따른다.

### 호환·복구·승인 경계

wire/SQL schema 호환성 변경 없음. 정상 handler 이후의 버퍼 반환 시각만 빨라진다. 정상 종료 시 저장 시점도 함께 앞당겨진다. 기존에 수분 동안 잔존하던 세션이 정리되므로 테스트 시작 전 현재 잔존 세션/저장 상태를 읽기 확인하고 보존해야 한다. DB 데이터 초기화/수치 복원은 포함하지 않는다.

소스 복구는 해당 한 줄 및 새 타깃 회귀 등록만 되돌리고 기존 사용자 변경을 보존한다. 실제 서비스 복구는 위 별도 승인된 정상 전환 절차로 이전 보존 빌드를 쓰며 DB를 과거 값으로 되돌리지 않는다.

이 변경은 NonZone 큐/패킷 수명·종료 경로이므로 콘텐츠 자율 변경 범위에 속하지 않는다. Coordinator가 **R01-PERSIST-01/r1 정확 범위의 구현 승인**을 Unity Coordinator 앱의 새 Slack 변경 스레드에 요청한다. 서버 정상 전환 운영 승인은 빌드·리뷰 결과와 정확한 PID/경로/해시가 준비된 뒤 별도로 요청한다. 사람 승인 전 제품 수정/서비스 전환은 없다. 플랫폼 managed sandbox/auto-review 통제는 별개이며 우회하지 않는다. 단계 완료 메시지에는 다음 구체 단계와 지정 사용자의 `!coord 재개 ...` 요청을 포함한다.

## 분석 완료 범위

현재 완료: 소스 수명 결함 확인, 실제 로그 지연 상관관계 확인, SQL 필드/값 복사/로드 점검, 최소 diff·테스트·운영 복구 후보. 미실행: 테스트 추가/빌드/수정 후 재현/DB 행 조회/서버 전환/Unity 입력. 기존 C05 결과를 취소하지 않지만 재접속 HP·MP·좌표 보존 완료로 재분류하지 않는다.
