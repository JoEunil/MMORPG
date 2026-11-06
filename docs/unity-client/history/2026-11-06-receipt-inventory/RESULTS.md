# 아이템 사용·거래 수령 개선 — 2026-11-06

## 범위
- 처치 HP/MP 포션보상 제거는 기존 로컬93a2e93 포함. EXP·골드·보유포션사용 유지, 확률드랍계획 추가 없음.
- 귀환/구역이동 직후 포션수량을 다음 준비 프레임에서 자동 읽기. 이전 읽기 응답은 소비 요청으로 바꾸지 않음.
- 거래 아이템 수령함: 구매·취소반환 목록과 명시적 전체수령. 판매취소 즉시반환/판매대금은 기존 동작 유지.
- 사용자 몬스터AI·DB스키마·프로시저 변경 없음. 원본Game/refactor 변경 없음.

## 계약
TCP51 요청12바이트(reqIDu32), TCP52 응답17+24*n 최대737바이트(reqIDu32/statusu8/processingCountu16/countu16; row eventIDu64/listingIDu64/itemIDu32/quantityu32). 상태0실패/1조회스냅샷/2캐시준비중. 기존 SQL18 READY 최초30행과 inventory 최근event 전체를 대조, 반영된행은 처리중 수로 분리. 처리중행도 최초30 한도를 차지하므로 빈목록은 전체완료 증명이 아니다. 조회는 DeliverItem/CLAIM업데이트를 호출하지 않는다. 지급은 기존 CheckOutbox/중복방지/flush가 담당하며 자동재시도하지 않는다.

## 검토와 수정
Astra설계자문은 단순READY목록/캐시대조/일반우편확장을 비교해 캐시대조 최소안을 추천. 캐시 Getter의 EMPTY 뒤 다른로더가 AVAILABLE이 되는 경우 실제data 재복사 보완. 독립코드리뷰 발견3건: 최대737응답을512 Normal에 쓰던 문제→Big전환+최대30검사; 기존BazaarListing Pack1복구; 기존결과안내assertion수정. 재검토 코드차단0.

## 자동 확인
- ClientCore Release 성공, 오류/경고0. Unity 컴파일 오류/경고0.
- MainServer/UnitTests Release 성공(기존C4244/C4267 경고 존재).
- 서버 관련2개 Passed: cache READY/applied/busy/실패경계, wire/auth/latebinding/Big30행737/31거부.
- Unity PlayMode 관련7개 Passed: 수령함조회/닫기후쓰기2, 포션도착2, 새wire/취소1, 기존39바이트목록1, 변경수령안내1. 기존통과검사는 반복하지 않고 변경영향으로 마지막2개만 선택.
- 동기PlayMode명령은 HTTP 도메인리로드 제한으로 실행되지 않았으며 성공으로 세지 않았다. 비동기로 재요청해 실제완료를 확인했다.
- Unity CLI beta16→17은 사용자승인으로 업데이트. 기존Pipeline 구명령 안내는 기록만 하고 패키지 변경안함.

## 실제 실행
최신 Windows Player 빌드성공(오류0·기존경고5: TMP obsolete4, RuntimePipelineConfig없음1). TradeReceipt 서버/MainServer·로그인·Player 시작성공. 사용자입력 감지로 클릭을 멈추고 현채팅에 허용질문을 남겼다. 실제새수령함 로그인/클릭은 미실행이며 자동fixture를 실입력으로 주장하지 않는다.

## 근거
ROOT/UnityClient/Builds/ClientGrowthHUD/verification/receipt-inventory: server-build-final.log, server-tests.xml, ui-receipts-tests.json, potion-arrival-tests.json, receipt-wire-test.json, existing-list-wire-test.json, receipt-feedback-test.json, player-build.json.
새Slack 부모1793935273.767879. HUNTREWARD01별도PR요청은 철회유지.

## 마감 상태
코드후보9a68eea에 Astra 권고문구 추가: 첫BLOCKED에서멈추는 수를 전체잔여수처럼 표시하지 않고 Some items remain unreceived로 안내. assertion문구만 일치시켰으며 게임로직불변이므로 이미통과한검사는반복하지않았다. 추가 Unity컴파일오류0. Astra는 코드·자동검증 범위 통과/실제화면근거부족으로 구분했다. 입력허용답변대기로 새탭배치/실제클릭미실행. 테스트Player/MainServer/Login정상종료, launcher stopped. 최종문구Player갱신빌드성공(오류0·기존경고5), player-build-final.json. 원격fetch 결과master9f12768변동없음, 브랜치 codex/trade-item-receipts로이름정리, push/PR없음.

## 후속 실제 입력·최종 검수 — 2026-11-06
사용자가 현채팅 질문에 지금게임화면확인진행으로 명시허용했다. 기존TradeReceipt 시작→보호정보를 메모리에서만 읽어 정상게임UI test1로그인→char11/Lv4 마을1(10,10)진입. 가방을열지않고 HP포션22/MP포션2, 골드100020·다이아750자동표시. 거래소클릭→4탭확인→ITEM RECEIPTS조회:0ready/0processing/최대30. RECEIVE ALL명시클릭:Received0/already present0 후 목록재조회. 가방확인HP22/MP2유지. 새양수배송·실제구역이동은 수행하지않았으며 위fixture범위와구분. 경제변경/DB수정없음. 실제화면 trade-receipts-live.jpg와 공개 trade-item-receipts.jpg. 같은Astra가 실제이미지를열어 배치·가독성·빈목록/0응답표현 검수통과/차단0. 예전대기기록은 시간순이력이며 현재대기가아님. 종료도우미로 Player/MainServer/Login 모두정상종료, launcher상태stopped. 최신 Player빌드0오류5기존경고. 게시준비완료, Slack게시승인만대기.

TRADEITEM01/r1 게시요청1793936786.718919, 부모1793935273.767879, 후보8da7615. 수신기pid43884/listening·최근health·고정Coordinator·등록스레드확인. 지정사용자회신대기/미소비. 완료채널1793936787.372659. push/PR게시없음.

2026-11-06 직전TRADEITEM01/r1요청에사용자현채팅승인수신. 현채팅승인으로범위일치확인·게시1회적용, Slack회신ID없음. 원격fetch후master9f12768변동없고작업트리clean. 승인된후보+인계문서만게시하며새기능추가없음.
