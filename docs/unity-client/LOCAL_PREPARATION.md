# 로컬 테스트 환경과 변경 권한

현재는 준비 단계다. 게임 개발과 전문 에이전트 실행은 사용자의 별도 시작 지시까지 대기한다. 아래 미확정 항목은 자동 실행하지 않는다.

## 확인한 실행 환경

| 대상 | 실행 경로 / 작업 위치 | 확인 결과 |
| --- | --- | --- |
| MySQL | 서비스 MySQL84, C:/Program Files/MySQL/MySQL Server 8.4/bin/mysqld.exe | 실행 중, 3306 수신. 로그인 설정은 localhost의 login DB. 사용자가 로컬 테스트용임을 확인 |
| Redis | 서비스 Redis, C:/Program Files/Redis/redis-server.exe | 실행 중, 6379 수신 |
| 로그인 서버 | C:/Users/joeun/Desktop/workspace/Game/Login, node ./bin/www | .env와 node_modules 존재. 3000 수신, 실행 PID 22328 |
| 게임 서버 | C:/Users/joeun/Desktop/workspace/Game/x64/Release/MainServer.exe | 파일 존재. 9999 수신, 실행 PID 14072. 작업 위치는 원본 Game 루트. 준비 완료 로그 확인 |
| 클라이언트 소스 | 작업 체크아웃의 ClientCore/ClientCore.csproj | netstandard2.0, Newtonsoft.Json 13.0.3 |

원본 Game의 기존 바이너리는 작업 체크아웃에서 빌드한 산출물이 아니다. 소스/빌드 일치는 미검증이며 기능 변경 후 통합 테스트에는 해당 변경 기준 빌드가 필요하다. 실행 전에 TEST_BAZAAR/TEST_CACHE/TEST_CACHE_DURABLE 환경 변수가 설정되지 않았는지 확인한다. 내부 통합 테스트·초기화 모드는 자동 실행하지 않는다.

## 서버·계정 준비 허용 범위

사용자 허용 완료: 위 기존 로그인/게임 서버 시작, 전용 계정 unity_ai_01·unity_ai_02 생성 및 로그인 확인. 기존 MySQL/Redis를 사용하되 서비스 재시작·기존 데이터 초기화·기존 계정 수정은 허용하지 않는다. 계정이 이미 존재하면 비밀번호를 덮어쓰지 않고 사용자에게 확인한다.

게임 계정은 DB 관리자 계정과 다르다. 회원가입은 POST /auth/signup(login_id, password, email, phone_number), 로그인은 POST /auth/login이다. DB에 직접 INSERT하지 않고 기존 API를 사용한다. 임의 비밀번호·응답 JWT·세션 토큰은 문서나 출력에 남기지 않는다. 전용 계정 비밀은 사용자 Windows 계정으로 암호화해 저장소 밖에 보관한다. 계정 생성/로그인 성공과 게임 월드 진입 성공은 별도 검증이다.

## ClientCore·Git 기본 권한

| 작업 | 현재 상태 |
| --- | --- |
| ClientCore 읽기·호환성 분석 | 허용 |
| 원본 ClientCore 소스 수정 | 개발 시작 후 Unity 최소 호환 수정 허용. 프로토콜 의미 변경 등 범위 확대는 별도 승인 |
| 확정 기획의 서버 콘텐츠 변경 | P01 허용 범위 유지. 개발 시작 후 적용 |
| DB/인증/경제 정책/새 프로토콜/Zone·큐·락·풀 변경 | 별도 승인 |
| 로컬 commit | 개발 시작 후 관련 검증을 통과한 작업만 codex/unity-ai-client 브랜치에 허용. 사용자 변경은 포함하지 않음 |
| push·PR 생성·master 병합·공개/배포 | 미확정: 자동 실행하지 않음 |

Git 상태/diff 확인과 문서 편집은 진행할 수 있다. 위 권한은 행동 지침이며 기술적인 파일 접근 차단을 의미하지 않는다. push·병합·공개는 별도 사용자 승인을 받은 뒤 실행한다. 이번 준비 중에는 커밋하지 않는다.
## 준비 검증 결과

- unity_ai_01·unity_ai_02: 기존 회원가입 API로 생성하고 로그인 성공 응답과 토큰 발급을 확인했다(2/2). 비밀번호는 %LOCALAPPDATA%/UnityCoordinator/test-accounts.xml에 Windows 계정으로 암호화해 보관한다. 토큰/비밀번호는 문서에 기록하지 않는다.
- 게임 서버 최초 시작은 bazaar.sp_bazaar_cancel 누락으로 실패했다. 사용자의 명시적 요청으로 Resources/DB/procedure.sql의 해당 CREATE PROCEDURE 정의만 기존 bazaar DB에 반영했다. 기존 루틴이 없음을 확인했으며 init.sql·시딩·다른 프로시저 교체는 실행하지 않았다.
- 반영 후 게임 서버의 준비 완료 로그와 9999 수신, 로그인 서버의 3000 수신을 확인했다. MySQL·Redis 서비스는 재시작하지 않았다.
- 실행 바이너리 SHA256: 529D16A75BD84417ADCFC4DD632830758D6F2939362B367FF0170B41511A9BD6. 이 값은 파일 식별용이며 소스/빌드 일치 증명은 아니다.
- 로그인 API 검증까지 완료했다. 캐릭터 준비·게임 월드 진입·두 클라이언트 동기화는 개발 단계의 통합 검증으로 남긴다. 게임 개발·하위 에이전트 실행·Git 커밋은 시작하지 않았다.
## 추가 스키마 점검

커밋 0899e57의 변경은 login.login_log의 PRIMARY KEY(log_id) 추가다. 현재 DB 조회는 ER_NO_SUCH_TABLE을 반환해 키만 빠진 것이 아니라 테이블 자체가 없음을 확인했다. 로그인 라우트는 로그인 로그 INSERT 오류를 응답에 전달하지 않으므로 앞선 계정 로그인 성공은 로그 저장 검증이 아니다. 사용자의 명시적 승인 후 저장소 정의 그대로 해당 테이블만 생성했다. 적용 SQL은 login-log-schema.sql에 기록했다. 기존 users 데이터와 init.sql 전체는 건드리지 않는다.

검증 완료: PRIMARY KEY(log_id)와 idx_login(login_id, logged_at)을 확인했다. 전용 계정 unity_ai_01·unity_ai_02의 실제 로그인 성공 후 시험 User-Agent로 조회해 로그 2건(log_id 1, 2), succeed=1, IP와 저장 시각 기록을 확인했다. 로그인 실패 시나리오는 이번 검증 범위에 포함하지 않았다. 기존 데이터 초기화·서버 코드 수정·에이전트 개발은 수행하지 않았다.
