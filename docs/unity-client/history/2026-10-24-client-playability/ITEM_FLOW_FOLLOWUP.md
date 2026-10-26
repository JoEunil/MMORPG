# 가방·거래소 아이템 갱신 흐름

- 기준: PR55 게시 버전. 레이더 후보는 기존 브랜치에 보존하고 새 codex/inventory-market-handoff에서 분리한다.
- 코드 추적 결함: 가방 읽기 중 I→B→판매 전환이 두 번째 inventory 요청을 보내 pending 예외로 끝난다. 진행 중 읽기의 완료를 기존 UI에서 공유/병합한다.
- 코드 추적 결함: 아이템 수령/판매 취소 중 창을 닫으면 완료 관찰이 끊기고, 먼저 열린 가방이 완료 후 낡은 내용으로 남을 수 있다. 패널 표시와 요청 완료 수명을 분리해 기존 RefreshAfterDelivery로 합친다.
- 금지: 서버·몬스터 AI·DB·패킷·ClientCore 복제·새 manager·mutation 재시도·레이더12파일 변경.
- 완료: 두 흐름을 고친 후 독립 리뷰, 변경 검사 최대2개만 실행. 이미 통과한 검사 반복 없음. 실제 입력/실거래와 fixture 검사를 구분한다.
- 게시: 고정 후보를 Astra 검수 후 Slack으로 한 번 승인 요청. PR55 병합 여부를 다시 확인해 base 선택, 사용자 병합은 하지 않는다.

## 구현·검증 결과
제품 C07/C09 2파일과 기존 C09BazaarUiTests 1파일 변경. C07 기존 읽기 소유권을 두 화면이 재사용하고, C09는 소유자의 최종 조회 완료까지 UI 처리를 지연한다. 닫힌 창의 Receive/Cancel도 완료를 관찰해 기존 RefreshAfterDelivery를 호출한다. mutation 재시도 없음.
독립 리뷰1건: 실패한 A 뒤 배송fresh B가 예약되어 있으면 C09가 A실패로 종료하는 경로. success 조건을 제거해 소유자 pending 동안 성공/실패 모두 기다리게 교정했다. 두 이벤트 구독 순서에서 A실패/B성공을 집중 검사에 포함했다.
Unity6000.0.47f1/PID43612, CLI1.0.0-beta.14 최신확인. 컴파일오류0. PlayMode Category InventoryMarketHandoff만1회실행, 2/2최초통과(.2216275s/.0346175s). 양방향패널전환·조회수·최신수량과닫힌수령/취소늦은완료·중복완료무시·mutation1회를확인했다. 기존통과검사반복0. 기존outbox닫기/Escape assertion은변경규약에맞춰갱신했으며실행범위는신규2개만이다.
사용자 몬스터AI/서버/DB/wire/Core/DLL/Scene/재질변경0. ROOT/CAND Plugin ClientCore.dll SHA256 DF871CE1FDA3C676D5A4F34CF414876EFED2E69E483C5F34A5A9DEB7FAF7F49B 일치. ROOT ClientCore 원본과 CAND 원본 차이는 보존했고 실제CAND pending계약·동일DLL로검증했다.
ROOT Editor에는 미게시 radar 제품이 남고 새Git브랜치에는 게시baseline만있다. 이중범위를같은전체후보라고보고하지않는다. 이번3파일hash는정본과Editor일치하며검사는아이템흐름fixture에한정했다. 실제입력/실서버성공거래증거아님. 시각배치변경없어새캡처/이미통과화면검사반복없음.
Astra/medium 종합검수 통과·필수수정0. 제품3파일·집중2검사결과·공개설명을확인했고별도PR분리타당판정. 사이트빌드/링크와diff whitespace검사통과. Editor는C02Edit/clean/compilingfalse/backgroundfalse로복원. RADAR01 후보12파일변경없음,별도브랜치에보존. 실제게시/병합/배포없음.
