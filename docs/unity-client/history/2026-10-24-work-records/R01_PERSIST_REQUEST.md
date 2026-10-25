# R01-PERSIST-01/r1 — 후보 구현·회귀 검증 승인 요청

Coordinator 소유. [원인 분석](R01_PERSISTENCE_ANALYSIS.md)과 [독립 리뷰](R01_REVIEW.md)의 범위다. 현재 서버 파일/DB/서비스는 변경하지 않았다.

승인 근거는 루트 AGENTS.md의 서버 보호 경계다. 그 지침은 “DB 스키마·인증/소유권·재화/거래 정책·새 프로토콜 계약·Zone/큐/락/풀·운영 작업과 원본 ClientCore 포괄 수정은 별도”로 정한다. 이번 수정은 큐·패킷 수명/종료 경계이므로 일반 콘텐츠 자율 변경에 포함하지 않는다. 이번 사람 승인은 플랫폼 도구 심사를 대신하지 않는다.

## 요청하는 정확한 범위

1. `CoreLib/NonZoneThreadPool.cpp`: 동기 handler 처리가 끝난 직후 `event.work.reset()` 한 줄을 추가해 다음 요청을 기다리기 전에 수신 버퍼 참조를 반환한다.
2. `CoreLib/NonZoneThreadPool.h`, `CoreLib/NonZoneHandler.h`: 실제 worker/실제 handler를 검증할 동일 `Core::NonZoneThreadPoolFixture`의 friend 선언만 각각 한 줄 추가한다. public API/가상 인터페이스/런타임 정책을 추가하지 않는다.
3. `UnitTests/CoreLib/NonZoneThreadPool.cpp` 신규 및 `UnitTests/UnitTests.vcxproj`의 해당 파일 등록: 실제 worker+handler와 기존 mock 경계를 써 다음 요청 없이 반환, disconnect 정확히 한 번, pending AUTH/ENTER 종료·늦은 callback 및 관련 수명 회귀를 유한하게 검사한다. 별도 복제 루프/실서버 네트워크/실DB는 사용하지 않는다. 추가 제품 seam이 필요하면 범위를 임의 확대하지 않는다.
4. 격리 빌드 출력 `UnityClient/Builds/R01-Candidate/` 등에 후보를 빌드하고 타깃 회귀를 실행·리뷰한다. 실행 중인 MainServer.exe와 현재 x64/Release 이미지는 덮어쓰지 않는다. 관련 기존 사용자 변경은 보존한다.

## 의도·위험과 중단 조건

실제 로그6사례에서 TCP 종료 후 이전 세션 상태 저장 큐 진입이 다음 접속까지 41~352초 늦었다. 마지막 Event가 PacketView를 보유하는 소스 경로와 부합한다. 저장 SQL/HP회복/부활 정책을 바꾸는 요청이 아니다.

독립 리뷰는 이 수명 후보를 **격리된 구현·회귀 검증 범위로만 조건부 진행 가능**으로 보며 안전한 배포 완료안으로는 차단한다. 참조 반환이 빨라지면 disconnect 뒤 늦은 AUTH/ENTER callback이 세션을 다시 만드는 경합이 드러날 수 있다. 이를 검증·기록하고, 실패하면 배포하지 않고 정확한 추가 최소안을 별도 요청한다. 단순 생존 선조회나 sleep으로 race가 해결됐다고 주장하지 않는다. 테스트를 약화해 통과시키지 않는다.

현재 실행 서버 교체·시작/종료·강제 종료·DB 갱신/초기화/복구·스키마·프로토콜·인증 정책 변경은 **이 요청에서 제외**한다. 배포는 필요한 보완·회귀·리뷰와 새 빌드 SHA가 준비된 후 별도 정확한 운영 승인을 요청한다. TCP 연결0만으로 잔존 Zone 세션/저장 완료를 단정하지 않는다.

## 복구

후보 검증이 실패하면 실행 서버를 유지하고 소스의 해당 한 줄/두 friend/신규 테스트 등록만 검토하여 되돌릴 수 있다. 다른 변경이나 DB를 과거 값으로 되돌리지 않는다. 후보 빌드도 실행 서버로 배포하지 않는다.

## 사람이 보낼 명령

등록된 R01 Slack 스레드에서 지정 사용자가 아래 문구로 회신하면 이 정확한 범위의 구현·회귀 검증을 재개한다.

`!coord 승인 R01-PERSIST-01/r1 후보구현·회귀검증 재개`

업무 승인은 관리형 도구/OS 심사와 별개다. 사람 회신 전 보호 대상 파일을 수정하지 않는다.

요청 발신: Unity Coordinator bot Slack1792326863.002079 (부모1792326328.900739).

재개 근거(2026-10-18): 사용자가 현재 채팅에서 “Slack에 승인했어. 확인하고 R01 후보 구현·회귀 검증부터 진행해줘”라고 이 후보 범위의 실행을 직접 지시했다. 이 명시적 지시에 따라 위 5개 파일의 후보 구현·격리 회귀를 진행한다. Slack connector의 thread/search와 bot conversations.replies(전체, has_more=false)에서는 지정 사람의 R01 회신을 확인하지 못했으며 수신기 deliveries도 비어 있다. Slack 승인 원문·회신 ID·자동 재개 왕복을 검증했다고 기록하지 않는다. 채팅에 중복 승인을 요구하지 않는다. 운영·DB·추가 보호 변경은 여전히 제외한다.
