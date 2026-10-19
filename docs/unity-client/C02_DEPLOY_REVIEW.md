# C02-AUTH-DEPLOY-01/r1 실행 전 독립 리뷰

2026-10-18 KST. 최종 판정: **코드상 차단 문제 없음**. helper 만료 경계는 B056 수정에서 해결됐고, helper 종료 코드/복구는 FFA91D, 준비 로그 공유 읽기는 아래 1D1508 기준으로 추가 확인했다. 리뷰어는 script/native call/프로세스/Editor/DB를 실행하지 않고 이 문서만 작성했다.

검토 대상: `UnityClient/Tools/Switch-C02LocalServer.ps1` SHA256 `F2DF7B05E5C328D2F2F7193AB3E1149810919F003FAF7B9CCF7DACC369AB4B41`. C02_AUTH_DEPLOYMENT.md에 기록된 승인자는 U0C9QEUHPMK, 요청 parent `1792304333.828019`, 답변 `1792304457.908929`, 원문 `!coord C02-AUTH-DEPLOY-01 r1 승인`이다. 이 리뷰가 승인 범위를 확장하지 않는다.

## 지적과 최종 해결

P2 — Switch-C02LocalServer.ps1:130, Stop-Normally. helper.WaitForExit(15000)가 false면 부모는 중단하지만 자식 helper는 살아 있고 q 송신의 만료 조건이 없다. Add-Type/콘솔 접근이 15초를 넘긴 조건에서는 부모의 실패 보고 뒤에도 자식이 q를 보낼 수 있다. 이 경우 운영자가 본 중단 시점과 실제 서버 종료 요청 시점이 분리된다. 최소 수정은 절대 만료시각을 자식에 전달해 native Send 전과 WriteConsoleInput 직전 만료 시 송신하지 않도록 하고, 부모 timeout 보고에 잔존 helper PID/결과 확인 필요를 명시하는 것이다. 서버/자식 강제 종료를 추가하라는 요구는 아니다. 총괄에 전달했고 아직 이 hash의 실행 전 해소 확인이 필요하다.

최종 재리뷰 기준: Switch-C02LocalServer.ps1 SHA256 `B05617BA33ABE28AE61A75C3F01D4A0209205E2ACF98E6C18D09EF11EE21A57A`. **P2 코드상 해결**. 부모는 launch 직전 UTC+12초의 절대 ticks를 전달하며 자식 진입 시 누락/만료 값을 거부한다. C# Send는 양수·만료 조건을 AttachConsole 직전과 WriteConsoleInputW 직전에 다시 확인한다. 따라서 준비가 지연된 자식은 만료 후 q를 송신하지 않는다. 부모 15초 wait timeout은 helper PID·still running·outcome inconclusive를 receipt에 기록하고 throw해 후속 서버 작업을 멈춘다. 서버 정상 종료 전체30초 상한, identity·no-client·콘솔 독점·고정 이미지·준비/복구 조건과 강제 종료 금지는 유지됐다. 이 delta에서 추가 차단 결함을 발견하지 못했다. AST/native 정의 컴파일 통과는 구현 인계이며 리뷰어가 실행하지 않았다.

## 나머지 확인 범위

- 원본 EXE `529D16A75BD84417ADCFC4DD632830758D6F2939362B367FF0170B41511A9BD6`, 새 EXE `C0F689BD0E58D3A20282143D5DD5C0C15514F42F192A6FF70568C2DDF312E825`는 실제 파일 SHA256과 일치한다. 원본 파일을 덮어쓰는 실행문은 없다.
- 최초 PID17840의 이미지 경로·해시·인자 없는 command line을 검사하고 Process.StartTime UTC ticks를 helper에 전달해 재사용 PID를 거부한다. helper는 송신 직전 대상 identity와 established=0을 재확인한다.
- native 콘솔은 전용 child에서만 FreeConsole/AttachConsole하며 main shell을 분리하지 않는다. GetConsoleProcessList가 target+self 정확히 두 개일 때만 CONIN$에 q의 down/up 두 이벤트를 보낸다. 콘솔 소유 목록을 입력 handle 획득 전과 쓰기 직전에 재확인한다. 다른 콘솔 입력·강제 종료는 없다.
- 원본 정상 종료가 Process.WaitForExit로 확인되고 9999 listener/established가 없을 때만 새 EXE를 Hidden으로 시작한다. TEST* 환경값이 설정돼 있으면 중단한다. 새 경로/해시/startTicks/listener 소유와 새 stdout의 준비 완료 문자열을 최대30초 안에 함께 요구한다.
- 새 시작/준비 실패 시 살아 있는 새 프로세스에 같은 정상 종료를 요청하고 종료·빈 포트를 확인한 뒤에만 고정 원본 경로/작업 디렉터리로 복구한다. 정상 종료가 확인되지 않으면 복구 서버를 중복 시작하지 않는다. 복구 준비 실패도 성공으로 처리하지 않는다.
- 기록은 고정 승인 ID·UTC·PID·해시·상태와 로컬 stdout/stderr 파일이며 script가 계정 암호·토큰·DB 설정을 읽거나 출력하지 않는다. DB·소스·의존성·로그인/MySQL/Redis 서비스 변경은 없다.

실제 콘솔 연결/정상 q·종료 queue drain·새 서버 준비·rollback은 미실행이다. Unity Play/runner/Recorder 중지는 외부 운영 선행 조건이며 script는 게임 established=0을 검사한다. 시작 뒤 본인 캐릭터 UI/서버 통합 검증은 별도 실행 근거가 필요하다.

## 첫 시도 뒤 helper 종료 코드·복구 재리뷰

최종 고정 script SHA256: `FFA91DB93928652E135E4536BF3A1EEA21A0695F0E826A79162BEA76DF0A1AA3`. 판정: **코드상 차단 문제 없음**. 아래 정적 검토는 종료 코드 확인/복구 분기와 원본 PID 갱신에 한정한다.

- 총괄은 B056 첫 시도에서 기존 PID17840은 실제 종료됐지만 부모가 helper 실패로 판정했고 stdout/stderr는 0바이트였다고 전달했다. 이후 승인 복구 범위로 원본 보존 이미지를 PID14116에 다시 시작해 9999/준비 완료를 확인했다. 새 이미지는 시작하지 않았다. PassThru.ExitCode가 null이었다는 원인은 가설이며 기존 로그로 확정하지 않는다. 앞선 미실행 표기는 해당 당시 리뷰 상태다.
- 최종 script는 Start-Process 직후 helper.Handle을 보유하고 WaitForExit/Refresh 이후 그 handle에 GetExitCodeProcess를 호출해 uint 종료 코드를 확인한다. 단순 PassThru.ExitCode에 의존하지 않고 실제 종료 코드0만 성공으로 인정하며 receipt에 helper PID/코드를 기록한다.
- handle 확보 실패·15초 timeout·native 종료 코드 조회 실패/259는 HelperUnresolved 예외 표식을 설정한다. 원본 종료 및 새 프로세스 cleanup의 두 catch는 이 표식을 만나면 즉시 throw해 복구/추가 시작을 수행하지 않는다. 결과 불명확 상태를 성공이나 복구 완료로 바꾸지 않는다.
- helper 완료 뒤의 오류로 정상 종료 판정이 실패했을 때는 원본 프로세스가 실제 HasExited이고 포트가 비어 있는 경우에만 원본 고정 이미지로 복구한 뒤 전체 적용을 중단한다. 원본이 살아 있으면 새 서버를 시작하지 않는다. 새 시작 실패 cleanup도 종료 실패 이후 새 프로세스가 실제 종료됐고 포트가 비어 있는 경우에만 원본 복구를 진행한다.
- 최초 원본 PID는14116으로 갱신됐고 기존 이미지/path/hash/인자/startTicks, 12초 절대 만료·native preAttach/preWrite 검사, target+self 콘솔 독점, 연결0, 정상 q/종료 대기, Hidden 고정 시작·ready·rollback 조건을 유지한다. 새 강제 종료·DB/소스/의존성 변경이나 승인 범위 확대는 없다.

이번 최종 script의 실제 재시도는 리뷰어 미실행이다. AST/native 정의 컴파일 성공은 구현 인계이며 정상 전환과 실패 복구의 실제 운영 성공을 대신하지 않는다. 리뷰어는 Art 소유 Editor/Assets에도 접근·수정하지 않았다.

## 준비 로그 공유 읽기 delta 재리뷰

최종 고정 script SHA256: `1D1508A1229AD27265E51136CF255B086DB1009620324E78C2DFE933568B7D37`. 판정: **코드상 차단 문제 없음**. 이번 범위는 Wait-Ready의 파일 공유 읽기·안전한 오류 단계 진단과 원본 PID28924 갱신이다.

- 총괄은 FFA91D 재시도에서 helper native exit0, 원본14116 정상 종료와 신규15300 시작/준비 문구를 확인했으나 ReadAllBytes의 공유 IOException 때문에 적용 판정이 실패했다고 전달했다. 새 프로세스를 정상 q로 종료하고 원본을 PID28924로 복구했으며, 복구 ready 읽기도 같은 오류가 나 총괄이 원본 경로/9999 수신/준비 문구를 별도로 확인했다. 이를 도구의 자동 ready 통과로 기록하지 않는다.
- 최종 Wait-Ready는 FileStream(Open, Read, FileShare.ReadWrite|Delete)으로 redirected stdout의 기존 쓰기 handle과 공유해 읽고 MemoryStream에 복사한 바이트를 사용한다. 두 stream을 finally에서 Dispose한다. 로그 읽기 실패는 reason=ReadinessLogRead와 base exception type만 기록하고 재throw한다. 실패를 무시하거나 ready=true로 대체하지 않는다.
- 기존 UTF8/949의 정확한 준비 문구, 새 PID/path/hash/startTicks·포트 listener 소유 확인, established0·30초 ready 경계를 유지한다. 외부 startup/readiness catch도 고정 reason/exception type만 추가해 비밀·로그 원문을 출력하지 않는다. 정상 q·helper 만료/확정 종료 코드·빈 포트 이후 원본 복구 조건은 유지한다.

리뷰어는 최종 script를 실행하지 않았다. 사용자가 일시 중지한 Unity 실행 테스트는 이 도구 리뷰에 포함되지 않으며 Editor/Assets를 조작하지 않았다. 서버 전환/ready의 실제 결과와 이후 허용된 통합 테스트는 총괄의 별도 실행 기록으로 판단한다.
