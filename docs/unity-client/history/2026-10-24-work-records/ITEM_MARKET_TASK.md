# 아이템·거래소 화면 개선

목표: BAG과 MARKET을 실제 게임 UI로 읽고 사용할 수 있는 한 묶음으로 완성한다. 관련 클라이언트 UI·회귀·실입력·캡처·사이트 기록·운영 지침을 한 브랜치/한 PR로 전달한다. 작은 탭·수정·문서만으로 PR을 분할하지 않는다.

기준: master449a3ef에서 codex/item-market-ui를 만들고, 승인된 기존 MyList6383ac5를76f03c0으로 통합했다. 이전 PR33은 codex/unity-c09-delivery에 병합돼 master에는 없으므로 이번 PR에서 누락을 해소한다. C10 검증 결과는 보존한다.

포함: 인벤토리 실제 아이템의 카드/선택 상세, 거래소 판매 검색·내 매물의 읽기 쉬운 행/선택 상세, 기존 유형·페이지·새로고침·닫기/실패/빈 결과, 두 패널 동시 겹침 방지, 기존 입력 차단과 수명/늦은 응답 경계 유지. 아이템 이름은 CoreLib/ItemData.h의 기존5종(HP/MP 포션·철검·철방패·가죽갑옷)을 영문으로 표시하고 미확인 ID는 Item #ID로 표시한다. 가격은 원본 Price로 표시하며 단가·합계·통화·수령 가능 여부를 추정하지 않는다.

제외: 새 서버·ClientCore 계약, 거래/등록/구매/취소/수령, 장착/사용/드래그, 재화/스키마/DB seed, 서버 IOCP 종료 경로, 새 패키지·폰트·가짜 아이템/능력치/희귀도. 빈 데이터 화면에 더미를 채우지 않는다.

소유권: Coordinator는 공통 ItemPresentation, 정책/작업카드/사이트/최종 Editor·Scene·통합을 맡는다. 인벤토리 담당은 C07InventoryScreen/Editor C07InventorySetup/해당 UI fixture, 거래소 담당은 C09BazaarScreen/Editor C09BazaarSetup/해당 UI fixture를 맡는다. 서로 다른 파일만 수정하고 테스트 중에는 Assets/Packages/ProjectSettings 저작을 멈춘다.

공통 최소 계약: C11ItemPresentation.Name(uint)/Badge(uint)/Accent(uint)를 공유한다. 두 화면의 ClosePanel()을 public으로 열고, inventory는 serialized C09BazaarScreen bazaarScreen, market은 serialized C07InventoryScreen inventoryScreen을 통해 열 때 상대 창을 닫는다. 기존 예외/취소/timeout/world/접속 이벤트를 재사용한다. 새 Manager/인터페이스/이벤트버스/범용 UI 프레임워크를 만들지 않는다. 기존 giant rows text를 디버그 상태 저장용으로 숨겨 두지 않고 실제 행/카드 컴포넌트의 표시를 테스트한다.

완료/검증: 저작과 독립 리뷰 후 동결하여 UI fixture(실제 서버값·미확인ID·선택·0/1/30행·취소/지연/검색조건/패널전환)와 직접 영향 회귀를 한 배치로 수행한다. 실제 전용 계정으로 로그인→MARKET 검색/스크롤/상세→MyList/새로고침→BAG/새로고침→MARKET복귀→닫기→정상퇴장을 한 번에 확인하고 캡처한다. 실제 소유 아이템·매물의 양수행이 없으면 해당 화면은 fixture 근거와 구분한다. 예상 자동수초/컴파일수분/입력수분, 통과 뒤 반복하지 않는다. 큰 UI 재구성으로 기존 관찰 helper가 텍스트 형식에 묶인 부분은 읽기 관찰만 실제 행 구조에 맞춰 갱신하고 리뷰한다.
