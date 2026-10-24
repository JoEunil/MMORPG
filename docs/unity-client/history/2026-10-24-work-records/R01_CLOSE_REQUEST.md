# R01-CLOSE-03/r1 — 정상 TCP 종료 뒤 AUTH·ENTER 재삽입 방지 후보 요청

Coordinator 소유. [실패 기록](../../../../UnityClient/Captures/r01-late-completion-regression.txt), [원인·최소 후보](R01_CLOSE_ANALYSIS.md), [후보 patch](../../R01_CLOSE_03_candidate.patch), [독립 리뷰](R01_CLOSE_REVIEW.md)를 근거로 한다. 아직 이 제품 보완은 적용·빌드·실행하지 않았다.

루트 AGENTS.md는 “DB 스키마·인증/소유권·재화/거래 정책·새 프로토콜 계약·Zone/큐/락/풀·운영 작업과 원본 ClientCore 포괄 수정은 별도”라고 정한다. 이번은 인증 완료·세션 잠금 경계의 제품 변경이며, 앞선 R01-REGRESSION-02/r1의 테스트 접근점 승인에 포함되지 않는다.

## 확인한 결함

추가 회귀는 격리 빌드·독립 도구 리뷰 후 고정 exe로 한 번 실행했다. 기존3 통과·늦은완료2 실패(exit1). 정상 Disconnect 완료 뒤 AUTH 성공 callback이 session42(zone0/user6982)를 다시 만들고, ENTER 성공 로드 응답이 lobby character를 남겼다. 기대값을 완화하지 않았으며 실행 서버/DB/Unity는 유지했다.

## 요청하는 정확한 범위

1. `CoreLib/StateManager.h`: 현재 AddSession을 BeginAuth/CompleteAuth로 나눈다. 실제 AUTH PacketView를 보유하는 동안 pending entry를 먼저 등록하고, callback은 같은 기존 session shard lock 아래 존재하는 entry만 완료한다. 완료된 실패와 pending을 구분하는 내부 bool 하나를 둔다. 첫 callback이 정한 UID/인증 결과는 이후 callback이 덮어쓰지 않는다. 살아 있는 기존 entry의 실패/중복 callback 응답 정책은 유지한다. `friend class LobbyZone` 한 줄로 아래 기존 두 클래스의 잠금 연계를 허용한다.
2. `CoreLib/NonZoneHandler.cpp`: 외부 AUTH 요청 전에 BeginAuth, callback에서는 CompleteAuth가 entry 부재를 반환하면 상태 삽입/응답 없이 끝낸다. 존재하면 기존 resStatus 응답을 유지한다.
3. `CoreLib/LobbyZone.h`: ImmigrateChar에서 session shard → lobby 순으로 잠근다. 같은 잠금 구간에 존재·인증·zone0을 확인하고 삽입한다. 기존 SetZoneID(0) 중첩 재획득은 제거한다. MQ 응답과 Zone 진입 실패 rollback의 기존 호출은 그대로 사용한다.
4. `UnitTests/CoreLib/NonZoneThreadPool.cpp`: 기존5개 부재 기대를 유지하고 AUTH pending/성공/실패/중복/첫 UID 유지·userID0, 정상 완료 후 종료 및 종료 후 완료 양순서, lobby 중복·rollback 경계 등을 실제 기존 handler/callback 경로와 필요한 최소 component 경계로 검증한다. component rollback 검사는 실제 ZoneChange 전체 실행과 구분한다. 새 제품 seam·다른 제품 파일이 필요하면 임의 확대하지 않는다.
5. 기존 `UnityClient/Builds/R01-Candidate/`의 프로젝트별 격리 중간 출력과 후보 출력에서 UnitTests/참조 라이브러리 및 **MainServer 후보를 빌드만** 한다. 독립 리뷰한 고정 exe로 타깃 및 관련 Net19 회귀를 유한하게 실행한다. 실행 중 x64/Release/MainServer.exe는 덮어쓰지 않는다. 테스트/후보 산출물 해시와 실패·통과·미실행을 txt로 남긴다.

정확한 제품 patch SHA256: `E3D832EA214D27FC0985816E8E91E527E38BDABD47FB637BFB1ED687ADCA7D7B`. 분석 문서 SHA256: `8085429BE78369EC837DC188FDC6AC13BFC08ADC378E65B62C41CEDBE8E78CE3`. 이 patch의 read-only `git apply --check`는 통과했으며 실제 적용은 하지 않았다. 테스트 확장은 위 관찰 범위에서 구현한다.

## 영향·한계·종료

새 wire/SQL/외부 인증 계약·DB 데이터·재화·서비스는 바꾸지 않는다. pending은 미인증이며 현재 Net 필터의 AUTH/PONG 허용을 유지한다. 기존 첫 callback 상태 고정 정책 때문에 실패 후 성공 재AUTH의 응답과 저장 인증 상태가 다를 수 있는 기존 동작은 이번에 고치지 않는다. 없는/미인증/zone0이 아닌 entry의 lobby 삽입은 거부한다.

보장 대상은 **정상 TCP 종료 통지와 늦은 응답**이다. transport를 닫지 않는 직접 Core Disconnect 후 같은 transport 재AUTH의 세대 구별, 일반 Zone lock 역순·중복 ENTER/ZoneChange 전체 원자성, SQL update 완료→load 순서, 서비스 종료 중 외부 callback 수명은 별도 잔존 위험이다. 이 후보 통과를 일반 수명 안전·실서버 HP 보존으로 확대하지 않는다. 관련 위험이 이 좁은 범위의 진행 자체를 막으면 근거를 보존하고 추가 요청한다.

비용/종료: 기존 2초 내부 gate와 60초 외부 실행 제한, 타깃·관련 회귀 1회씩. 신규 실패/변경 없으면 반복하지 않는다. 실패를 보존하고 같은 원인 수정2회 한도를 따른다. 후보 구현·독립 리뷰·격리 회귀가 끝나면 별도 운영 조건과 정확한 다음 승인·재개 명령을 준비한다.

## 제외·복구

실행 서버 정상/강제 종료·시작·교체·MainServer 후보 실행·배포·DB 갱신/복구/초기화·Unity 입력·push/병합/공개는 제외한다. V04 입력은 필요한 보완·검증·운영 조건 이후 B05로 묶는다.

복구는 이번 세 제품 파일의 정확한 hunk와 새 테스트 확장만 되돌리며, 기존 reset·승인된 fixture friend·다른 사용자 변경·실행 이미지·DB는 보존한다. 실행 서버 전환은 현재 PID/이미지·잔존 상태/저장·복구를 확인한 별도 운영 요청이다.

## 지정 사용자 명령

Unity Coordinator 앱의 R01 스레드에 아래 명령으로 이 정확한 후보 구현·격리 검증 범위를 승인·재개한다.

`!coord 승인 R01-CLOSE-03/r1 후보구현·회귀검증 재개`

독립 후보 리뷰와 이 정확한 요청서의 범위 검토를 완료했고 승인 요청 범위의 코드상 차단 문제는 없다(R01_CLOSE_REVIEW.md). 아직 이 제품 보완에 대한 사람 승인은 없으며 제품 적용·새 후보 실행 성공을 뜻하지 않는다. 플랫폼 도구 심사는 별도다.

Unity Coordinator bot 발신1792329612.955889(부모1792326328.900739): https://aiagent-vqu1541.slack.com/archives/C0C9VFZMV43/p1792329612955889 . 사람 승인 전 추가 제품 보완은 적용하지 않는다.

재개 근거: 최신 사용자 채팅 “Slack에 R01-CLOSE-03/r1 승인했어. 확인하고 후보 구현·회귀 검증부터 진행해줘”는 이 정확한 범위의 구현·검증을 직접 지시한다. 해당 지시를 수용해 진행한다. Slack검색/R01·V04전체thread/bot전체replies에는새R03사람회신이조회되지않았으며Slack승인회신ID를확인했다고기록하지않는다. 채팅에중복승인을요구하지않고허용범위밖운영·DB·Unity는유지한다.

후보 단계 완료: 승인patch3제품파일·기존fixture확장동결·독립리뷰차단없음·격리UnitTests/MainServer빌드exit0, 별도tester타깃10/10·Net19/19각1회통과. 이전late2도부재기대유지통과. 후보MainServer01C6261A…는미실행이며정상전환은별도R01-DEPLOY-04/r1로구체요청했다. 후보검증을실SQL/재접속HP보존성공으로확대하지않는다.
