# C02-DB-01/r1 실행 전 독립 리뷰

2026-10-18 KST. 판정: **코드상 차단 문제 없음**. 아래 고정 driver는 기록된 사람 승인 범위 안에서 실행 가능한 코드 상태다. 이 리뷰 자체가 DB 실행이나 별도 변경의 승인은 아니다. 리뷰어는 DB/driver/Editor를 실행하지 않았고 이 문서만 작성했다.

대상: `UnityClient/Tools/Prepare-C02Characters.cjs` SHA256 `C6847D0F7EF092C893D1175F2D88040DEA24FC49CBE6CE293606322F21CECF7C`.

## 승인·값 대조

- SERVER_REQUESTS.md의 승인 원문은 지정 사용자 U0C9QEUHPMK의 `!coord C02-DB-01 r1 승인`, ts `1792303103.818899`다. Coordinator가 확인한 원문/요청 개정 기록을 근거로 검토했으며 리뷰어가 Slack 승인을 대신 발행하거나 별도 실행 범위를 추가하지 않았다.
- 기존 후보 C02-test-characters.candidate.sql과 Resources/DB/init.sql을 대조했다. 대상은 기존 unity_ai_01/unity_ai_02 각각 1개 profile와 character, 총 4행이다. 이름 두 개, channel/zone=1, 위치 (10,10)/(12,10), version=1, level=1, exp=0, hp/mp/max=10000, attack=10, dir=0, deleted_at=NULL이 승인 r1과 일치한다.
- 최초 driver 413658 기준은 실제 DB hp 기본값이 100이라는 metadata 차이로 쓰기/트랜잭션 전에 중단됐다고 Coordinator가 보고했다. 최종 driver는 승인 값 전부를 INSERT에 명시한다. 스키마·기존 행·DB 기본값을 바꾸지 않고 승인된 신규 행만 10000으로 생성하며 최종 값 assertion도 유지한다. 실제 기본값 차이를 알리는 진단은 지정 숫자 필드 metadata이고 자격 증명을 포함하지 않는다.

## 안전 경계 확인

- 인자 없음은 SELECT/행 잠금/ROLLBACK 후 종료하며 INSERT에 도달하지 않는다. 정확한 단일 `--commit` 인자만 쓰기 경로로 허용한다. 다른 인자·조합은 중단한다.
- 기존 Login의 mysql2/dotenv와 .env를 메모리에서 읽고 비밀번호를 인자나 결과에 저장하지 않는다. SQL은 고정문과 parameter binding을 사용하며 multipleStatements=false다. 단일 연결·단일 트랜잭션이다.
- 쓰기 전에 profile/characters 두 테이블이 InnoDB이며 trigger가 없음을 확인한다. 승인 값 컬럼 및 profile.version/characters.deleted_at이 존재하고 generated/EXTRA가 없는지 검사한다. 스키마 차이가 실제 INSERT를 불가능하게 하면 commit 이전 rollback 경로로 간다.
- 두 지정 login_id만 SELECT FOR UPDATE해 정확히 한 계정씩·서로 다른 user_id를 얻는다. ID를 이름에서 추정하지 않고 문자열로 유지한다. 두 user_id의 기존 characters는 deleted 상태까지 포함해 0행, 두 신규 이름 profile도 0행이어야 진행한다. 재실행은 기존 행을 재사용/덮어쓰지 않고 중단한다.
- 게임 TCP 9999 Established=0을 연결 전, 트랜잭션 검사 후, commit 직전에 확인한다. 서비스나 사용자 세션을 강제 종료하지 않는다. 이 검사는 순간 연결 상태의 관찰이며 로그인 차단 장치는 아니므로 Coordinator가 DB 준비 실행 동안 게임 테스트 세션을 열지 않는 실행 순서를 유지한다.
- INSERT는 profile→character를 계정별 한 번씩 총 네 번 수행하고 매번 affectedRows=1을 요구한다. 최종 조회는 두 승인 user_id만 대상으로 정확히 두 character를 요구하며 신규 char/profile/user ID, 이름, version, channel/zone, 위치, deleted_at과 승인 수치 전부를 확인한 뒤 commit한다.
- INSERT나 최종 확인 오류는 rollback한다. commit 시도 이후 commit 응답/결과 파일 기록에서 문제가 생기면 결과가 모호할 수 있음을 고정 메시지로 알리고 INSERT 재실행 대신 승인 행을 확인하도록 한다. commit 후 자동 삭제·재시딩·기존 행 UPDATE는 없다.
- 결과 파일과 콘솔에는 승인 ID·UTC·두 테스트 이름과 user/char/profile ID·위치만 기록한다. DB 오류 객체·SQL·연결 설정·비밀번호·토큰을 직렬화하지 않는다. inventory/currency/diamond·계정/암호·스키마·서버 소스·프로세스를 변경하는 실행문은 없다.

## 미검증과 완료 경계

정적 리뷰에서는 DB metadata/조회/쓰기 결과를 직접 확인하지 않았다. 최종 고정 hash의 읽기 전용 preflight, commit 결과 및 정확히 네 신규 행 생성은 Coordinator의 별도 실행 근거가 필요하다. commit 응답이 불명확하면 성공/실패를 단정하거나 즉시 재실행하지 않는다. 이후 실제 로그인 목록 count=1→선택 ID→Enter/Zone/일치 Full 검증도 이 DB driver 리뷰와 별개다. AUTH 서버 수정·서버 바이너리 교체/재시작·타 계정 실접속 시험은 이 판정에 포함되지 않는다.
