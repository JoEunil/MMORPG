# C10 내 매물 UI 독립 정적 리뷰

2026-10-19. 결론: **코드상 차단 문제 없음**. 최초 발견한 fixture 컴파일 2건과 한글 glyph 부재 1건을 Coordinator가 최소 수정한 뒤 영향 부분을 재검토했다. 컴파일·fixture 실행·실제 Unity 입력·화면 결과를 통과했다는 의미는 아니다.

## 범위

루트 AGENTS/HANDOFF/roles/reviewer 및 후보 C10_TASK/CONTRACT/REVIEW를 읽고 C09BazaarScreen, C09BazaarSetup, 신규 C10BazaarMyListUiTests와 meta, 기존 C09BazaarUiTests/C09LiveBazaarUiTests, C10BazaarMyListTests, ClientCore RequestBazaarMyList/GetBazaar/CancelBazaar와 응답 수명을 대조했다. 기존 C02WorldScreen/C05CombatScreen 입력 차단, TMP Settings와 실제 font/fallback asset도 확인했다. 제품·fixture·Scene·Editor·서버·Git·Slack 조작 없이 이 문서만 작성했다.

## 발견과 해소

- **fixture 컴파일:** C10BazaarMyListUiTests의 EventSystem namespace 누락과 ILogger 모호 참조를 보고했다. 최종 파일 13행의 `using UnityEngine.EventSystems;`, 247행의 `ClientCore.ILogger`로 해소된 것을 확인했다. 실행 컴파일은 배치에서 별도 확인한다.
- **폰트 표시:** Setup은 TMP_Settings.defaultFontAsset를 사용하지만 기본 LiberationSans SDF는 static atlas이며 baked Hangul 0개다. 연결된 dynamic fallback 역시 같은 LiberationSans.ttf를 사용하고 원본 TTF cmap의 Hangul 범위도 0개다. TMP Settings 전역 fallback은 비어 있다. 따라서 새 한국어 탭·상태·행·필터는 실제 글자를 표시할 수 없었다. 새 폰트/설정 추가 없이 SALES/MY LISTINGS/REFRESH, Trading/Sold/Claimed와 기존 Page/POTIONS/WEAPONS/ARMOR로 최소 교체하고 선택 표시도 ASCII `>`로 바꾼 최종 소스를 확인했다. 실제 glyph 화면과 경고는 실행 배치 확인 대상이다.

## 확인한 동작 경계

| 항목 | 정적 확인 |
| --- | --- |
| 탭과 늦은 응답 | SelectView가 기존 pending 취소 후 새 종류의 요청을 보낸다. ViewModel은 요청 ID와 Search/MyList 종류를 함께 확인하고 dispatcher 반영 때 수명·월드·pending을 재검사한다. 취소된 이전 탭 응답이 새 탭의 공통 callback을 실행하지 않는다. |
| timeout과 수명 | 닫기·8초 timeout·WorldChanged 비활성·ChangeClient·OnDisable에서 pending 취소와 화면 정리를 유지한다. Client 이벤트/버튼/월드 이벤트 구독과 해제가 대칭이다. 새 global timer/manager는 없다. |
| 검색 보존 | 내 매물 선택/갱신은 검색 itemType/page를 변경하지 않는다. 검색 복귀는 기존 두 값을 사용한다. MyList에서 필터·이전/다음·페이지 표시를 숨기고 검색으로 돌아오면 복원한다. |
| 서버 의미 | 반환 ID·아이템·수량·가격·등록 시각·상태를 표시한다. Sold는 판매됨 의미이고 claimable/정산 가능/금액을 합성하지 않는다. Claimed는 반환 상태 2의 표시다. 쓰기 기능·더미 행·판매 정책 변경이 없다. |
| 레이아웃 | 500×590 패널 안 탭 top58~92, 필터98~132, viewport top140/bottom142로 겹침이 없다. 하단 refresh x14~162, 이전 x214~282, 페이지 x258~334, 다음 x378~446 중 버튼끼리 겹침이 없다. 페이지 텍스트와 이전 버튼의 영역은 기존 구성상 인접/일부 교차하므로 실제 글자와 클릭 가능 영역은 배치 화면에서 확인한다. 세 줄 MyList 행은 기존 ContentSizeFitter/세로 ScrollRect/Mask로 높이를 늘리는 경로다. |
| 게임 입력 | 열기/조회에서 StopActions로 이동·예약 스킬을 정리한다. C05CombatScreen은 IsOpen 동안 스킬 입력을 거부하고 C02 이동은 GraphicRaycaster 결과를 먼저 확인한다. 실제 클릭·스크롤·1/2/3 입력 결과는 정적 리뷰로 대체하지 않는다. |
| 회귀와 fixture | 신규 2 case는 이전 탭 응답 무시, 반환 값과 Sold/Claimed 표시, 정산 가능 문구 부재, MyList pagination 비표시, 검색 필터/페이지 복귀 및 닫기 후 늦은 응답 무시를 검사한다. 기존 C09 assertion/응답 포맷은 유지한다. timeout/world 수명은 동일 공유 경로 및 기존 C09/UI·C10/Core fixture와 연결된다. 버튼 이벤트 직접 Invoke fixture는 실제 UI 입력 통과 근거가 아니다. |

새 불필요한 추상화나 제거해야 할 중복 상태는 발견하지 못했다. myListings는 현재 화면 종류, requestPending/deadline은 UI timeout을 나타내며 Core의 wire pending과 역할이 다르다. 기존 P28 서버 경로는 UI 변경 대상에 포함되지 않는다.

## 동결된 검토 파일 SHA256

| 파일 | SHA256 |
| --- | --- |
| UnityClient/Assets/C02/C09BazaarScreen.cs | FA09669D0D4EC3954F87E7A4ECC2A2C75678605EE6179129E0CD9E916FA95E1B |
| UnityClient/Assets/C02/Editor/C09BazaarSetup.cs | 078C9FA4CF8B3E033CC2D1C189FE20058ADAC3B4C7F7E3852C28281BF221F7CD |
| UnityClient/Assets/Tests/C02/C10BazaarMyListUiTests.cs | 61DAFA0B34EFA300FDFD71206279B0051E286E1E1FE9CDA47AFD8276C594FB11 |
| UnityClient/Assets/Tests/C02/C10BazaarMyListUiTests.cs.meta | EC791C7B792459C0EECB7D0A16ABF387E62EF314E7AAFC52F9003E42567A4358 |
| ClientCore/ViewModel.cs | 7872B05C335A8F8CDE46AE4571216A16C249684C0989C3B2D08BF8BF6844D472 |
| ClientCore/ViewData.cs | 826E8A52160111D1774589374147EFAB736546855C90A62498A4C8135F5795FD |

## 남은 배치

최종 DLL/Scene 연결과 동일 소스의 compile, 신규 UI 2 case 및 기존 C09·C10 영향 fixture, 두 전용 계정의 실제 MARKET→MY LISTINGS→REFRESH→SALES 복귀→닫기와 서버 SELECT 결과, 30행 fixture 스크롤·하단 버튼·폰트·게임 입력 차단을 한 동결 배치에서 확인한다. 실제 양수 행이 없으면 fixture와 실제 빈 조회 근거를 구분한다. 별도 작성 중인 C10 live observer는 이 리뷰의 동결 대상이 아니며 Coordinator 후속 검토 대상이다.
