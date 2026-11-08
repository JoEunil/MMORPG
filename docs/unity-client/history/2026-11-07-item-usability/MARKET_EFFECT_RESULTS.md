# 거래소 아이템 효과 표시 — 2026-11-07

- 사용자 요청: 같은 PR에 직접 관련된 추가 작업이 있으면 진행. 기존 등급 포션·장비를 구매/판매/수령 선택에서 비교할 수 있도록 표시를 보완했다.
- MarketScreen.ShowSelectedItem의 기존 수량 줄에 서버 콘텐츠의 HP/MP 회복량 또는 ATK/DEF 효과를 짧게 덧붙인다. 새 UI 객체/직렬화/네트워크/서버/경제 변경 없음. 미등록/효과없는 품목에는 값을 만들어 붙이지 않는다.
- 선택 가격·거래 처리 결과·확인 버튼 상태는 기존 경로를 유지한다. 선택 초기화에서 이전 효과도 함께 지워진다.
- 코드 리뷰 필수0. Unity컴파일완료. 직접 영향 기존2검사통과:선택효과/아이콘/새조회초기화/기존거래정보유지 및 수령함표시/읽기수명.
- 선택효과검사 최초1회는같은Listing77을반복생성해Transform.Find가지연삭제중인이전비활성행을선택하여실패했다. 각매물고유ID를주어실제현재행을선택하도록fixture를고치고실패한1항목만재검사했다. 제품실패가아니며수령함통과검사는반복하지않았다. 초기실패JSON도보존했다.
- 실제World씬별도PreviewScene으로최대수량65535·상급HP4500을배치해1280×720미리보기확인. 수량/효과줄190×22에서preferred130.24×15.65,overflowfalse. 가격과겹침없음. 실제매물/구매성공근거가아니다.
- 앞선등급포션3검사·서버빌드·가방배지/포션표시통과검사는변경없어반복하지않았다. 추가실서버/입력/Player빌드없음.
- 근거ROOT/UnityClient/Builds/ClientGrowthHUD/verification/item-usability/market-effect-test.json, market-receipt-effect-test.json, market-effect-layout.json, market-effect-preview.png. 이번추가도같은가방·장비·포션PR후보에묶는다.

## 종합 검수
동일Astra검수담당이실제미리보기와2검사근거를확인해통과/필수0판정. 최대수량/효과잘림·가격겹침없음. 기존게시요청을추가분포함r2로갱신하며미소비r1승인을r2로대체적용하지않는다. PreviewScene닫힘·Playoff/scene clean확인후Editor18568정상종료완료.
