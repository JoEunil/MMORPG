# R01 독립 정적 리뷰

2026-10-18. 검토 대상은 `R01_PERSISTENCE_ANALYSIS.md`의 R01-PERSIST-01/r1 후보와 관련 실제 소스·로그다. 분석 문서 SHA256: `EC3A166D7CDE667158F619235EAA2436558FE3662BF9B46DC902C82B57AC6E38`. 리뷰어는 이 문서만 작성했다. 서버·DB·서비스·Unity·빌드·테스트를 변경하거나 실행하지 않았다.

## 판정

**배포 완성안으로는 차단 문제 있음. 격리된 후보 구현·실제 worker 회귀검증만을 위한 조건부 개발 범위는 구분하여 요청할 수 있다.** `event.work.reset()`의 참조 반환 개선은 소스 근거가 있고 최소지만, 비동기 AUTH/ENTER 완료와 disconnect의 순서를 안전하게 처리한다는 증거는 없다. 후보 개발 승인이 알려진 경합의 해결이나 실행 서버 전환 승인을 의미하지 않는다. 현재 사람 승인 요청 전이며 서버 수정은 없다.

## 확인한 수명 결함과 최소 후보

- `CoreLib/NonZoneThreadPool.cpp:35,41`: 반복문 밖 Event가 Process 뒤에도 PacketView를 소유한다. 다음 Dequeue는 semaphore를 먼저 기다리므로 새 요청 전까지 마지막 패킷 참조가 남는다. PONG/Zone ACTION은 이 작업자의 다음 이벤트를 보장하지 않는다.
- `NetLibrary/ClientContext.cpp:176,187`: 마지막 ReleaseBuffer에서 workingCnt가 0이 되어야 종료 통지를 진행한다. 따라서 TCP 종료와 Core 퇴장·저장 사이의 지연 원인이 된다. 다음 pop에서 unique_ptr 대입이 옛 참조를 반환하는 현재 경로와 분석이 일치한다.
- 실제 `logs/game.log`의 session9~14 여섯 사례를 대조했다. TCP 종료→저장 요청은 41.819~352.921초, 다음 접속→이전 저장 요청은 1.371~1.411초다. session15는 마지막 TCP 종료 뒤 저장 요청이 없다. 이는 강한 로그 부합이며 SQL 완료나 모든 HP 차이의 단독 원인까지 입증하지 않는다.
- 현재 NonZoneHandler의 비동기 경로는 필요한 값을 SessionCallbackData/Message/ChatEvent 등에 복사한다. Process 반환 뒤 원 PacketView를 보유하는 경로는 확인하지 못했다. 따라서 동기 호출 뒤 `event.work.reset()` 자체에 사용 후 해제 근거는 없다. 빈 work reset은 무효이고, 다음 queue pop 안에서 하던 해제를 handler 반환 뒤로 옮기므로 새 큐 락을 추가하지 않는다. Context의 기존 recursive mutex·정확히 한 번 종료 통지·재활용 조건도 유지한다. 기존 handler 예외 정책의 개선을 이 한 줄에 포함하지 않는다.

## 배포를 차단하는 비동기 종료 경계

**[P2] 늦은 AUTH 완료가 종료 처리 뒤 Core 세션을 다시 등록할 수 있다.** `CoreLib/NonZoneHandler.cpp:114`의 ResponseSession은 `StateManager::AddSession` 후 응답을 보낸다. `ExternalLib/SessionAuth.cpp`는 Redis 비동기 callback으로 이를 나중에 호출한다. AUTH Process→패킷 반환→TCP 종료→Disconnect가 아직 없는 Core 세션을 보고 반환→AUTH callback 순서가 가능하다. `StateManager.cpp:59`의 없는 세션 종료는 닫힘 표시를 남기지 않고, `StateManager.h:90`의 AddSession은 전송 연결의 생존 여부를 검사하지 않는다. IOCP 응답 송신이 실패해도 이미 넣은 Core 상태는 취소되지 않는다. 즉 PacketView의 메모리 수명과 비동기 작업의 논리 수명은 다르다. 기존에도 잠재한 경합이지만 즉시 반환으로 이 순서가 가능해지므로 한 줄만으로 안전한 배포를 단정할 수 없다. 실제 빈도는 미실증이다.

**[P2] 늦은 ENTER 로드 완료도 종료 뒤 lobby 상태를 넣을 수 있다.** `CoreLib/MessageQueueHandler.cpp:50,70`은 성공 응답을 받은 뒤 LobbyZone::ImmigrateChar를 호출한다. `CoreLib/LobbyZone.h:38`은 lobby map에 먼저 넣고 SetZoneID를 부르며, 이미 없는 Core 세션의 SetZoneID는 아무것도 하지 않는다. disconnect가 먼저 끝나면 orphan lobby 상태가 남을 수 있다. AUTH와 같은 종료 경계의 별도 완료 경로이며 실제 재현은 미실행이다.

후속 최소 분석 대상은 AUTH의 `NonZoneHandler.cpp`·`StateManager.h/.cpp`, ENTER의 `MessageQueueHandler.cpp`·`LobbyZone.h`와 공통 StateManager 경계다. 기존 lobby→StateManager 호출과 Disconnect의 StateManager→lobby 호출을 고려해야 하므로 단순히 외부에서 shard 락을 감싸는 처방은 하지 않는다. 이번 리뷰에서 전체 수명 해결안을 설계하거나 이 파일들의 변경을 승인하지 않는다.

기존 생존 검사 `NetLibrary/ClientContext.h:108`의 IsSessionAlive(expectedSessionID), `SessionManager.h:148`의 GetSocket은 Net 내부에 있다. `CoreLib/IIOCP.h`에는 송신 함수만 있다. Net 검사 API의 존재만으로 현재 Core callback에서 안전하게 사용할 수 있다고 볼 수 없고, 검사 후 등록 사이 종료가 끼는 경합도 남는다. 전송 경계 확장이 필요하다면 정확한 추가 파일·동작을 후속 승인안으로 명시해야 한다.

## 유한한 후보 구현·회귀 범위

후보 개발 요청은 아래 다섯 파일만 명시하는 것이 타당하다. 현재 수정 허락을 의미하지 않으며 사람 승인 뒤 구현 담당이 적용한다.

1. `CoreLib/NonZoneThreadPool.cpp`: Process/Disconnect 호출 뒤 work reset 한 줄.
2. `CoreLib/NonZoneThreadPool.h`, `CoreLib/NonZoneHandler.h`: 동일한 좁은 friend fixture 선언 각 한 줄. 실제 pool 진입점과 concrete handler 초기화가 private이므로 pool 한 곳의 seam만으로는 부족하다.
3. `UnitTests/CoreLib/NonZoneThreadPool.cpp` 신규 및 `UnitTests/UnitTests.vcxproj` 등록.

실제 WorkFunc·실제 NonZoneHandler::Process·제어 가능한 ISessionAuth mock을 사용해야 한다. 비가상 concrete handler를 subclass mock으로 대체하거나 worker 루프를 복제하는 검증은 이 결함의 회귀가 아니다. 다음 enqueue 없이 handler 완료 후 bounded 구간 안에 Release가 한 번 발생하고, stop/drain 뒤에도 이중 Release가 없는지 확인한다. 기존 Context 마지막 참조·pending IO·정확히 한 번 Disconnect 회귀와 연결한다. 새 public API·가상 handler 계층은 필요 근거가 없다.

배포 판단 전에는 실제 callback을 보류하여 **AUTH 요청→종료 처리 완료→AUTH callback**, **ENTER 로드→종료 처리 완료→로드 응답**을 각각 재개하는 결정적 회귀가 필요하다. 종료 후 Core 세션/lobby 삽입이 없어야 한다. 현재 최소 seam만으로 이 회귀를 구성할 수 있는지 구현 단계에서 정확히 확인하며, 추가 seam이 필요하면 범위를 별도로 검토한다. 알려진 경합이 확인되거나 필요한 경계를 검사할 수 없으면 후보 결과를 보존하고 배포를 중단한다. 실패를 피하려고 기대조건을 약화하거나 완료 시각을 임의 지연하지 않는다.

## 운영·증거 한계

- 코드/fixture 후보 승인은 빌드·회귀까지만이다. 실제 서비스 종료·실행 파일 교체·배포·DB 초기화/복구는 포함하지 않는다. 운영은 정확한 당시 PID/경로/해시와 복구안을 갖춘 별도 승인이 필요하다.
- 현재 TCP 연결 0만으로 Zone의 잔존 세션이나 저장 완료를 보장하지 않는다. session15의 마지막 저장 요청이 없으므로 기존 정상 q 종료가 최신 상태 저장까지 보장한다고 가정하면 안 된다. 별도 운영안에서 잔존 상태와 최종 퇴장 snapshot/저장 확인·보존을 구체화해야 한다. HP·MP를 임의 복원하지 않는다.
- Cache/DB 작업자 사이의 저장 완료→다음 조회 순서는 이 reset만으로 보장되지 않는다. 향후 정상 disconnect 후 추가 접속 없이 저장 요청·SQL 저장값을 먼저 확인한 뒤 재접속 ENTER/첫 Full과 대조해야 한다. 다른 원인이 남으면 별도 최소 요청으로 분리한다.
- 이번 리뷰는 직접 소스·로그 읽기다. Coordinator의 읽기 전용 DB 결과는 전달 근거로만 참조했고 리뷰어가 DB를 조회하지 않았다. 수정 후 build/worker 회귀/비동기 종료 재현/실서버 재접속/Unity 입력은 모두 미실행이다.

검토 소스 SHA256: NonZoneThreadPool.cpp `32D0CF767907E2BD9CEEA3D9308D807810634DC921AF630DD5017DCD4CE71566`, NonZoneThreadPool.h `4DF0B96C6C778CA936418120C841A98F077486A62938D9063C3FE0B88DF643C0`, NonZoneHandler.h `CD43252484914072FB82020DDB66F5D68BB26D06EAD727274FD96A9EF0533B12`. 이번 리뷰는 후보 적용 전 기준이다.
