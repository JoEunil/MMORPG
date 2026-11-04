# 거래 결과·아이템 사용 안내 보완 — 2026-11-04

## 범위와 결정

사용자가 작업 재개와 입력 테스트 보류를 지시했다. 최신 master85d98d3에서 codex/client-commerce-feedback을 분기했다. 정본 ROOT/UnityClient/Builds/ClientCommerce/source, 실제 Editor ROOT/UnityClient. 사용자 서버 refactor·몬스터 AI·ClientCore·패킷·DB·Scene·Prefab·에셋은 변경하지 않았다.

Pages의 PR63 병합85d98d3 배포 run41636826584 success와 공개 /play/ HTTP200, 새 교전·포션 GIF참조 반영을 확인했다. 별도 재배포하지 않았다.

## 구현

- 거래 결과를 닫기·재열기에서 보존한다. 등록 성공은 내 매물 보기로 바꾸되 자동 조회가 결과를 지우지 않게 한다. 직접 Refresh·탭 변경·새 거래에서 결과 문구를 해제한다. 지갑·가방 갱신, 늦은 응답·요청 상관성·불명 결과 재시도 보호는 기존 경로다.
- 자동조회 결과에 표시 가드를 덧대는 안, 재열기/등록후 자동조회만 생략하는 안, 별도 결과 배너를 만드는 안을 비교했다. 종합 검수는 단일 statusText와 bool하나로 충분한 두 번째 안을 추천했다.
- 가방에서 HP포션·미지원 MP포션·장비·미확인 아이템의 용도를 구분한다. HP결과 불명은 재접속 안내를 우선하고 쿨다운 종료로 결과 안내를 조기 삭제하지 않는다. 새 사용 효과나 장착 기능은 추가하지 않았다.
- 총괄 코드검토에서 포션 불명 결과가 기본 설명에 가려지는 문제를 찾아 같은 제작 담당자가 보완했다.

## 검증과 제한

Unity6000.0.47f1/CLI1.0.0-beta.16. ROOT의 대상6파일이 master기준과 동일함을 확인해 외부 verification/root-before-sync에 백업하고 후보만 동기화했다. 재컴파일 completed, failed=false, errors=[], Play=false와 새 필드/메서드 로드를 확인했다. 파일 SHA256은 verification/candidate-files.json으로 정본과 Editor 일치를 확인했다. git diff --check 통과.

변경한 5개 fixture 메서드(매개변수 포함7경우)를 CommerceFeedback 카테고리로 준비했다. 실행 도구의 자동 심사가 같은 파일의 InputSystem 경로 때문에 PlayMode 실행을 입력 보류 위반으로 거절했다. 실행 전 거절이며 테스트 실패나 통과가 아니다. 우회·간접 실행·필터를 바꾼 재시도는 하지 않았다. 상태 회귀7개와 실입력·실서버·실화면 검증은 미실행이다. 기존 통과 테스트를 반복하지 않았다.

테스트 서버를 시작하지 않았다. Editor는 Play off 상태로 남긴다. 새 GIF·실화면 자료를 만들지 않았으며 공개 사이트의 실제 동작 완료로 이 수정을 설명하지 않는다. 입력 재개 시 바뀐 거래·가방 경로만 한 배치로 확인한다.

## 종합 검수

같은 commerce_quality 담당자가 상황자문 이후 고정6파일 diff를 최종 코드·범위 검수했다. 정적 검수 통과/필수수정0이며 실행 품질은 근거 부족이다. 컴파일·정적 검수 완료를 실행/화면/게시검수 통과로 확대하지 않는다. 검증 보류 상태로 로컬 후보만 보존한다.

## 사용자 입력 재개 후 — 2026-11-04

사용자의 명시적 입력 재개에 따라 기존 CommerceFeedback PlayMode fixture를 한 번 실행했다. total7/pass7/fail0/skip0,0.32초. 기존 보류 동안의 자동심사를 우회한 것이 아니다. Temp/pipeline_test_status.json을 verification/state-tests.json에 보존했다. 현재 실제 UI는 test1 수동 로그인 대기이며, 인증 후 같은 Play에서 이어간다. 기존 테스트 서버만 기동했고 소스/DB/AI 변경은 없다.

## 같은 Play 실제 검증 완료

사용자가 test1 수동 로그인을 완료한 뒤 캐릭터11/char_1로 필드 진입했다. Play·Assets 재시작/쓰기 없이 정상 InputSystem 이벤트 큐→PlayerLoop로 I/B·버튼을 조작했다. 새 입력장치 바인딩과 목록 피벗 때문에 최초 도우미 선택이 반영되지 않아, 기존 현재 장치·RectTransform 실제 중앙을 쓰도록 외부 도우미만 수정했다. 강제 InputSystem.Update·제품 핸들러 직접 호출·상태 주입은 없었으며 물리 키 검증과 구분한다.

HP포션 안내와 사용 버튼, unknown33 안내와 버튼없음을 실제 확인했다. MP·장비·실패/timeout은 상태 fixture 범위다. HP포션1개/100다이아 등록 성공, 골드99990→99980/수수료10, 포션9→8을 확인했다. 닫기/재열기 뒤 성공문구 동일=true, 직접Refresh 후 매물182 확인, 선택/취소/수령 뒤 포션9복귀. 수령 응답은 Received0/already present1/pending0으로 자동 회수가 이미 반영됐음을 뜻하며 별도1개 신규지급으로 보고하지 않는다. 수수료는 정상 게임 규칙대로 환불하지 않았다. DB를 직접 고치지 않았다.

근거 live/bag-hp.png,bag-unknown.png,register-reopened.png,items-received.png와register-result.txt. 공개에는 사용 안내/재열기 두 PNG만 화면 이력에 연결했다. 기존 대표 화면이 전체 상태를 더 잘 보여줘 README대표 선택은 유지했다.

같은 commerce_quality 담당자가 실제4화면을 열어 게시검수 통과/필수0. 목록의 기존Trading잔상·수령상세 일부말줄임은 기존표시한계이며 CANCELLED/수령결과는명확하다. 이번후보의실패/timeout모든상황을실서버에서재연했다고주장하지않는다.

정상Disconnect,소유MainServer q+Enter/exit0,helper LOGIN_STOPPED,Playoff/background=false를확인했다. 기존autotick·DB/Redis유지. 사용자명시PR게시요청이있어동일승인을다시요구하지않는다. 게시후서버없는성장HUD작업을이어간다.
