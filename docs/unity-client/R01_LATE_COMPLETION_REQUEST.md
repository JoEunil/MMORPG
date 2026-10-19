# R01-REGRESSION-02/r1 — 늦은 AUTH·ENTER 종료 회귀 접근점 요청

Coordinator 소유. R01-PERSIST-01/r1 후보의 실제 worker 수명 회귀와 별개로, 배포 판단에 필요한 늦은 비동기 완료 경계를 검증하기 위한 추가 범위다. 아직 이 추가 보호 파일은 수정하지 않았다.

## 정확한 추가 범위

- `CoreLib/PacketWriter.h`, `CoreLib/MessageQueueHandler.h`, `CoreLib/LobbyZone.h`, `CoreLib/StateManager.h`에 기존과 같은 `friend class NonZoneThreadPoolFixture;`를 각각 한 줄 추가한다. public API·런타임 동작·인증 정책은 바꾸지 않는다.
- 기존 승인으로 만든 `UnitTests/CoreLib/NonZoneThreadPool.cpp`에서 fixture가 필요한 의존성만 준비하고 실제 handler/callback/로드 응답 경로를 유한하게 검사한다. StateManager의 lobby 의존성 초기화에만 접근하며 실제 Zone·전체 Core worker를 시작하지 않는다. 기존 등록 파일을 재사용한다.
- R01-Candidate의 프로젝트별 격리 출력에서 빌드·타깃 회귀·독립 리뷰한다. 기존 x64/Release 및 실행 이미지는 보존한다.

## 필요한 이유와 검증

두 기존 friend만으로 worker와 NonZoneHandler를 준비할 수 있다. 성공 AUTH callback에는 PacketWriter의 packet pool 초기화가 필요하다. ENTER 응답에는 MessageQueueHandler·LobbyZone 초기화, 이미 인증된 zone0 session의 실제 Disconnect에는 StateManager의 lobby 의존성 초기화가 필요하다. 공개 Initializer는 Zone/recv/broadcast/chat/perf 등 전체 topology를 시작해 이 좁은 회귀보다 비용·영향이 크다. private 우회 매크로·타입 재정의·메모리 접근 트릭은 사용하지 않는다. 독립 검토자가 이 네 경계를 확인했다.

기대 결과는 (1) AUTH 요청→패킷 참조 반환→종료 처리 완료→보류 성공 callback 뒤 Core session이 없어야 함, (2) 인증 session의 ENTER 요청→종료 처리 완료→보류 성공 로드 응답 뒤 session과 lobby character가 없어야 함이다. 실제 코드 경합이 드러나면 실패를 보존하고 배포를 차단한다. 실패를 통과하도록 기대를 완화하거나 sleep을 넣지 않는다. 현재 후보만으로 이 경합이 해결됐다고 주장하지 않는다.

## 제외·복구

실패 원인의 제품 수정·새 인증/소유권 정책·프로토콜·DB/운영·서버 시작/종료/교체·Unity 실제 입력은 제외한다. 실패하면 정확한 최소 제품 보완안을 별도로 분석·리뷰·요청한다. 복구는 이번 네 friend와 추가 테스트 부분만 되돌릴 수 있으며 R01 기존 후보·다른 변경·실행 서버는 보존한다.

## 지정 사용자 명령

Unity Coordinator 앱의 R01 스레드에서 다음 정확한 추가 범위를 승인·재개한다.

`!coord 승인 R01-REGRESSION-02/r1 추가회귀 재개`

Unity Coordinator bot 요청1792328185.787879(부모1792326328.900739) 발신 완료.

승인 확인: 지정 사람 U0C9QEUHPMK가 V04부모1792326340.552749의 회신1792328764.170199에 `R01 스레드의 !coord 승인 R01-REGRESSION-02/r1 추가회귀 재개`라고 답했다. Coordinator가 search와전체thread에서 원문·요청ID/r1을 대조해 이 정확한 범위를 수용했다. 최신 채팅도 같은 회귀 재개를 직접 지시했다. 앞에 문장이 있어 자동수신 필터에는 걸리지 않았으며 수동 확인/수용으로 기록하고 같은 회신을 중복 실행하지 않는다. 추가 보호 범위나 운영 승인을 뜻하지 않는다. 플랫폼 도구 심사는 별도다.

실행 완료: 네 friend와 기존 fixture 확장2개·격리빌드exit0·독립리뷰 후 별도 tester가 고정F60BFB7D…를 한 번 실행했다. 기존3 PASS/늦은AUTH·ENTER2 FAIL(exit1). 정상 Disconnect 완료 뒤 AUTH는 Core session을 재생성하고 ENTER는 lobby character를 남겼다. [실패 원문](../../UnityClient/Captures/r01-late-completion-regression.txt)을 보존했고 기대/제품을 고치거나 재실행하지 않았다. 제품 보완은 별도 [R01-CLOSE-03/r1](R01_CLOSE_REQUEST.md) 요청으로 준비한다. 실서버 전환/SQL/Unity 검증은 여전히 미실행이다.
