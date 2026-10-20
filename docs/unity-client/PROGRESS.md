## 등록 실패 처리 안전 보완 — 2026-10-20

불명 DB 결과에 자동 아이템·수수료 반환을 하지 않고, 확정 실패의 보상 실패와 worker 무응답을 구분하도록 고쳤다. 독립 리뷰에서 찾은 invalid 응답의 예외 전파도 보완했다. 후보 서버/테스트 빌드와 직접 native2/2 통과. 외부 등록 UI/TCP와 DB·경제·WAL 구조는 유지하며 실제 서버 교체·DB쓰기·Unity입력은0이다. 전체 등록 원자성/재시도/재시작 수렴은 여전히 미해결이다. [검증 기록](MARKET_REGISTER_SAFETY_RESULTS.md). 공개 기능 변화가 없어 기존 사이트/캡처·거래 등록 미지원 설명을 유지했다.

## 판매 취소·아이템 반환 연결 — 2026-10-20

내 판매중매물 확인→취소확정→반환확인을 기존서버규칙으로 연결했다. 응답이불확실할때가방캐시가남는문제와잘못된취소응답의오류전달을보완했다. 직접영향Unity2·native1최종PASS,실제한로그인에서본인매물0/취소비활성·정상퇴장을확인했다. 본인매물없어실DB취소·반환성공은미실행. 기존23696서버유지/새취소서버빌드만/재기동·seed0. [검증기록](MARKET_CANCEL_RESULTS.md).

# 2026-10-20 — 내 매물 대금 수령 흐름 구현·검증

내 매물 선택부터 서버 수령 결과 표시까지 연결하고, 리뷰에서 발견한 상태 표시·오류 전달 문제를 고쳤다. 실제 서버에서는 매물이 없는 상태와 비활성 버튼만 확인했다. 실지급은 fixture로만 검증했으며 실제 지급 요청·DB 쓰기는 없었다. 상세 [검증 기록](MARKET_CLAIM_RESULTS.md). 검증 대상은 기준 `0f9b96d` 위의 거래소 수령 변경이며 최종 Git 전달 상태는 HANDOFF.md에서 관리한다.

## 기존 콘텐츠 전체 편집 확장 검증 완료

스킬5종·몬스터3종 전체로 Unity 편집 범위를 넓혔다. 기존 에셋의 값과 GUID를 유지했고, 보스 스킬의 상속 기본값은 기존0/0으로 보완했다. 핵심 자동2개·이전 C++테이블 전필드 동등성1회가 통과했고 실제 편집 화면과 사이트 이력도 갱신했다. 실행 서버·DB는 유지했다. [결과](CONTENT_CATALOG_RESULTS.md). 새 묶음은 기존 PR에 합칠 게시 직전 승인을 별도로 요청하며 이전 WORLD01 승인은 재사용하지 않는다.
## 몬스터 배치 편집 묶음 검증 완료

셀별 몬스터 수·시작 간격을 Unity에서 편집하고 배치 미리보기·서버 데이터 내보내기를 연결했다. 기존 구역당241마리와 ID·배치 규칙을 유지했다. 독립 리뷰 후 exporter2개·실제 C++ 초기화1회·Inspector 수 변경/기본복원 한 흐름이 통과했다. 서버 소스는 스폰 초기화의 수치 연결만 바뀌며 실행 서버와 DB는 유지했다. [결과](SPAWN_AUTHORING_RESULTS.md). 관련 화면·문서와 PR 게시 직전 Slack 승인 정책을 한 로컬 묶음에 정리하며 새 작은 PR은 만들지 않는다.
## 사냥 상태와 콘텐츠 편집 묶음 완료 — 2026-10-19

서버의 레벨·원시 경험치와 몬스터 체력을 HUD에 연결했다. 실제 Ring 입력 뒤 몬스터 체력과 게이지 일치, 정상 퇴장 정리를 확인했다. 기울어진 게이지는 실제 필드 카메라를 연결해 해결했다. 사냥 자동3개·입력1흐름으로 종료했으며 처치/양수 EXP/레벨업은 이번 실행에서 관찰하지 않았다.

후속으로 Ring·일반 몬스터의 기본값을 Unity 에셋에서 편집·검증해 기존 C++ 테이블에 내보내도록 연결했다. 리뷰의 HP signed 상한을 보완했고 exporter2개·native 동등성1개 통과. 실행 서버·DB·원본 Game 변경 없이 후보를 검증했다. 사이트에 두 기능과 실제 캡처를 반영했고 같은 큰 PR로 전달한다. [사냥 검증](HUNTING_STATE_RESULTS.md), [콘텐츠 편집 검증](CONTENT_EXPORT_RESULTS.md).
## 2026-10-19 — 아이템·거래소 화면 개선 묶음 검증 완료

카드/매물행·선택상세·패널전환과 사이트 사례를 한 PR로 묶는다. 독립 리뷰의 재활성화 캐시 문제와 실제 캡처의 행높이를 수정했다. 최종 Unity98/98, 실제시장4/가방2응답·선택/스크롤·키차단·정상퇴장과 화면 보존을 확인했다. 가방 실제조회는기존서버실패이며성공카드는fixture만검증. 최종행높이52px영향확인완료. 사이트22파일/9HTML·원본캡처/ZIP·desktop/mobile탐색통과. [검증과 제약](ITEM_MARKET_RESULTS.md).

# 진행 기록

## 2026-10-20 — 거래소 구매와 구매품 수령 흐름 검증

가격 확인을 거치는 구매 요청, 명시적 구매품 수령, 수령 뒤 가방 재조회와 오래된 응답 차단을 연결했다. 독립 리뷰 차단 없음, Release 빌드와 직접 영향 Unity/C++ 검증 통과. 실제 실행은 재화 부족으로 구매가 거절됐고 사용 금액은 0, 이후 수령은 0건이었다. 처음 가방 조회는 캐시 준비 전 실패했으나 재조회에서 서버의 빈 결과를 확인했다. 실제 구매·지급·영속화는 검증하지 않았고 DB 쓰기·스키마 변경은 없었다. 서버 종료코드 0은 도구 출력에서 확인되지 않아 성공으로 기록하지 않는다. [상세 근거와 캡처](MARKET_PURCHASE_RESULTS.md). 검증 기준은 `20b365e2f935d86f72f3808600cb547f56374685` 기반의 로컬 후보이며, 최종 commit·push·PR은 미완료다.

## 2026-10-19 — C07 인벤토리 조회 후보 검증

가방 조회·재조회·슬롯/아이템 ID/수량 표시를 연결했다. 조회가 무응답이던 서버 캐릭터 귀속 누락과 실패 응답의 잔여 데이터 문제를 최소 보완했다. Unity57/57·서버31/31을 통과했고, 합성 입력 실패 후 실제 Windows 마우스로 로그인→필드→가방 조회/재조회/재열기→정상 종료와 캡처를 검증했다. 전용 캐릭터에 inventory DB 행이 없어 실서버 성공 목록은 미검증이며 실패를 빈 가방 성공으로 표시하지 않았다. 상세 [결과](C07_RESULTS.md). 최종 PR 반영 승인 전까지 원격 변경은 없다. 다음 Zone 전환은 계약 분석만 준비했다.

## 2026-10-19 — C06 전체 채팅 완료

기존 서버에 맞춰 구형 ClientCore 채팅/profile 형식을 고치고, 필드 채팅 입력·실제 수신 표시를 추가했다. 이름 조회 누락 시 멈춤과 폰트 수명 문제는 리뷰 중 보완했다. 회귀45/45와 두 계정의 실제 송수신·한글·입력 중 스킬 차단을 통과했다. 캡처에서 발견한 마지막 줄 겹침은 표시 영역만 고쳐 해당 화면만 재검증했다. 서버 제품 추가 수정은 없다. [검증·제약](C06_RESULTS.md), [독립 리뷰](C06_REVIEW.md).

Git은 기존 검증 조합을 첫 로컬 스냅샷a2296d9로 저장했고 C06을 별도 커밋으로 정리한다. GitHub push/PR/병합/공개는 아직 실행하지 않았다. 테스트는 작업 단위 완료 뒤 한 배치, 실패 보완 시 영향 범위만 확인했다.

## 2026-10-19 — 수정 서버 적용·재접속/외곽 이동 배치 통과

꺼져 있던 Unity·로그인·Slack 연결을 복구하고, 기존 로그/캐릭터 상태 보존 후 검증된 R01 수정 서버를 시작했다. 로그인→선택→필드→외곽 이동→정상 퇴장→무접속 DB 관찰→재접속을 한 배치로 실행했다. 캐릭터5011의 위치가 (39,39)에서 약(-3.426,-3.426)으로 저장됐으며 HP9975·MP10000과 함께 재접속 **첫 raw Full**과 일치했다. 새 맵 외곽 이동·HUD·캐릭터 화면 캡처2장을 직접 검수했다. [실행 요약](../../UnityClient/Captures/b05-batch-final.txt).

이전처럼 다음 게임 접속을 기다리지 않고 종료와 동시에 퇴장 큐에 들어갔고, 다음 접속 전 READ ONLY 행에서도 저장을 확인했다. SQL 완료 시각 자체는 계측하지 않았고 DB 관찰은 퇴장26.942초 뒤다. 이번 MP는 같은 값 보존 확인이며 새 스킬 비용 변화/HP0 영속·전체 맵 성능은 미실행이다. 정상 종료 시 기존 CancelIoEx1168 경고2개는 보존했고 기능 통과와 별도로 기록했다. 테스트 코드는 missing namespace1줄 보완 후 컴파일 통과, 실제 입력 재시도 없음. Play/입력/연결 정리 완료, 서비스는 유지했다. 아래 승인 대기/후보 미실행은 과거 이력이다.

## 현재 — R01-CLOSE-03 후보·회귀 완료 / 정상 전환 승인 요청

승인3제품파일·기존fixture를구현·독립리뷰하고격리UnitTests/참조lib/MainServer빌드exit0. 고정후보에서타깃10/10·기존Net19/19각1회통과, 이전늦은AUTH/ENTER2실패도기대완화없이통과했다. [실제 회귀](../../UnityClient/Captures/r01-close-candidate-regression.txt), [동결·빌드](../../UnityClient/Captures/r01-close-candidate-ready.txt), [제품 리뷰](R01_CLOSE_IMPLEMENTATION_REVIEW.md). 후보MainServer01C6261A…는미실행, 현재C0F689BD…서버와DB/Unity입력유지.

[정상 전환 요청](R01_DEPLOY_REQUEST.md)과 [도구 독립 리뷰](R01_DEPLOY_REVIEW.md)를준비했다. 전환도구F965C508…구문/native정의검증·불변읽기전용preflight경로통과근거(5DA실행)/새저장실패gate읽기검증을분리기록했다. q/start/복구/실HP영속은미실행. UnityCoordinator앱1792331719.054649에 `!coord 승인 R01-DEPLOY-04/r1 정상전환·조건부복구 재개` 요청. V04는전환·helper조건후B05로묶으며기수용명령을다시묻지않는다. 아래진행중표기는이력이다.

사용자가현재채팅에서정확한R01-CLOSE-03/r1 후보구현·회귀를직접지시해수용했다. Slack새승인회신은아직조회되지않아Slackts를만들지않고직접지시근거로진행한다. 승인3제품파일·기존fixture범위만구현/독립리뷰후고정exe로타깃·Net19를별도실행한다. MainServer는격리build-only/실행서버·DB·Unity입력유지. 착수bot1792330243.336369, V04는기수용/필요조건후B05. 아래결함재현/승인요청은이전단계이력이다.

후속 [R01-CLOSE-03/r1 요청](R01_CLOSE_REQUEST.md)·[제품3파일 후보](R01_CLOSE_03_candidate.patch)·[분석](R01_CLOSE_ANALYSIS.md)·[독립리뷰](R01_CLOSE_REVIEW.md) 준비완료/승인요청범위차단없음. 정상TCPclose범위 pending등록/기존entry완료 및 shard→lobby 삽입원자화, 기존AUTH응답/rollback보존을제안한다. 아직제품보완적용·새회귀없음. 격리MainServer는build-only까지요청하며실행/전환·DB·Unity입력은제외한다. 지정사람에게 UnityCoordinator앱으로 `!coord 승인 R01-CLOSE-03/r1 후보구현·회귀검증 재개`를요청한다.

승인한4friend·추가회귀2개를구현/격리빌드exit0/독립리뷰후별도tester가1회실행했다. 기존3pass·lateAUTH/ENTER2fail(exit1), 종료뒤AUTH는Core session(zone0/user6982)을되살리고ENTER는lobbycharacter를남겼다. 기대부재조건은유지했다. [실패 원문](../../UnityClient/Captures/r01-late-completion-regression.txt), [추가 회귀 리뷰](R01_LATE_COMPLETION_REVIEW.md). 이결과로배포차단을유지하고최소제품보완안을docs내후보로분석·독립리뷰중이다. 제품경합fix/서버전환/DB/Unity입력은미실행. V04는필요조건후B05로묶는다. 아래준비중표기는이력이다.

R01-REGRESSION-02/r1의 지정 사람 Slack 회신1792328764.170199를 V04스레드 전체 원문에서 확인했다. 정확한요청ID/r1·추가회귀재개를수용,네헤더friend/기존fixture확장·격리빌드·독립리뷰만진행한다. 구현자는제품경합수정/운영/DB/Unity에접근하지않고실제늦은응답후상태부재기대를검사한다. 실패를보존하고후속최소제품안은별도승인요청한다. 착수bot1792328859.683139. 아래요청대기는이력이다.

승인5파일 후보 구현과 독립리뷰 완료. 격리UnitTests빌드 exit0, 실제worker/handler 후보3/3 및 별도tester의 기존네트워크수명19/19 통과. [후보 근거](../../UnityClient/Captures/r01-candidate-regression.txt), [기존 회귀](../../UnityClient/Captures/r01-related-regression.txt), [독립 리뷰](R01_CANDIDATE_REVIEW.md). 초기LNK1101 실패를보존하고 후보build-only최종PDB생성제외로재빌드했다. 실행MainServer이미지C0F689BD…유지, 후보MainServer는미빌드/미실행이다.

늦은AUTH/ENTER완료회귀는추가접근점이필요해미검증·배포차단. 네헤더의같은fixturefriend각1줄·기존test확장만 [R01-REGRESSION-02/r1](R01_LATE_COMPLETION_REQUEST.md)로정리·독립검토했고제품경합수정/운영/DB를제외한다. Unity Coordinator 앱으로다음정확승인·재개명령을요청한다. V04는이미확인한사람재개지시를유지하며필요조건후B05로합친다. 아래진행중기록은이력이다.

사용자가 현재 채팅에서 R01 후보 구현·회귀를 명시적으로 재개했다. 승인된 제품5파일만 구현자에게 배정하고 실제 worker 반환·stop/drain 회귀 및 늦은AUTH/ENTER 검증 접근점을 독립 검토 중이다. 실행 서버 PID20024/9999·이미지 C0F689BD… 유지, DB/Unity Assets/Editor 조작 없음. Slack R01 사람 승인 회신은 조회되지 않아 채팅의 직접 지시를 근거로 기록했다. V04의 지정 사람 재개 회신1792327258.440959(`coord 재개 V04 실행검증`)은 전체 스레드에서 확인·수용했다. 느낌표가 없어 수신기 자동전달은 없었으며 재전송을 요구하지 않는다. V04 실입력은 필요한 R01/운영 조건 후 B05로 묶는다. 아래는 후보 재개 전 이력이다.

재접속 HP/MP/좌표 차이는 마지막 비Zone PacketView 보유로 종료·저장 요청이 다음 접속까지 지연되는 소스 경로와 실제 로그6사례(41~352초)로 좁혔다. DB는 READ ONLY로 두 전용 캐릭터만 대조했다. 후보 reset은 독립 리뷰상 타당하지만 늦은 AUTH/ENTER의 상태 재삽입 경합 때문에 배포는 차단. [정확한 후보 구현·회귀 승인 범위](R01_PERSIST_REQUEST.md)를 Unity Coordinator 앱으로 요청했고 보호코드는 아직 변경하지 않았다. [원인](R01_PERSISTENCE_ANALYSIS.md), [리뷰](R01_REVIEW.md).

월드는 곡선 길·비대칭 식생·낮은 폐허와 전역색/국소detail로 개편, compile0/refs·picking보존·정적리뷰차단없음. [최종 화면](../../UnityClient/Captures/v04-static/quarterview_10_10.png), [V04리뷰](V04_REVIEW.md). 실입력·성능·몬스터가독성은 미검증이며 Slack에 `!coord 재개 V04 실행검증`을 요청했다. R01은 `!coord 승인 R01-PERSIST-01/r1 후보구현·회귀검증 재개` 요청. 단계 완료는 다음 범위와 명령 요청으로 남기도록 운영 문서를 갱신했다.

## 이전 — C05 전투·HP0 / V03 완료

전투 키1/2/3·버튼과 서버 확정 HP/MP, 원자 행동 취소와 HP0 클라이언트 입력 차단을 구현했다. 독립 재리뷰 차단 문제 없음, Core/Unity 컴파일 오류0, 관련 PlayMode29/29. 실제 UI 로그인·서버 스킬/MP·Ring 피해·같은 몬스터 HP0/제거·15.26초 첫 alive Full/활성 visual, 계정2 raw HP0/HUD/비활성 버튼/실키·지면 입력 차단과 정상 disconnect를 확인했다. HP0 전용 phase 진입 때 이미0이어서 그 구간의 새 사망 전이는 미관찰이다.

전체 필드/외곽 지면·수목·바위·능선·중앙 저밀도 폐허를 보완하고, 계정2 실제 외곽 이동/서버-visual 대조/PNG를 확인했다. 실제 전투GIF11.76초도 검수했다. [종합 결과](../../UnityClient/Captures/c05-batch-final.txt), [HP0 결과](../../UnityClient/Captures/c05-final-batch-result.txt), [동결 SHA](../../UnityClient/Captures/c05-final-freeze.txt), [리뷰](C05_REVIEW.md), [전투 GIF](../../UnityClient/Captures/c05-attempt3/c05-combat-live.gif), [외곽 PNG](../../UnityClient/Captures/c05-final-account2-outer.png).

계정1 계획 밖 HP0/재접속 HP차이는 숨기지 않았다. DB복구 없이 재접속 freshHP9975/MP10000을 받아 저장/로드 원인은 미확인이다. fixture와 실제입력, 관찰·미관찰을 구분했고 helper의 UI/카메라/기본공격 실패를 보존·최소수정·재리뷰했다. HP0 서버보안·부활·새 애니메이션 통과를 주장하지 않는다.

Play/compile/dirty/runner/background=false·임시입력/Recorder/peer/게임연결0·기존서비스와 원본Game 변경 유지. 커밋/push/공개 없음. 각 단계는 Unity Coordinator bot의 C05/V03/W03 스레드에 보고한다. Slack 수신기 회귀7/7·인증·재연결은 확인했으나 실제 사람 회신 왕복은 미검증이며 수신 대기 중이다. 다음 기능/입력은 재개 지시 후 진행한다. [Slack 운영](SLACK_WORKFLOW.md), [인계](HANDOFF.md).

## 이전 — C04 완료

C04 타 플레이어·몬스터0/1/2 표시 구현, 독립 리뷰, 관련 PlayMode fixture22/22와 실제 B01/B02/B03 배치를 완료했다. 실제 로그인·목록선택·새서버진입, 서버 상태/표시 대조, 클릭이동·목표교체·HUD차단, 연결해제/재진입, 정상 peer 종료 뒤 관측만료, 중앙 AOI type0/1/2를 확인했다. 실제 PNG와 GameView GIF(6.14초)를 Coordinator가 검수했다. [최종 결과](../../UnityClient/Captures/c04-batch-final.txt), [fixture](../../UnityClient/Captures/c04-fixture-results.txt), [리뷰](C04_REVIEW.md), [실제 GIF](../../UnityClient/Captures/c04-recorder/c04-server-live.gif).

HP0 실전투와 월드 텍스트 focus 입력은 각각 미관찰/미검증이다. 기존 대표 지면 밖은 아트가 미완성이다. 최초 테스트 peer 잔여도 정상 stdin exit0 정리했고, 최종 Play/compile/background/runner=false·Recorder/peer/게임연결0·기존서비스 유지. 보호영역 추가 변경/커밋/push/공개 없음. 사람 승인 필요 요청은 현재 없다. 최신 권한·재개 상태는 HANDOFF.md를 따른다.

## 이전 진행 기록 — 아래 시점의 대기/보류는 최신 재개·완료로 대체됨
최신 C03: 단일Zone 본인 클릭이동 코드·Core/Unity컴파일0오류·EditMode직접순수6/6·독립리뷰네지적해소완료. 실제입력/서버이동은보류, INPUT_TEST_BATCH.md B01+B02에최신통합본으로묶었다. C03_TASK/REVIEW와 Captures/c03-final-freeze.txt가현재기준. C04는별도계약읽기조사완료/제품구현전, 삭제/AOI한계확인. 서버·DB·Scene·Art/운영추가변경·커밋/push/공개없음.


최신 작업 V02: 사용자가 장난감 같은 비율·장비·배경·UI 개정을 요청하고 실행 테스트 재개를 명시했다. V02_TASK.md 대표1scene 저작·동결·컴파일0·독립정적리뷰 차단문제 없음 완료. 최신 사용자 지시로 Unity 입력 테스트를 시작 전에 다시 보류하고 INPUT_TEST_BATCH.md B01에 V02 UI+새 AUTH 본인진입 공통 flow를 모았다. Editor r2 미리보기만 확인했으며 실제 진입/실행 캡처는 미실행이다. 프로젝트 동시2명 제한은 해제했고 단일파일/Editor 소유권과 도구 슬롯 상한은 유지한다. 아래 V01 실행 보류는 이전 시점 기록이다. V02 Slack 앱 스레드: 1792309178.902329.


기준 commit: `1d5a3d69074a1de6bafde30f1ec4dfe5cac1a951` (`codex/unity-ai-client`). C01 구현과 검증 대상은 이 기준 commit 위의 미커밋 변경이다. 작업 트리 전체를 하나의 commit으로 간주하지 않는다.

| ID | 담당 | 상태 | 결과 근거 | 남은 일 |
|---|---|---|---|---|
| C01 로그인·캐릭터 목록 | 구현·리뷰·테스트 역할 | 구현·독립 리뷰·Editor 실제 입력/Play 재시작 검증·대표 캡처 완료; Coordinator가 기록된 제약과 함께 기능 수용 | [작업 카드](FIRST_TASK.md), [서버 계약](SERVER_CONTRACT.md), [독립 리뷰](C01_REVIEW.md). Unity 6000.0.47f1, 로그인 3000·게임 9999 수신, 기존 MySQL·Redis 유지 ([환경 기록](../../UnityClient/Captures/c01-environment.txt)). ClientCore 최소 7파일과 Unity C01 UI/Scene/DLL 변경은 dotnet 및 Unity 컴파일 통과. 초기 NUnit 4개 중 3개 통과, 실제 UI 입력 fixture 1개 실패. 동일 UI 코루틴/assertion을 별도 Editor update driver로 실행해 실패 로그인→정상 재시도→TCP auth·실서버 목록 status=0/count=0 표시 일치→Scene reload 재로그인·중복 없음 및 비밀 검사 통과. 실제 Play stop→start 후에도 auth=true/list=1 및 동일 assertion 통과 ([초기 실행](../../UnityClient/Captures/c01-editor-input-first.txt), [재시작 실행](../../UnityClient/Captures/c01-editor-input-restart.txt)); 두 결과 모두 NUnitRunner 결과는 아니다. 대표 PNG [c01-character-list.png](../../UnityClient/Captures/c01-character-list.png) (1280×720)는 Coordinator가 status=0/count=0, 설명, 비밀번호 비움, 겹침 없음을 직접 확인했다. [c01-character-list-state.gif](../../UnityClient/Captures/c01-character-list-state.gif)은 Recorder 5.1.7의 GameView 36프레임을 6fps로 기록한 정적 결과 상태 캡처(1280×720, 6초)이며 로그인 전환 영상이 아니다 ([캡처 기록](../../UnityClient/Captures/c01-capture.txt)). 상세 실행 결과는 [테스트 기록](../../UnityClient/Captures/c01-test.txt). | NUnit UI runner의 입력 실패는 별도 Editor 실행 통과와 구분한다. 실서버 목록은 빈 계정만 검증됨; non-empty 목록은 fixture count 2 snapshot만 통과. 강제 partial-send 실험 및 실행 서버 바이너리와 서버 소스 빌드 일치는 미검증. |

| C02 캐릭터 선택·필드 진입 | 구현·리뷰·테스트 / Coordinator 실행 | 구현·최종 독립 리뷰·fixture10/10·C01 관련 회귀3/3·실제 목록/선택/서버 진입·Play 재시작·PNG/GIF 완료 | [작업 카드](C02_TASK.md), [리뷰](C02_REVIEW.md), [실행 기록](../../UnityClient/Captures/c02-test.txt). 승인 후 지정 계정 캐릭터 준비4행만 커밋. 실제 Mouse/GameView 문자 입력으로 status1/count1 본인행→선택→Enter1회→Zone1/일치 Full·서버(10,10)/캡슐/HUD를 확인. [첫 통과](../../UnityClient/Captures/c02-live-entry-first-pass.txt), [Play 재시작](../../UnityClient/Captures/c02-live-entry-restart-pass.txt). [실제 PNG](../../UnityClient/Captures/c02-live-entry.png), [실제 상태 GIF](../../UnityClient/Captures/c02-live-entry-state.gif)는6초 정적 진입 후 상태다. 이전 빈 목록/합성 자료와 실패 이력은 보존했다. | 기존 실행 서버에서의 C02 통과이며 새 AUTH 소유권 패치의 운영 통과가 아니다. 이동/전투/다른 캐릭터 렌더링은 범위 밖. |
| C02-AUTH-01 소유권 검사 | 구현·독립 리뷰 / Coordinator 빌드 | 승인된 두 줄·독립 리뷰·합성 SQL5/5·전체 로컬 빌드 exit0 완료 | [리뷰](C02_AUTH_REVIEW.md), [빌드 기록](../../UnityClient/Captures/c02-auth-summary.txt). 누락 의존성은 원본의 기존 정확 버전 package/lib/DLL만 읽기 복원, 설정·원본 변경 없음. | [운영 전환](C02_AUTH_DEPLOYMENT.md) r1 사람 Slack 승인·도구 정적 검사·[독립 리뷰](C02_DEPLOY_REVIEW.md) 완료. 초기 자동 검토 거절 후 같은 범위의 사용자 직접 승인을 받아 정상 전환 완료. 최종 도구 SHA1D1508… 실행에서 원본 정상 q 종료·새 작업 빌드 PID20024/9999/준비 완료 확인. 원본 실행 파일·로그인/DB/Redis 보존. 새 서버 Unity 회귀는 사용자 재개까지 대기. |

| V01 MMORPG 대표 화면 | 아트 저작·독립 정적 리뷰 | Edit Mode 저장·컴파일 오류0·참조 누락0, 실행 보류 | [작업 카드](V01_TASK.md), Art/README.md. 갑옷/망토/문장 영웅·석조 관문·산 능선·금속/양피지 UI. 실제 데이터/진입 게이트 유지. | 개정본 렌더·실제 UI·서버 진입·최종 캡처는 사용자 재개 후. 이전 V01 PNG는 최종 개정본이 아니다. |

결정: 사용자가 개발 시작/계속 진행·별도 승인 Slack 전달·변경별 새 스레드·Unity Coordinator 앱 발신을 지시했다. 이후 명시 승인으로 서버 보호 영역 두 줄 수정과 전용 캐릭터4행 생성만 적용했다. DB 스키마·기존 계정/비밀번호/재화/아이템 변경은 없었다. 정상 로그인·플레이 상태 저장은 발생했다. commit·push·merge·공개는 하지 않았고 원본 Game의 사용자 변경은 보존했다. HTML은 사용자 유보 지시대로 제작하지 않았다.

C02 실행 정리: Play/paused/compiling/runner/runInBackground=false, Recorder0, native Keyboard/Mouse만, 게임TCP established0. 기존 서비스 유지. Slack은 앱 sender U0C9ZRTQ6F8을 auth.test와 읽기로 확인했고 사용자 명의 발신은 중단했다. 수신기 Coordinator 설정은 그대로이며 새 변경 스레드 자동 재개는 미검증이다. 최신 운영 단계는 HANDOFF.md와 요청 문서를 따른다.

## 2026-10-19 — C08 Zone 1↔2 이동 검증 완료

기존 격자 이웃 구역으로 이동하고 새 서버 상태가 확인된 뒤 조작을 재개하도록 연결했다. 종료 경합과 늦은 이전 구역 패킷도 처리하고 좌표·HP/MP·쿨다운을 보존한다. 서버 39/39, 최종 영향 Unity 회귀 6/6, TCP 경계 3/3 통과. 실제 마우스 입력으로 저장된 Zone 1 재접속 상태 확인, Zone 1→2→1 왕복·추가 이동·정상 종료까지 통과했다. 초반 fixture·TCP 수신·종료 observer 문제는 조건을 유지해 수정하고 해당 범위만 다시 확인했다. 상세 결과와 실행 간 범위는 [C08 결과](C08_RESULTS.md).

C08 변경은 기준 `d752482` 위의 로컬 검증 결과다. GitHub 반영은 최종 승인 대기이며 실제 커밋·전달 상태는 HANDOFF.md를 따른다. 기존 서버 정상 종료에서 AcceptEx 경고가 재현되어 안전 중단 후 후보를 start-only로 실행했다. 경고 코드는 변경하지 않았고 DB 직접 변경도 없다. 최종 PR 생성 전 변경 요약·위험·복구안을 함께 Unity Coordinator Slack 스레드에 승인 요청한다.
