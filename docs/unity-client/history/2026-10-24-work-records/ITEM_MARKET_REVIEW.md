# 아이템·거래소 UI 독립 리뷰

2026-10-19. 대상: C10-Delivery/source, codex/item-market-ui. 제품 코드·Scene·Editor·서비스·Git 조작 없이 파일만 읽었다. 최종 실행 결과를 대신하지 않는다.

## 판정

**코드상 차단 문제 없음.** 최초 P2 1건은 최소 수정 후 영향 재리뷰로 해소됐다. 아래 P2는 수정 근거의 이력이다. 현재 범위에서 추가 차단 결함이나 불필요한 프레임워크는 발견하지 못했다.

### P2 — 인벤토리를 재활성화하면 성공 캐시만 남고 카드가 사라짐

- 위치: `UnityClient/Assets/C02/C07InventoryScreen.cs:310`, 같은 파일 `ChangeClient` 75행, `TogglePanel` 112행.
- 재현: BAG 열기 → 양수 아이템 응답 성공 → C07InventoryScreen 컴포넌트 disable → 같은 로그인 모델을 유지한 채 enable → BAG 열기.
- 원인/영향: OnDisable이 실제 카드를 삭제하지만 `hasInventoryResponse`는 true다. 재활성 시 `model == next` 분기는 캐시 플래그를 유지하며, TogglePanel은 이미 성공했다고 판단해 요청을 보내지 않는다. 이전 성공 메시지와 빈 카드가 같이 표시된다.
- 최소 수정: 카드 삭제 시 성공 캐시를 무효화하거나 재활성 시 서버 snapshot에서 카드를 복원한다. timeout/미완료 요청의 재접속 제한은 보존한다. 같은 모델 재활성 회귀 1건을 배치에 포함한다.

## 확인한 범위

- 서버 ItemData.h의 기존 5종과 C11ItemPresentation 이름/배지/색의 대응. 미확인 ID는 Item #ID이며 능력치·희귀도·통화·가격 단위·수령 가능 여부를 만들지 않는다.
- BAG 카드 정렬/선택 상세는 InventoryItemView 값 복사, MARKET 행/선택 상세는 BazaarListingView struct 값 복사에 기반한다. 거대한 rows 문자열이나 별도 숨긴 중복 목록은 없다.
- MARKET 요청 취소·timeout·늦은 응답·탭 전환 경계는 기존 request ID를 사용하는 모델에 위임한다. refresh와 실패는 실제 행과 선택을 지운다. Trading/Sold/Claimed를 구분하며 Sold를 수령 가능이라고 바꾸지 않는다.
- 두 화면의 public IsOpen/ClosePanel 계약이 실제 존재하고, 서로 열 때 상대 ClosePanel을 호출한다. 두 Setup의 serialized 필드 이름은 runtime과 대응한다. Combat InputAvailable에 BAG/MARKET 모두의 입력 차단이 있다.
- Setup은 viewport Image+Mask, ScrollRect, content layout/fitter를 연결한다. 실제 Canvas/GameObject에 적용한 결과와 30행 스크롤은 실행 배치에서 확인해야 한다.
- 기존 UI fixture의 서버값·취소·지연·timeout·타입/페이지·빈/실패 assertion은 삭제하거나 완화해 성공으로 바꾸지 않았다. 문자열 관찰은 실제 카드/행/선택 상세 관찰로 바뀌었다.
- C11LiveItemUiTests는 실제 모델 이벤트와 UI를 읽고 결과를 기록한다. 버튼 호출·가짜 응답 주입·모델 상태 변경은 없다. 내부 반사는 읽기만 한다. 실제 입력과 서버 응답 성공/실패를 구별하며 서버 위치·HP/MP·자기 스킬 결과를 관찰한다.
- UI 타입에 실제 모호성을 만드는 추가 using은 확인되지 않았다. 새 패키지·폰트·Manager·인터페이스·이벤트 버스는 없다. Scene 변경 및 DLL 배치 일치는 Coordinator 최종 통합 검증 항목이다.

## 최초 동결 SHA256

```text
7B36C945D4E26CB44774F613789E235C39BBB099DE6EED66D46A6CBC04B6AAE5 C07InventoryScreen.cs
2CD69BC93CB518602C973A4B11CD31E0D9E87CED6CAACD91600ABD2CA3A0B20F Editor/C07InventorySetup.cs
53D7AA74A4BB739E54DBC98D2A865EA2F14857B61ECD7EB7489C30ABF64E1208 Tests/C07InventoryUiTests.cs
797B184D6A28BBD9697197C40F47B74B3618432A7665B01A31457D731C3A064B C09BazaarScreen.cs
080AE871FD29887D0DB35B80BAC76CE5B8AD0E9CAB4003479C2C9A0BD53CD72A Editor/C09BazaarSetup.cs
D725433403E2C390751BB72E023FF313B1EFB359C93944AE48E0C9364C6090BF Tests/C09BazaarUiTests.cs
B63B355611DB0F0490AAB9F36CCB9D5F48BE98A4424FF747FFAC96D0D75C8A4B Tests/C10BazaarMyListUiTests.cs
FD945ACA6F9FB78506CF6522B72955A31CFB721C7A998D803209EF4F33EBA541 C11ItemPresentation.cs
CD4C876D48E178ECBB4D40507A99D0237B4C9B958E51CB043885B58195ED3B18 C05CombatScreen.cs
E4E8017D67A17CAE7081A7D1509CB9BF78182FD84F752B137A2E2FCC45BD0628 Tests/C11LiveItemUiTests.cs
```

## 남은 실행 조건

P2 수정과 영향 재리뷰가 끝난 현재 후보를 동결한다. 컴파일·UI fixture와 직접 영향 회귀·실제 BAG/MARKET 전환/선택/스크롤/스킬키 차단·정상 퇴장·캡처를 한 배치로 검증한다. 양수 실계정 데이터가 없으면 해당 화면은 fixture 근거와 구분한다. 이 리뷰에서 테스트나 Editor 실행은 하지 않았다.

## P2 수정 영향 재리뷰

Coordinator 수정 파일을 다시 읽었다. OnDisable에서 `hasInventoryResponse = false`로 카드 삭제와 성공 캐시를 함께 무효화한다. 진행 중 요청의 requestTimedOut 제한은 보존하므로 무토큰 인벤토리 응답을 새 요청과 혼동시키는 재시도를 만들지 않는다. 신규 `ReenablingAfterSuccessRequestsFreshItemsInsteadOfKeepingAnEmptyCachedView`는 성공 카드 → OnDisable/OnEnable → 같은 모델에서 BAG 열기 → 두 번째 실제 모델 요청 → 새 MP Potion/Quantity 7 선택 상세를 검증한다. assertion 완화는 없다. 이 fixture는 결정적 lifecycle 컴포넌트 시험이며 네이티브 UI 입력 시험으로 주장하지 않는다. 아직 실행 결과는 확인하지 않았다.

최종 변경 두 파일 SHA256(나머지는 위 동결값 유지):

```text
AAF3BFDC1A668E60649FF6A68ABA0A6C916399C59A7421B19BDBA58C2B415AE4 C07InventoryScreen.cs
EAE6729A3BE993E5536510AC763DE4868E7A1151368719438130F6850A459315 Tests/C07InventoryUiTests.cs
```

최종 판정: **코드상 차단 문제 없음.** 테스트 통과나 최종 Scene 적용을 의미하지 않는다. Coordinator는 컴파일 로그 오류 0을 보고했으나 리뷰어가 Editor·테스트를 직접 실행하지 않았다.

## 구역 이동 fixture 영향 재리뷰

C08ZoneTransitionTests.cs를 새 인벤토리 컴포넌트 계약과 대조해 읽었다. 제거된 inventoryRows 반사 주입은 ScrollRect/grid/detail 참조 주입으로 바뀌었고 모두 현재 runtime 필드와 일치한다. 전환 중 성공 응답 검사는 실제 생성 카드 1개, ItemName=Item #42, ItemQuantity=Qty 3으로 대체돼 기존 item ID/수량 의미를 보존한다. 구역 전환 단일 요청·거절 복원·다른 자기 ID Full의 잠금 유지·ACK 후 일치 Full까지 잠금·timeout 재접속·이동 중 BAG/refresh 차단·도착 후 저장 응답 재사용·실패 후 수동 재시도 조건은 유지돼 있다. 이 fixture는 렌더링/실입력 검사가 아니므로 최소 scroll/grid 연결로 충분하며 실제 마스크/레이아웃 검증은 최종 UI 배치에 남는다.

추가 코드상 차단 문제 없음. 기존 실패는 실패로 보존하며 이 수정의 테스트 통과를 리뷰만으로 주장하지 않는다. 테스트·Editor·제품 코드·Git 조작 없이 문서만 갱신했다.

SHA256: 203D56D199887C789D5E787D47F744F60FCE787613B2E1CF02A302E234393015

동일 시점 active root 파일과 candidate 파일 SHA256 일치: True.

## 시장 행 높이 영향 재리뷰

최종 실입력 후 발견된 시장 행 100px 표시 문제의 최소 수정으로 C09BazaarSetup.cs:98의 `rowsLayout.childControlHeight = true`를 확인했다. C09BazaarScreen.cs:271의 각 행 LayoutElement minHeight/preferredHeight=52를 이제 부모 VerticalLayoutGroup이 적용한다. childForceExpandHeight=false와 content의 PreferredSize fitter가 유지돼 행이 불필요하게 늘어나지 않는다. 30행 기준 content 높이는 패딩10 + 행1560 + 간격116 = 1686px이며 기존 ScrollRect/Mask 구조를 유지한다. runtime 데이터·선택 listener·요청/응답 경계는 바뀌지 않는다.

판정: 추가 코드상 차단 문제 없음. 관련 MARKET 행 선택/스크롤과 실제 52px 높이만 영향 확인하면 충분하다. 이미 성공한 전체 로그인·가방·조회 흐름을 이 한 줄 수정 때문에 반복할 필요는 없다. 리뷰어는 Setup을 실행하거나 Scene을 검사하지 않았으며 최종 저장 Scene 적용·실입력 결과는 Coordinator 검증 항목이다.

수정 후 Setup SHA256: 37BE40BD46D39B832E7435C2869BD43253EC71D5B4AF7723F1593F51ED112E76. 읽은 시점 candidate/active root의 Setup 해시는 동일했다. 최초 동결 표의 이전 Setup 해시는 이 값으로 대체한다. 실입력 PASS 보고는 Coordinator의 선행 관찰이며 리뷰어의 실행 결과로 합산하지 않는다.
