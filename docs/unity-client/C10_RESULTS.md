# 내 매물 조회 — 검증 결과

2026-10-19. 현재 필드 캐릭터의 매물 조회, 새로고침, 판매 검색 복귀, 닫기를 구현하고 독립 리뷰·자동 회귀·실입력 검증을 마쳤다. 등록·구매·취소·수령·DB seed·직접 데이터 쓰기는 수행하지 않았다.

## 변경과 리뷰

서버는 TCP28/29에서 클라이언트 소유자 ID를 받지 않고 인증된 필드 캐릭터를 사용한다. 기존 query11 SELECT를 연결하고 정상 0행과 실패를 구별한다. SOLD로 정산 가능 여부를 추론하지 않는다. ClientCore는 기존 Bazaar 데이터와 이벤트를 재사용하며 요청 종류/ID, 취소·접속·Zone 수명을 확인한다. UI에는 SALES/MY LISTINGS 탭과 새로고침을 추가했다.

서버/Core 독립 리뷰와 UI 독립 리뷰의 최종 차단은 0건이다. UI 리뷰가 발견한 테스트 namespace 오류 2건과 기존 폰트의 한글 미지원은 namespace 명시와 기존 영문 라벨 사용으로 수정했다. 새 폰트·패키지·프로젝트 설정 변경은 없다. 실입력 helper는 실제 이벤트를 관찰하고 버튼이나 수신 핸들러를 대신 호출하지 않는다.

## 자동 배치

- C++ 선택 회귀 57/57 통과: MyList 신규11, Search parser/queue, NonZone 인증·귀속·지연 응답, TCP 분할/길이/flags 등 직접 영향 범위. 근거 `C10-Delivery/c10-unit-results.txt`.
- Unity C02 PlayMode 96/96 및 C01 TCP 4/4 통과. 신규 MyList Core11/UI2와 기존 검색·세션·필드 회귀를 포함한다. 근거 원본 체크아웃 `Captures/c10-unity-tests.txt`, `c10-tcp-results.txt`.
- 처음 EditMode/assembly:C02.Tests 선택은 실제 어셈블리가 C02.PlayModeTests라 0건이었다. 이 결과는 통과 수에 포함하지 않았고 원본을 보존한 뒤 올바른 PlayMode namespace로 실행했다.

## 실제 입력

Windows 실제 클릭으로 두 전용 계정의 로그인·캐릭터 선택·필드 진입 후 MARKET→MY LISTINGS→REFRESH→SALES→CLOSE→Disconnect를 각각 수행했다. 텍스트는 기존 검증된 GameView 입력 이벤트로 입력했고 비밀 값은 저장하지 않았다. 각 계정의 4회 응답, 총8회 모두 실제 ViewData와 행·상태 표시가 일치했다. 내 매물은 두 캐릭터 모두 정상 빈 성공이었다. SELECT 사전/사후 소유 매물 수는 각각 0으로 동일했다.

- 캐릭터5011: 좌표(103.061752,11.0422964), HP9975/MP10000 유지.
- 캐릭터5012: 좌표(42.7640381,49.47214), HP10000/MP9999 유지.
- 첫 계정에서 거래소 열린 상태의 실제 1키 입력과 30행 판매 목록 스크롤을 확인했다. 닫기 후 행 초기화와 정상 로그아웃도 두 계정에서 통과했다.
- PNG8장 모두1280×720 원본으로 남겼고 첫 내 매물 캡처를 직접 검수했다. 근거 `Captures/c10-physical-c10-first-a.txt`, `c10-physical-c10-second-a.txt`와 같은 접두어 PNG.

실제 양수 소유 매물이 없으므로 실제 소유 행·판매 상태별 화면은 미실행이다. 0/1/30행과 Trading/Sold/Claimed 상태 표시 근거는 자동 fixture이며 실제 데이터 검증과 구분한다. 데이터를 만들거나 바꿔 성공 조건을 채우지 않았다.

## 서버 전환·남은 문제

기존 C09 서버36748은 무접속에서 q로 정상 종료했고 캐릭터 스냅샷 전후 동일, WAL0·포트 해제를 확인했다. 전환 도구는 종료 시 error.log 증가로 중단했다. 기존 IOCP의 listen 종료 뒤 실패 ACCEPT completion이 PostAccept를 재호출하는 경로에서 GQCS995 8건/AcceptEx10022·10038 8건이 발생한 것으로 소스와 로그를 대조했다. 로그를 보존하고 별도 start-only 사전검사를 통과한 뒤 C10을 기동했다. IOCP 제품 코드를 고치거나 검증 guard를 약화하지 않았다.

C10 서버9060(StartUtcTicks639280006196299384)은 검토한 MainServer SHA256 `3D1011A6B7C8E2FE6A0ECC3C3B739E65EBBC340F4F80B0DF8E213073CAAB402E`, 포트9999 준비/초기 error0을 확인했다. 정상 로그아웃 뒤 기존 CancelIoEx1168 경고2건이 남았다. 별도 잔여 문제이며 성공으로 숨기지 않는다. 마지막 Unity Play/compile/Scene dirty=false, 임시 계정 선택 복구, 게임 Established 연결0이다. 서버는 후속 개발용으로 유지한다.

ClientCore DLL은 `CCD88B33B6A9A4923AD48FE816A62B4688F38018CA7AB7A2BE21C9490623F473`이다. root의 기존 파일은 `Captures/c10-before-integration`에 보존했다. 소스·Scene·DLL 동결 기록은 `Captures/c10-final-freeze.txt`이며 시험 뒤 제품 수정은 없다. 기존 C09/P28 코드와 원래 Game 작업 폴더를 보존했다.

## 전달

기능 브랜치는 `codex/unity-c10-my-list`이며 기존 PR29의 head `0e2adcf`에서 시작했다. PR29는 이번 확인 시 아직 열려 있으므로 새 PR은 해당 브랜치를 base로 해 MyList 변경만 리뷰하도록 준비한다. 이 문서 작성은 원격 push/PR/병합 승인을 의미하지 않는다. 사이트에는 실제 빈 결과와 검색 복귀 캡처를 별도 변경으로 추가한다.
