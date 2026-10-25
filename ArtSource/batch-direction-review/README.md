# 묶음 방향 비교 보드

`index.html`은 다음 세 작업을 A/B로 한 번에 비교하고 사용자가 조합을 전달하기 위한 로컬 시안이다. 추천만 표시하며 자동 선택·확정·전송·저장을 하지 않는다.

- 마을 입구→사냥터: 성곽길 정돈 / 숲 가장자리. SVG 평면 장식 시안이며 기존 충돌·통행·몬스터 AI 변경이 아니다.
- 가방: 그리드+오른쪽 상세 / 넓은 그리드+하단 상세. 기존 슬롯·수량·선택 상세만 표현한다.
- 거래소 판매: 목록+옆 판매 폼 / 목록→별도 판매 확인. 기존 가격·수량·골드 수수료·다이아 거래 역할을 유지한다.

## 출처와 사용 범위

지도·레이아웃·CSS·SVG는 이번 작업에서 직접 작성했다. 외부 게임 자산·폰트·이미지·유료 리소스와 이미지 생성 도구는 사용하지 않았다. 아이콘은 기존 `UnityClient/Assets/Resources/Items/item-00{1..5}-*-v1.png` 5개를 변경 없이 `media/`에 복사했다. 서버는 이 프로토타입 폴더만 공개하면 되며 저장소 상위 경로에 의존하지 않는다. 출처 기록 `docs/unity-client/ITEM_ART_SET.md`와 이력 `ITEM_ART_DRAFT.md`를 확인했다. 내장 OpenAI 이미지 생성 출력이며 별도 제3자 에셋 라이선스는 없다. 상업 이용 세부 조건은 이번 작업에서 별도 재검증하지 않았으며 CC0·독점권을 주장하지 않는다.

실제 항목 이름은 `C11ItemPresentation.cs`, 가방 표시 범위는 `C07InventoryScreen.cs`, 판매 수수료 및 재화 역할은 `C09BazaarScreen.cs`를 확인했다. 예시 가격100의 등록 수수료는 `max(10, price/10)` 기준10골드이며, 예시 잔액1,000에서990으로 표시한다. 화면의 모든 데이터는 예시다.

## 검증 범위

실제 Unity 구현·실입력·실서버 거래 통과를 나타내지 않는다. 실제 게임 카메라의 가림·성능·장식 배치도 후속 구현에서 확인해야 한다. Coordinator가 로컬 서버에서 데스크톱·모바일 화면과 아이콘 경로를 확인한다. Assets·Scene·Prefab·서버·DB는 수정하지 않았다.

## 복사 아이콘 SHA256

- `media/item-001-hp-potion-v1.png`: `1d17bdea7722aa953397a9f52c3af8bc0042cda57e5055d8feb6e949775b5579`
- `media/item-002-mp-potion-v1.png`: `2fbf7a7ccf66e4d2b0cf3264fe696e74f0cb96c520d3557e4b6e1ada4a7fa3b0`
- `media/item-003-iron-sword-v1.png`: `15f82f45d35894de912d825ad4699346bcc3af46a6bb11e39552ce2e3dabab6d`
- `media/item-004-iron-shield-v1.png`: `bf1d1d748883af775fd7e1ec3f947b4b4dfc149039602811cbcfae2300a79a2a`
- `media/item-005-leather-armor-v1.png`: `9ec34f4a89ec982ff52c0172519796943806a29c3900726c0adafa35f39a8668`

## Coordinator 실제 확인 — 2026-10-25

로컬 브라우저 viewport727px에서 세 구간/아이콘 표시·가로 넘침없음 확인. 이미지 complete/naturalWidth 모두정상. 테스트로 마을A/가방A/거래소B 선택→3줄요약을 확인한 뒤 새로고침하여 selected0으로 복원했다. 첫 자동화는 라벨 축약을 잘못 써 no-match였고 실제 DOM 라벨을 읽어 올바른 항목으로 확인했다. 이는 사용자 선택이 아니다. 별도 와이드/모바일기기, Unity 게임/UI 실행은 미검증. 비교 보드 서버는127.0.0.1:8792의 이 폴더만 제공한다. 캡처는 ROOT/UnityClient/Builds/Client-Resume/batch-board-preview.png.
