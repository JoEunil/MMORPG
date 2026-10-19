# C09 배치 준비 독립 리뷰

범위: 신규 `C09LiveBazaarUiTests.cs`, 두 로컬 서버 도구의 C09 추가 diff, C09 UI fixture의 컴파일 최소 수정. 제품·Editor·서버·DB 실행이나 Git/Slack 조작은 하지 않았다.

**판정: 코드상 차단 문제 없음.** 실행·컴파일 통과를 이 판정으로 대신하지 않는다.

## Live observer

- 실제 물리 입력으로 열린 UI의 실제 ViewModel 응답만 관찰한다. handler 직접 호출이나 가짜 서버 결과로 입력 성공을 만들지 않는다. Begin은 전용 캐릭터5011의 확인된 필드 상태·닫힌 거래소를 요구한다.
- 응답별 유형/페이지 1/0→2/0→3/0→3/1→3/1→3/0→3/0 순서, 성공 여부, 실제 ViewData의 숫자 행과 UI 일치, 최대30행, 빈 성공 표시를 검사한다. 좌표·HP·MP는 Begin 스냅샷과 비교한다.
- Observe는 assertion/파일 예외를 잡아 실패를 누적하고 callback 밖으로 던지지 않는다. 후속 Checkpoint/Finish가 누적 실패를 다시 assertion하므로 앱 흐름을 방해하거나 실패를 숨기지 않는다. Finish는 실제 UI 정상 로그아웃·숨겨진 월드를 요구하며 finally에서 구독을 해제한다.
- 출력은 조회 결과의 공개 listing/item/seller ID·수량·가격·등록 시각 및 테스트 캐릭터 위치/HP/MP뿐이다. 로그인 ID·비밀번호·토큰 읽기/출력이 없고 runId/캡처 label은 경로 문자 제한과 기존 증거 덮어쓰기 방지를 거친다. Screenshot 저장은 비동기이므로 완료와 실제 이미지는 Coordinator가 별도 확인해야 한다.

## 로컬 서버 도구

동결 SHA256 확인: Switch `DB232C5872FD9E4124CE70E696233C8ABB7EF35E4D1C7A2AFBE0265F2A3D5450`, Start `432092795A851E3EE54C3366B475543AD487119250286D6C99FF3EAF475CB7A8`.

- C09는 고정 C08 이미지 B3D4E8B…/PID35672/StartUtcTicks639279859750855209에서 고정 C09 이미지 ABA94914…로 전환한다. 원래 C08 실행 파일은 보존되고 조건부 복구도 그 이미지로 향한다.
- C09 모드는 C07/C08 switch·hash 및 별도 R01 승인 ID와 혼합할 수 없다. C09 hash 단독이나 검토 hash 불일치는 거부한다. quit child 인자에 C09 mode/hash가 전달되어 부모와 동일한 Original/New 이미지를 검증한다.
- 기존 PID/path/hash/starttime·독점 console·무접속·q 정상 종료·native exit0/30초·포트·TEST 환경변수·DLL·WAL·로그·읽기 전용 DB 스냅샷·종료 오류 delta guard 본체는 diff에서 바뀌지 않았다. guard 실패 뒤 강제 종료나 자동 복구를 추가하지 않았다. Start-only도 MainServer/9999 부재와 고정 hash를 다시 확인한다.

## UI fixture 컴파일 수정

초기 독립 리뷰에서 놓친 두 컴파일 누락을 Coordinator가 실제 컴파일로 발견했다. 존재하지 않는 `fixture.Refresh` 두 참조는 실제 같은 SEARCH/REFRESH 버튼인 `fixture.Search`로 교체했고, `GetActionState()`는 실제 ViewData가 구현하는 `IViewDataModel`로 명시해 호출한다. 수정부를 재검토했으며 요청 횟수·이전 결과 제거·실제 행동 취소 assertion은 유지된다. assertion 삭제로 테스트를 통과시키는 변경은 없다.

이후 검증은 동결 버전의 공통 배치에서 수행한다. 이 리뷰는 실제 server start/SELECT/7회 UI 입력·캡처 성공을 주장하지 않는다.

## PR28 회귀 통합 추가 검토

동결 root fixture SHA256 `BD918712D32D9685BB4C0EE2B1ADC251D0B225CB52718FAFFB28DD8F333EC53F`, Delivery fixture `418D733B77B11B1675AEA7A020883E6990F57B086B663142B4B248060EE23981`를 직접 확인했다. 두 파일의 OP용 CountedView 생성자와 `PendingEnterCompletionCannotRebindEmigratedCharacterOrInventory` 테스트 블록은 PR28 수정 커밋 `c571` 원문과 각각 정확히 일치한다.

Delivery의 df172f6 대비 두 파일 diff는 기존 write-once guard와 그 두 회귀 블록뿐이며 fixture의 충돌 marker는 없다. root에는 기존 C09 fixture에 같은 두 블록이 추가됐고, 기존52개 내용 보존은 담당자의 메모리상 블록 제거 후 이전 동결 SHA 일치 근거로 확인했다. 원래 두 실제 ENTER/빈 로비/늦은 응답/후속 Inventory5011/same-ID rollback 의미를 유지하며 C08 원자 전환 경계도 수정하지 않는다.

**추가 통합 판정: 코드상 차단 문제 없음.** index 충돌 해제·커밋은 Coordinator 책임이며 이 리뷰는 Git을 변경하지 않았다. 새 회귀1개만 빌드·실행하면 되고 기존52개 재실행은 요구하지 않는다. 실행 중 서버 이미지와 DLL의 동일성은 Coordinator·담당자의 기존 해시 증거를 사용하며, 이 리뷰에서 프로세스/바이너리를 조작하지 않았다.

## 실제 화면 하단 겹침 수정 재검토

Coordinator가 실제 첫 SELECT의30행·숫자 UI/좌표/HP/MP 일치를 확인한 뒤 하단 Search/Prev 겹침으로 배치를 중단하고 증거를 보존했다. 리뷰어는 실제 입력·PNG를 직접 검증하지 않았으므로 전체7조회 성공으로 기록하지 않는다.

Setup의 page x50, Prev x-32, Next x132 세 위치 수정만 재검토했다. paper492폭과 기존 anchor/pivot/폭을 대조하면 Search x14..162, Prev180..248, page258..334, Next344..412로 서로 겹치지 않고 부모 내부에 들어간다. 제품 조회 로직·ClientCore DLL·테스트 assertion은 바뀌지 않았다. **수정분 코드상 차단 문제 없음.** Editor API 적용 후 실제 화면·7조회 흐름을 영향 검증으로 재확인하면 되고, 이 배치 위치 수정 때문에 기존 자동 회귀를 반복할 필요는 없다.

## 최종 전달 후보 좁은 통합 검토

Delivery HEAD `6823367` 위 작업 트리와 root의 `c09-owned-manifest.txt` 60파일을 읽기 전용으로 비교했다. 전부 존재하며 SHA256 불일치0, manifest 밖 tracked/untracked 추가0이다. 실제 차이는 tracked39+untracked20이고 LobbyZone은 이미 HEAD에 동일 guard가 있어 추가 diff가 없다. PR28 원형 회귀는 기존 문맥으로 보존됐으며 그 테스트를 삭제·완화하지 않았다. 정책·receiver·다른 root WIP 파일을 전달 후보에 추가하지 않았다.

최종 c의 원본 txt를 직접 읽어 7응답 성공,30/12/0×5, 모두 uiMatch=True, 각 응답 좌표/HP/MP 동일, 실제 Close 및 정상 logout Finish PASS를 확인했다. PNG 직접 검수·Play/dirty/접속 상태 확인은 Coordinator 근거를 사용한다. RESULTS와 PR_BODY의 빈 응답5회 및 자동 배치별 검증 한계 설명도 원본과 일치한다.

**최종 통합 코드상 차단 문제 없음.** 검증된 제품과 전달 파일의 해시 일치로 새 제품 변경이 없으므로 추가 시험 반복은 요구하지 않는다. 이 문단 추가 뒤 이 리뷰 문서만 Delivery에 다시 복사해야 하며, 최종 commit·원격 전달 승인은 Coordinator가 별도 처리한다.
