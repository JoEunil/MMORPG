# 로컬 환경 대조와 정상 전환

현재 목표·입력·실행 후보는 HANDOFF, 권한은 AGENTS.md를 따른다. 아래는 2026-10-24 감사 관찰과 확인 절차이며 영구 실행 상태가 아니다.

## 위치

- Editor: C:/Users/joeun/Desktop/workspace/Game-UnityClient/UnityClient
- Git 정본: 위 프로젝트 Builds/C10-Delivery/source
- 사용자 서버: C:/Users/joeun/Desktop/workspace/Game — 사용자 AI/미완료 변경 보존
- ClientCore: 정본 ClientCore/ClientCore.csproj → UnityClient/Assets/Plugins/ClientCore/ClientCore.dll
- 기존 비밀: 실제 사용자 LOCALAPPDATA/UnityCoordinator의 Windows 계정 암호화 파일. 내용을 출력하지 않는다.

## 실행 전

실제 프로젝트·컴파일·Play·프레임, 서버 실행 파일·작업 폴더·소스 기준·DLL, 포트·소유 프로세스, TEST_BAZAAR/TEST_CACHE/TEST_CACHE_DURABLE 초기화 모드 미설정을 확인한다. 전용 계정·아이템·재화가 부족하면 미검증으로 남기며 DB seed로 통과시키지 않는다.

감사 시 Unity6000.0.47f1/Pipeline7800, Login3000·MySQL3306·Redis6379·미리보기8791 수신, MainServer9999 미수신을 확인했다. 옛 PID나 파일 존재로 현재 실행/소스 일치를 단정하지 않는다.

전용 unity_ai_01/02와 기존 비밀을 재사용한다. 비밀번호 덮어쓰기·DB HP복구·재화 seed·초기화는 하지 않는다. 정상 게임 상태 변경과 DB 직접 수정을 구분한다.

## 정상 종료·복구

### 자동화 키 입력 구분

2026-10-25 현재 Computer Use `press_key`의 I/Shift+I/Tab은 Unity 원시 추적에서 TEXT로 관찰됐으며 해당 키의 STAT 눌림은 관찰되지 않았다. 이 상태에서 `Keyboard.current.*Key.wasPressedThisFrame`이 반응하지 않는 것을 제품 단축키 결함으로 단정하지 않는다. 포커스·프레임·장치와 원시 이벤트를 구분해 확인하고 실제 물리 키 입력과 비교한다. 사용자 물리 입력은 같은 관찰에서 I/B 각각1회 정상 감지됐다. 이 차이로 게임 단축키를 변경하지 않는다. 도구 내부 구현/다른 버전의 동작은 추정하지 않는다.

자동화된 게임 입력이 필요하면 기존 `C01EditorInputRun`과 정상 PlayerLoop가 소비하는 `InputSystem.QueueStateEvent` 경로를 재사용할 수 있다. 명시적으로 합성 키 입력이라고 기록하고 native/물리 키 검증을 대신했다고 보고하지 않는다. 강제 `InputSystem.Update`나 제품 핸들러 직접 호출로 성공을 만들지 않으며, 키 해제·관찰기 정리를 보장한다. 도구 문제를 숨기기 위해 TEXT 이벤트를 게임 단축키로 연결하지 않는다.

## 정상 종료·복구

현재 대상을 식별하고 기존 q 정상 종료·저장을 확인한다. 기존 전환 도구의 시간 제한·복구 조건을 지킨다. 종료 미확인 시 강제 종료하지 않는다. 원본 빌드·복구 위치를 보존하고 서버/DLL 동시 버전이 필요한 변경은 혼용하지 않는다.

부모 shell 정책을 자식 런타임 정책으로 가정하지 않는다. 실제 실행 파일의 정책·표준입력·로그로 도구 차단과 제품 결함을 구분한다. 과거 STOP01의 프로세스 한정 정책 승인은 미래 변경 승인이 아니다. 다른 shell/inline 경로로 우회하지 않는다.

종료 시 Play·background·임시 설정·연결과 서버 유지/종료 상태를 복원·기록한다. MySQL/Redis 재시작·계정/기존 데이터 변경은 별도 승인 경계를 유지한다.

초기 DB 준비·승인·실행 기록은 [원문 이력](history/2026-10-24-guideline-audit/README.md)에 보존한다.
