# 로컬 환경 대조와 정상 전환

현재 목표·입력·실행 후보는 HANDOFF, 권한은 AGENTS.md를 따른다. 아래는 2026-10-24 감사 관찰과 확인 절차이며 영구 실행 상태가 아니다.

## 위치

- Editor: C:/Users/joeun/Desktop/workspace/Game-UnityClient/UnityClient
- Git 정본: 위 프로젝트 Builds/C10-Delivery/source
- 현재 씬: Assets/Client/Scenes/Login.unity · Assets/Client/Scenes/World.unity
- 기본 빌드 시작 씬은 로그인·월드 UI가 함께 있는 World.unity다. Unity 기본 SampleScene·환영 안내는 제거했으며 SampleSceneProfile은 이름과 달리 현재 PC 렌더링 설정이 사용하므로 보존한다.
- 현재 코드: Assets/Client/Runtime · Editor · Tests (Mmorpg.Client.* 어셈블리)
- 사용자 서버: C:/Users/joeun/Desktop/workspace/Game — 사용자 AI/미완료 변경 보존
- ClientCore: 정본 ClientCore/ClientCore.csproj → UnityClient/Assets/Plugins/ClientCore/ClientCore.dll
- 기존 비밀: 실제 사용자 LOCALAPPDATA/UnityCoordinator의 Windows 계정 암호화 파일. 내용을 출력하지 않는다.

## 실행 전

격리된 도구 환경의 STATUS_NO_INSTANCES만으로 Editor 종료를 단정하지 않는다. 실제 사용자 프로세스·프로젝트와 대조하고, 필요한 개별 Unity 명령은 도구 심사를 거친 사용자 컨텍스트에서 확인한다. Editor가 있는데 별도 인스턴스를 다시 띄우거나 권한·보안 설정을 바꾸지 않는다.

실제 프로젝트·컴파일·Play·프레임, 서버 실행 파일·작업 폴더·소스 기준·DLL, 포트·소유 프로세스, TEST_BAZAAR/TEST_CACHE/TEST_CACHE_DURABLE 초기화 모드 미설정을 확인한다. 전용 계정·아이템·재화가 부족하면 미검증으로 남기며 DB seed로 통과시키지 않는다.

감사 시 Unity6000.0.47f1/Pipeline7800, Login3000·MySQL3306·Redis6379·미리보기8791 수신, MainServer9999 미수신을 확인했다. 옛 PID나 파일 존재로 현재 실행/소스 일치를 단정하지 않는다.

전용 unity_ai_01/02와 기존 비밀을 재사용한다. 비밀번호 덮어쓰기·DB HP복구·재화 seed·초기화는 하지 않는다. 정상 게임 상태 변경과 DB 직접 수정을 구분한다.

## 정상 종료·복구

### 자동화 키 입력 구분

2026-10-25 현재 Computer Use `press_key`의 I/Shift+I/Tab은 Unity 원시 추적에서 TEXT로 관찰됐으며 해당 키의 STAT 눌림은 관찰되지 않았다. 이 상태에서 `Keyboard.current.*Key.wasPressedThisFrame`이 반응하지 않는 것을 제품 단축키 결함으로 단정하지 않는다. 포커스·프레임·장치와 원시 이벤트를 구분해 확인하고 실제 물리 키 입력과 비교한다. 사용자 물리 입력은 같은 관찰에서 I/B 각각1회 정상 감지됐다. 이 차이로 게임 단축키를 변경하지 않는다. 도구 내부 구현/다른 버전의 동작은 추정하지 않는다.

자동화된 게임 입력이 필요하면 기존 `Mmorpg.Client.Tests.LoginEditorInputRun`과 정상 PlayerLoop가 소비하는 `InputSystem.QueueStateEvent` 경로를 재사용할 수 있다. 명시적으로 합성 키 입력이라고 기록하고 native/물리 키 검증을 대신했다고 보고하지 않는다. 강제 `InputSystem.Update`나 제품 핸들러 직접 호출로 성공을 만들지 않으며, 키 해제·관찰기 정리를 보장한다. 도구 문제를 숨기기 위해 TEXT 이벤트를 게임 단축키로 연결하지 않는다.

## 정상 종료·복구

현재 대상을 식별하고 기존 q 정상 종료·저장을 확인한다. 기존 전환 도구의 시간 제한·복구 조건을 지킨다. 종료 미확인 시 강제 종료하지 않는다. 원본 빌드·복구 위치를 보존하고 서버/DLL 동시 버전이 필요한 변경은 혼용하지 않는다.

부모 shell 정책을 자식 런타임 정책으로 가정하지 않는다. 실제 실행 파일의 정책·표준입력·로그로 도구 차단과 제품 결함을 구분한다. 과거 STOP01의 프로세스 한정 정책 승인은 미래 변경 승인이 아니다. 다른 shell/inline 경로로 우회하지 않는다.

종료 시 Play·background·임시 설정·연결과 서버 유지/종료 상태를 복원·기록한다. MySQL/Redis 재시작·계정/기존 데이터 변경은 별도 승인 경계를 유지한다.

초기 DB 준비·승인·실행 기록은 [원문 이력](history/2026-10-24-guideline-audit/README.md)에 보존한다.

## 검증 포트 지정

Windows 예약 범위가9999를 포함하면 예약·방화벽 설정을 자동 변경하지 않는다. TCP서버와 Login 앱 양쪽에 같은 `MMORPG_GAME_PORT`를 명시할 수 있다. 미설정 기본값9999는 유지하고 빈 값·숫자가 아닌 값·1~65535 밖은 거부한다. 로그인 응답이 실제 포트를 전달하므로 Unity 주소를 임의 우회하지 않는다. 2026-10-29에는 예약9923~10022를 확인해10099를 사용했다. 다음 실행에서는 실제 예약·사용 상태를 다시 확인한다.

## test1 자동 로그인

2026-11-04 사용자 요청으로 test1은 로컬 보호 정보를 재사용한다. 실제 Windows 사용자의 LocalApplicationData/UnityCoordinator/test1-credential.xml은 DPAPI 보호 파일이다. Git에 복사하거나 내용을 출력하지 않는다. sandbox LOCALAPPDATA로 경로를 추정하지 않는다.

현 도우미: ROOT/UnityClient/Builds/ClientGrowthHUD/verification/netlib65/live/protected-login.cs와 read-protected-login.ps1. 정상 로그인 UI에 입력하며 비밀번호를 명령 인자·파일·로그에 풀어 저장하지 않는다. 2026-11-04 실제 인증·월드 진입은 Computer Use의 정상 입력란 선택·type_text·로그인 클릭으로 확인했다. 보호 reader stdout은 메모리에만 두고 입력한다. 기존 C# 합성 입력 도우미의 완료 반환은 인증 성공 증거가 아니며, 현재 검증한 경로는 native UI 입력이다. 입력 허용과 대상 창을 확인한 뒤 사용하고 정책 거절을 우회하지 않는다.

## 바탕화면 테스트 실행 환경 — 2026-11-04

바탕화면 `MMORPG 테스트 시작.bat`와 `MMORPG 테스트 종료.bat`는 ROOT의 기존 StartGame/StopGame 도우미를 호출한다. 최신 Windows Player·MainServer·로그인 앱의 비밀 제외 소스를 ROOT/UnityClient/Builds/LocalGame/Aim360에 배치했다. 실행 경로는 ROOT/tools/local-game/settings.json에서 관리하며 임시 Git 작업 폴더 삭제에 의존하지 않는다. 로그인 `.env`와 설치된 Node 의존성은 기존 Game/Login에서 읽고 복제하지 않는다.

Unity CLI beta.16의 Windows 독립 빌드와 MainServer 빌드 성공. 현재 refactor의 WaitCloseSignal에 없던 선택형 MMORPG_STOP_FILE 종료 지원만 추가해 기존 도우미와 연결했다. _dupenv_s/RAII로 경로를 읽으며 기존 fatal/q 종료를 유지한다. 최초 getenv MSVC C4996은 같은 위치의 보안 API로 수정, 서버 재빌드 성공. 독립 리뷰 차단0.

시작 BAT에서 게임32536/MainServer2092/Login24832·포트10099/3000 준비 확인 후 종료 BAT로 세 프로세스 정상 종료했다. state.phase=stopped, 종료 후 두 포트 리스너0. 캡처 도구는 다른 게임 화면을 반환해 독립 Player 로그인 화면을 시각 확인했다고 보고하지 않는다. 실제 로그인·플레이 검사는 반복하지 않았다. 기존 실서버 조준 검증과 이번 BAT 수명 검증을 구분한다.

검증 후 동일 로그인 소스를 고정 실행 묶음으로 복사하고 app.js 해시 일치 및 경로 check를 확인했다. 비밀 파일은 복사하지 않았다. MySQL·Redis는 기존 실행 상태를 유지하며 BAT가 시작/종료하지 않는다. 서버 강제 종료나 정책 변경 없음. 상세 로그 Builds/ClientGrowthHUD/verification/desktop-launch 및 LocalGame/Run.

## 창 모드·포인터 — 2026-11-04

다음 BAT 실행부터1280×720 창 모드로 시작한다. ROOT/tools/local-game/settings.json의 ClientArguments(-screen-fullscreen0/-screen-width1280/-screen-height720)를 기존 실행 도우미의 ArgumentList로 전달한다. 변경한 도우미는bin-windowed에별도빌드해이전실행중인프로세스를교체/종료하지않았다. 현재BAT대상은LocalGame/Aim360/Player-Windowed/MMORPG.exe. 설정경로check와dotnetpublish성공, Player독립빌드성공.

UnityAPI로PlayerSettings.fullScreenMode=Windowed/defaultIsNativeResolution=false/1280×720/resizableWindow=true,48px GameCursor.png/hotspot(5,4)/readable Cursor importer를설정하고정본ProjectSettings와.meta에보존했다. 에디터가닫혀있어CLIheadlessPipeline명령으로설정했으며제품코드의매프레임커서매니저는추가하지않았다. 밝은면/어두운외곽선으로바탕대비를높인직접제작도형이고원본SVG를함께보존한다.

빌드·설정근거 Builds/ClientGrowthHUD/verification/windowed-cursor. 포인터는하드웨어커서라게임화면캡처에포함되지않을수있다. 현재단계에서실제OS커서조작감평가나새로그인검사를반복하지않았다. 최종패링묶음의입력검증과구분한다.

## 패링 묶음 실행 — 최신

바탕화면BAT의현재대상은ROOT/UnityClient/Builds/LocalGame/Parry/Player/MMORPG.exe와동일폴더의Server/Login이다. 창모드인자와커서는유지한다. 새Windows빌드·서버해시·Launcher check를확인했으며사용자입력보류때문에이번묶음을기동/로그인하지않았다. 이전BAT시작/종료검증을패링실전검증으로재사용하지않는다.
