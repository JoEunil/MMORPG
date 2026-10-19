# C09 독립 리뷰

기준: `df172f6` 이후 C09 제품·fixture 후보 및 신규 C09 화면/Setup/테스트. 리뷰어는 제품 파일·Editor·실행 환경·Git·Slack을 수정하지 않았다. 서버 계약·보안·동시성 검토의 high 상향을 Coordinator에게 요청했고, 도구의 역할 설정은 변경하지 않은 채 Coordinator 교차 검토와 병행했다.

## 발견 및 수정 확인

- **P2 / 테스트 컴파일 차단 — 수정 확인:** `C09BazaarUiTests.ZoneTransitionCancelsRequestAndClearsPanel`의 존재하지 않는 `FakeNetwork.Cancellations` 참조를 제거했다. 늦은 응답 뒤 실제 `Login.Data.GetBazaar()`가 비었는지 확인하는 assertion으로 교체한 수정분을 재검토했다. 닫힌 패널 뒤 모델 재오염까지 검사하므로 취소 검증을 약화하지 않는다. 실행 컴파일 확인은 이후 공통 배치 범위다.
- Coordinator가 발견한 유형 버튼 겹침은 Setup의 세 버튼 폭을 각각142로 수정한 상태를 읽어 확인했다. 최종 화면 배치는 실행 배치에서 별도 확인한다.

## 코드 대조 결과

- TCP26 요청16바이트와 TCP27 응답15+39×count(최대1185), flags0, count0~30, 실제 최대 크기와 bigPacketPool 사용을 대조했다. ClientCore의 TCP 길이/flags와 parser의 count/status/ID/중복 행 검증이 일치한다.
- 새 요청은 인증 완료·userID/characterID·유효 필드 Zone을 한 shard 조회로 확인한다. Cache Search는 기존 query12의 TRADING·item_type·LIMIT30/OFFSET SELECT만 호출한다. 등록/구매/취소/수령/Outbox와 DB 쓰기 경로는 이번 진입점에 연결되지 않았다.
- Core/Cache 풀2048과 응답 sizeof guard, 실제 양쪽 queue의 전체 복사 경로를 대조했다. 요청을 응답으로 덮기 전에 필드를 보존하고, DB null/예외/축소 오류/부분 읽기 실패는 count0 실패로 초기화한다. enqueue 거부·DB worker 실패 callback은 원래 메시지를 한 번 반환하도록 구성됐다. 응답 queue 거부는 로그 후 반환하며 클라이언트 제한 시간으로 종료한다.
- ClientCore는 단일 pending requestID를 소비하고 mismatch/취소/닫기/전환/timeout/Dispose 뒤 응답을 버린다. 새 요청은 ID를 증가시키고 ID 소진은 재접속으로 제한한다. ViewData는 반환 목록을 복사한다. UI는 검색 중 중복 요청을 막고, 열기/검색 때 기존 행동을 취소하며 Canvas raycast와 C05 모달 guard를 사용한다.
- 사용자 P2: LobbyZone이 shard→lobby 잠금 아래 다른 characterID의 늦은 로드 응답을 거부하고 기존 귀속을 덮어쓰지 않는다. C08의 최초 진입은 같은 shard를 잡은 상태에서 lobby→destination→zoneID 갱신을 마치므로 그 원자 경계를 보존한다. 새 fixture는 빈 lobby 창과 실제 최초 admission 경합을 구분한다.

## 판정 및 미검증

**코드상 차단 문제 없음:** 발견한 fixture 참조 오류의 최소 수정 확인을 마쳤다. 검토한 제품 경로에는 추가 코드상 차단 결함을 발견하지 못했다. 불필요한 추상화·범용 프레임워크 추가는 없다.

리뷰 시점 기능 테스트·Unity 입력·실DB SELECT·실서버 바이너리·최종 Scene 적용은 실행하지 않았다. 코드 리뷰는 Coordinator의 공통 배치 결과를 대신하지 않는다. 실DB 양수 행이 없으면 fixture30행과 실제 빈 성공을 구분해야 한다.
