# 아이템·거래소 화면 개선 — 검증 결과

2026-10-19. 인벤토리 카드/선택 상세와 거래소 행/선택 상세, 두 화면 전환, 내 매물 연결과 공개 화면 이력을 한 변경 묶음으로 정리했다. 현재 candidate는 codex/item-market-ui이며 master449a3ef에서 승인된 내 매물6383ac5를76f03c0으로 통합했다. 이전 PR33은 기능 브랜치에만 병합됐으므로 이번 master 대상 PR에 함께 포함한다.

## 구현·독립 리뷰

실제 서버 아이템5종만 이름/배지/색을 공유하고 미확인ID는 그대로 표시한다. 가격·수량은 원본 값이다. 등록/구매/취소/수령·장착/사용·가짜 아이템/재화/희귀도·새 DB/프로토콜은 추가하지 않았다. 인벤토리와 거래소를 별도 담당자가 구현하고 독립 리뷰를 받았다. 재활성화 때 카드가 비어도 성공 캐시가 남아 재조회하지 않는 결함을 수정하고 회귀를 추가했다. 최종 리뷰 차단0. 상세 ITEM_MARKET_REVIEW.md.

## 작업 묶음 검증

- Unity C02 PlayMode 최종98/98 통과. 실제 카드/행의 값·선택·빈 결과·실패·timeout·늦은 응답·접속/Zone 수명을 확인했다. 최초97개 중 구형 inventoryRows를 주입하는 C08 fixture5개가 실패했다. 새 ScrollRect/grid/detail 참조와 실제 카드 ID/수량 관찰로 바꾸고 전환/잠금 assertion을 보존했다. 컴파일 반영 전 재실행 실패도 보존하고, 최신 Grid 필드를 반사로 확인한 후 최종98개를 실행했다. 근거 Captures/item-ui-final-tests.txt. 초기/중간 실패 로그는 삭제하지 않았다.
- 실제 Windows 클릭/전용 계정 문자 입력으로5011 로그인→선택→필드→MARKET 판매30행/첫 행 상세/스크롤→MyList0행 성공/새로고침→BAG/새로고침→MARKET복귀→닫기→정상 로그아웃. 시장4응답과 가방2응답의 실제 서버 데이터/화면이 일치했다. 화면 상호 닫힘·가방1키차단·자기 ActionResult0, 좌표103.061752/11.0422964·HP9975·MP10000 유지. 근거 Captures/item-market-item-ui-a.txt 및8개PNG. 내부 핸들러 호출을 실제 입력으로 대신하지 않았다.
- 가방 실서버 응답은 두 번 모두 실패이며 오류를 정확히 표시했다. 기존 테스트 계정의 inventory행 부재를 숨기거나 빈 성공/양수 카드 실서버 통과로 기록하지 않는다. 카드 성공·선택은 fixture 근거만 있다. DB seed/직접쓰기·거래0.
- 캡처에서 시장 행이52px 대신 기본100px인 문제를 발견했다. 부모 LayoutGroup의 childControlHeight=true 한 줄을 독립 리뷰하고 Editor Setup으로 Scene에 적용했다. 해당 영역만 추가 실제 로그인/시장 첫 행 선택·스크롤로 확인했다. 30행 모두52px/content1686px, scroll위치0, 상세서버값일치, 정상로그아웃. 기존 전체흐름을 반복하지 않았다. Captures/item-ui-layout-check.txt, item-ui-layout-scroll-final.txt, item-market-item-ui-layout.txt/selected·scrolledPNG. 이 targeted 관찰은 공용 Abort 정리 메서드로 종료했으며 전체배치 Finish 통과로 합산하지 않는다. 최초 scroll 필드명 잘못 읽은 관찰오류는 보존 후 실제 부모 ScrollRect에서 확인했다.

## 서버와 종료 상태

이번 UI 저작으로 서버·ClientCore·DLL·DB는 바꾸지 않았다. 통합 PR에는 기존 승인된 내 매물의 TCP28/29·기존SELECT연결·Core 요청수명 변경이 포함되며 C10_RESULTS의 서버57/57·TCP4/4 및두계정 실입력 근거를 유지한다. UI만 바뀐 이번 묶음에서 관계없는 서버 회귀/서버 재시작은 반복하지 않았다. MainServer9060/포트9999 유지, 최종Established0. Play/compiling/Scene dirty=false. 기존 정상로그아웃 CancelIoEx1168 경고는 해결했다고 주장하지 않는다. Scene·소스·테스트·DLL의 root/candidate 동일성을 Captures/item-ui-final-files.txt로 확인했다. 기존 root WIP와 원본Game은 보존했다.

## 사이트와 전달

같은 브랜치에서 새 공개 사례와 최종 거래소/가방 오류 실제 캡처2개를 추가하고 이전 숫자목록 이력을 보존했다. SHOWCASE·작업별Markdown이 원본이며 생성HTML 본문을 직접 수정하지 않았다. 빌드의 링크/앵커 검사·HTTP22파일/9HTML bytes일치·ZIP·원본 이미지 동일성 통과. 실제 브라우저 홈→상세→이력→상세를 확인했고 desktop1440/mobile390에서 가로 넘침0, 이미지1280원본로드/표시800·333px를 확인했다. Captures/item-site-check.txt 및desktop/mobilePNG. 로컬 미리보기는 http://127.0.0.1:8789/MMORPG/ 이며 PR 승인/병합 전 공개Pages에 적용된 것으로 기록하지 않는다.

소스 공백 검사는 통과했다. 생성 Scene에는 Unity가 저장한 빈 m_Name/m_EditorClassIdentifier/m_text 뒤 공백 33곳만 있으며 임의 수동 편집하지 않았다.
