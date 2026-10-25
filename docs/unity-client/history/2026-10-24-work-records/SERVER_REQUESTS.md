# 서버·운영 변경 요청 후보

2026-10-18 KST. 후보·사람 승인·실행 결과를 요청 단위로 기록한다. 승인자는 사람 사용자이며 Coordinator만 Unity Coordinator 앱의 변경별 Slack 스레드로 요청한다. 승인 전 후보와 이후 실행 이력을 구분한다.

Coordinator 발신 기록: 기존 스레드 `C0C9VFZMV43 / 1792249122.665229`에 아래 두 r1 요청을 보냈다. 2026-10-18 15시경 전체 스레드 재조회로 지정 사람 승인자 U0C9QEUHPMK의 두 답변을 확인했다. 마지막 확인 뒤 도착한 답변을 놓친 상태를 바로잡았다. 자체 발신 메시지는 승인 근거에서 제외한다.

승인 원문/범위: DB 답변 ts `1792303103.818899`(14:58:23 KST), 원문 `!coord C02-DB-01 r1 승인`; AUTH 답변 ts `1792303113.782979`(14:58:33 KST), 원문 `!coord C02-AUTH-01 r1 승인`. 기존 r1 요청 범위 그대로 허용하며 AUTH의 현재 바이너리 교체/재시작/배포 제외는 유지한다.

사용자가 변경 단위별 Slack 새 스레드와 Unity Coordinator 앱 발신을 지시했다. 최초 커넥터 발신이 사용자 명의였던 오류를 바로잡고, bot user U0C9ZRTQ6F8/workspace T0C9ECC3GS3을 auth.test로 확인한 DPAPI 토큰 경로를 사용했다. 현재 DB 앱 스레드 [1792303702.565229](https://aiagent-vqu1541.slack.com/archives/C0C9VFZMV43/p1792303702565229), AUTH 앱 스레드 [1792303706.232249](https://aiagent-vqu1541.slack.com/archives/C0C9VFZMV43/p1792303706232249)로 진행을 묶고 기존 사람 승인은 원문 링크로 유지한다. 사용자 계정 발신 스레드는 과거 오류 기록으로 보존한다. 수신기 Coordinator 대상은 바꾸지 않고 각 변경 스레드를 커넥터로 직접 읽는다. 내부 DB ID를 포함한 상태 발신은 자동검토에서 차단되어 최소 변경명·기존 승인 링크만 앱으로 보냈다.

- [C02-DB-01 r1 요청](https://aiagent-vqu1541.slack.com/archives/C0C9VFZMV43/p1792300789445369?thread_ts=1792249122.665229&cid=C0C9VFZMV43), 발신 ts `1792300789.445369`.
- [C02-AUTH-01 r1 요청](https://aiagent-vqu1541.slack.com/archives/C0C9VFZMV43/p1792300793294809?thread_ts=1792249122.665229&cid=C0C9VFZMV43), 발신 ts `1792300793.294809`. Slack에 요청한 범위는 두 소스 파일 수정·로컬 빌드·비파괴 검증까지이며 현재 서버 바이너리 교체/재시작은 제외했다.

## C02-DB-01 / r1 — 지정 계정의 테스트 캐릭터 2개 준비

- 상태: Slack 및 이 채팅 직접 승인 후 guarded4행 커밋 완료. 실제 C02 선택→Zone/Full 진입과 Play 재시작 검증·캡처 완료. 승인 전 DB 변경은 없었다.
- 실행 플랫폼: 읽기 전용 preflight 통과·C02_DB_REVIEW.md 독립 리뷰 뒤 --commit을 요청했지만 자동 승인 검토가 기존 DB의 영구4행 삽입에 Slack 기록을 권한 근거로 인정하지 않는다는 이유로 거절했다. 실제 DB 커밋은 실행되지 않았다. 같은 범위의 이 채팅 직접 승인 확인을 요청했고 우회/재시도하지 않는다. 이는 Slack 승인 원문 확인과 별도의 실행 차단이다.
- 이후 사용자 직접 답변 `위 범위의 DB 커밋 승인`을 받아 같은 고정 driver --commit을 실행했고 정확히 profile2+characters2행 커밋을 완료했다. 생성 ID·좌표·시간은 비공개 로컬 `UnityClient/Captures/c02-db-preparation.txt`에만 기록했다. 스키마·계정·기존행·재화·아이템 변경은 없었다. 초기 플랫폼 거절은 이력이며 현재 DB 준비는 완료 상태다.
- 대상: 기존 `login.users` 중 `login_id IN ('unity_ai_01','unity_ai_02')`만 user_id 조회/행 잠금. 기존 `game.profile`, `game.characters`에 계정당 1행씩 총 4행 INSERT. 스키마·계정·비밀번호·프로시저·기존 행은 변경하지 않는다.
- 기존 생성 API: `Login/routes/auth.js`의 공개 POST는 login/signup/getSession뿐. 서버 PacketTypes/NonZoneHandler에도 캐릭터 생성 opcode/처리 없음. `CacheLib/Config.h:QUERY_2`/QUERY_20은 준비된 SQL이지만 확인한 네트워크 생성 경로는 없다. `Resources/DB/createCharacter.sql`은 5,000명·재화 지급·프로시저 교체를 하는 대량 시딩이므로 실행 금지.
- 정확한 후보: [C02-test-characters.candidate.sql](../../C02-test-characters.candidate.sql). 단일 연결 트랜잭션. 두 계정 모두 offline, 기존 캐릭터(삭제 표기 포함) 0개, 두 이름 충돌 0개, 대상 계정 각 1개를 확인한 경우만 진행. 드라이버는 매 INSERT 영향 행=1과 최종 정확히 2행 검증을 강제하고 하나라도 다르면 ROLLBACK한다. 재실행 시 기존 행을 재사용/덮어쓰지 않고 중단해 Coordinator가 확인한다.
- 값: 이름 `unity_ai_01_c02`/`unity_ai_02_c02`, channel=1, zone=1, 위치 (10,10)/(12,10). 나머지는 `Resources/DB/init.sql:28-52` 기존 기본값: profile version=1; level=1, exp=0, hp/mp/max=10000, attack=10, dir=0, deleted_at=NULL. login.users.user_id를 조회해 연결하며 계정명에서 숫자 ID를 추측하지 않는다.
- 승인 후 사전 점검에서 현재 DB hp 기본값=100으로 source 정의와 다름을 확인해 쓰기 전에 중단했다. 스키마는 그대로 두고 위 r1의 승인된 값을 INSERT에 명시한다. 실행 드라이버는 UnityClient/Tools/Prepare-C02Characters.cjs이며 기본 실행은 읽기 전용 검사/rollback, --commit만 검토 후 4행 생성이다. profile version·deleted_at도 명시하고 최종 모든 필드를 대조한다. 추가 승인 범위 확대는 없다.
- 최소 범위: C02 Enter/Zone 진입은 profile/characters만 읽고 인벤토리/재화 조회를 호출하지 않는다. 따라서 inventory/currency/diamond 행·초기 아이템·재화 지급은 제외. 후속 기능에서 필요하면 별도 범위를 정한다.
- 영향: 2개 캐릭터가 목록에 나타나고 정상 진입/몬스터 시뮬레이션/접속 종료에 따른 HP·좌표 등 상태 저장이 가능하다. 재시작은 소스상 필요하지 않다(목록/진입은 DB 직접 조회). 캐릭터 생성 API나 소유권 검사 정책을 새로 만들지 않는다. 위치는 Zone1 내부이나 몬스터로부터 안전하다고 검증하지 않았다.
- 검증: 승인 후 해당 두 계정과 두 신규 char/profile ID만 대조하고 실제 로그인 목록 count=1/name/ID, 선택한 ID→opcode6→opcode17 Zone1/internalID!=0/유한 좌표→동일 internalID의 Full 수신을 확인한다. 임의 계정·타인 캐릭터 조회/접속 테스트는 제외한다.
- 복구: 커밋 전 오류는 ROLLBACK. 커밋 후 자동 삭제하지 않는다. 정상 연결 해제와 서버 상태 저장 완료를 먼저 확인하고, 이번에 기록한 char/profile ID·user_id·이름이 정확히 일치하고 다른 참조/후속 데이터가 없는 경우만 별도 승인으로 해당 신규 characters→고립된 profile을 트랜잭션 삭제한다. AUTO_INCREMENT 번호 공백은 되돌리지 않는다. 계정·기존 캐릭터·기존 데이터는 복구 대상이 아니다.
- 요청할 명시 승인: 위 두 전용 계정만 읽어 식별하고 기존 스키마에 테스트 캐릭터 1개씩 생성·검증하는 로컬 DB 운영 작업. 일반적인 '계속 진행'은 이 보호 작업의 승인으로 간주하지 않는다.

## C02-AUTH-01 / r1 — Enter 캐릭터 소유권 조회 조건

- 상태: C02-AUTH-01/r1 두 파일 각1줄 수정·독립 리뷰·합성 SQL5/5·전체 로컬 빌드 exit0 완료. 초기 의존성 실패는 보존하고 기존 정확 의존성만 읽기 복원해 해결했다. 운영 적용은 별도 C02-AUTH-DEPLOY-01/r1로 분리했다. C02 클라이언트는 본인 목록 ID만 사용했고 타 계정 접속 시험은 하지 않았다.
- 원인: `CoreLib/NonZoneHandler.cpp:GetCharacterState`가 세션 userID와 CHANNEL_ID를 내부 요청에 넣지만 `CacheLib/Handler.cpp:CharacterStateRequest`는 `ExecuteSelect(3, body->characterID)`만 호출한다. `CacheLib/Config.h:QUERY_3` 조건도 `WHERE c.char_id = ?`뿐이다. 삭제 캐릭터·다른 소유자/채널의 ID를 DB 조회 단계에서 제외하지 않는다. wire/목록 제약을 보안 경계라고 볼 수 없다.
- 최소 변경안: `CacheLib/Config.h:QUERY_3` 말미를 `WHERE c.char_id = ? AND c.user_id = ? AND c.channel_id = ? AND c.deleted_at IS NULL`로 변경. `CacheLib/Handler.cpp:CharacterStateRequest` 바인딩만 `ExecuteSelect(3, body->characterID, body->userID, body->channelID)`로 변경. 사용자/채널은 클라이언트 본문에 추가하지 않고 기존 세션 파생 값을 사용한다.
- 영향: 인증/소유권 보호 영역. DB 스키마·새 wire 계약 없음. 본인/해당 채널/활성 캐릭터 성공 동작은 유지하고 미일치 조회는 기존 status=0 경로로 간다. 동일 캐릭터 중복 로그인 정책·락/큐/Zone 구조 변경은 제외한다.
- 검증 후보: SQL/바인딩 좁은 단위 검증으로 본인·맞는 채널·active 한 행 성공, 다른 owner/channel/deleted/없는 ID 0행; 기존 목록→본인 Enter 회귀. 타 계정 실접속 테스트는 별도 명시 허가 없이 실행하지 않는다. 기존 ExecuteSelect 가변 인자 바인딩 순서와 타입을 구현 시 확인한다.
- 빌드/운영: 소스 2파일 변경 후 해당 기준 서버 빌드와 정상 재시작이 필요하다. 현재 원본 바이너리와 소스 빌드 일치는 미검증이므로 현 서비스에 자동 적용하지 않는다. Coordinator가 승인 범위와 실행 경로·정상 종료/시작 정책을 확정한다.
- 복구: 해당 2개 변경만 원복한 빌드로 정상 교체(사용자 기존 변경 보존). DB/wire 변경이 없어 데이터 마이그레이션/초기화는 없다. 배포 전후 바이너리 해시와 검증 결과를 기록한다.

## C02-AUTH-DEPLOY-01 / r1 — 검증 빌드의 로컬 운영 적용

상태: 앱 요청의 사람 승인 확인, 정상 전환 도구 준비 중. 정확한 고정 경로/해시·q 정상 종료·30초 중단·원본 보존·본인 진입 회귀·복구 계획은 [C02_AUTH_DEPLOYMENT.md](C02_AUTH_DEPLOYMENT.md). Unity Coordinator 앱의 [전용 요청](https://aiagent-vqu1541.slack.com/archives/C0C9VFZMV43/p1792304333828019)에 지정 사람 U0C9QEUHPMK가 ts `1792304457.908929`(15:20:57 KST), 원문 `!coord C02-AUTH-DEPLOY-01 r1 승인`으로 답했다. 기존 source/build 승인과 별개로 이 r1의 정상 운영 전환만 허용하며 강제 종료·DB 초기화는 제외다. 현재 서버를 건드리기 전에 도구를 검토한다.

최신: 도구 SHA256 B05617BA33ABE28AE61A75C3F01D4A0209205E2ACF98E6C18D09EF11EE21A57A 정적 검사/독립 리뷰 완료. 자동 승인 검토가 Slack 기록을 정확한 서버 종료·전환의 권한 근거로 인정하지 않아 실행 요청을 거절했고 실제 운영 조작은 없었다. 이 채팅에 동일 범위 직접 승인 질문을 남겼고 대기한다. 해당 플랫폼 차단도 앱의 같은 전용 스레드에 기록했다.

### C02-AUTH-DEPLOY-01 최종 결과 — 이전 대기 상태 대체

사용자가 동일 범위에 이 채팅에서도 직접 승인한 뒤 두 도구 오류/원본 정상 복구 이력을 거쳐 최종 도구 SHA256 1D1508A1229AD27265E51136CF255B086DB1009620324E78C2DFE933568B7D37로 정상 전환했다. 2026-10-18 16:15:43 KST 작업 빌드 MainServer PID20024/포트9999/준비 완료 확인. 원본 실행 파일·로그인·DB·Redis 보존. receipt: UnityClient/Captures/c02-auth-switch-20261018-161532.txt. 새 서버 Unity 본인 진입 회귀는 사용자 실행 보류에 따라 미실행이다. 프로젝트 승인 창구는 Unity Coordinator 앱의 변경별 Slack 스레드이며 채팅 중복 승인 질문을 자체 요구하지 않는다. 관리형 플랫폼 실행 심사는 별개이며 권한 설정을 변경하거나 우회하지 않았다.
