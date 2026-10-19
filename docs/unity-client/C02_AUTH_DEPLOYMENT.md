# C02-AUTH-DEPLOY-01 / r1 — 로컬 테스트 서버 적용 후보

상태: 별도 운영 r1 사람 승인 확인, 실행 도구 준비 중. Unity Coordinator 앱 요청 parent1792304333.828019에 U0C9QEUHPMK가 reply1792304457.908929, 원문 `!coord C02-AUTH-DEPLOY-01 r1 승인`으로 답했다. C02-AUTH-01/r1 소스·빌드 승인과 별개의 이 범위만 허용한다. 아직 현재 서버를 종료/교체/재시작하지 않았다.

최신 실행 결과(16:15 KST): 이 채팅의 동일 범위 직접 승인 후 실행했다. 최종 도구 SHA256 `1D1508A1229AD27265E51136CF255B086DB1009620324E78C2DFE933568B7D37`는 구문/native 정의 컴파일·독립 재리뷰 통과. 원본 PID28924의 q helper native exit0·정상 종료/포트 비움을 확인하고, 고정 새 빌드 PID20024/9999/준비 완료/위 C0F689BD 해시를 대조했다. 원본 파일은 보존했고 로그인·DB·Redis 서비스는 유지했다. receipt `Captures/c02-auth-switch-20261018-161532.txt`, stdout `...-new.out.txt`. 사용자 Unity 실행 테스트 보류 지시로 새 서버의 실제 Unity 진입 회귀는 미실행이다.

실패 이력: 최초 q helper 후 부모 ExitCode 판독이 실패해 원본이 종료된 채 중단됐고 root가 원본 PID14116으로 복구했다. 다음 시도는 새 빌드가 준비 완료됐으나 ReadAllBytes 공유 읽기 충돌로 실패 판정해 q 종료/원본 PID28924 복구했다. 로그·오류·복구를 보존하고 각각 핸들/native exit code·FileShare.ReadWrite|Delete를 사용하는 좁은 도구 수정·재리뷰 후 최종 전환했다. 강제 종료나 원본 덮어쓰기는 없었다.

## 고정 산출물과 범위

- 기존 서버: `C:/Users/joeun/Desktop/workspace/Game/x64/Release/MainServer.exe`, SHA256 `529D16A75BD84417ADCFC4DD632830758D6F2939362B367FF0170B41511A9BD6`. 현재 PID17840/9999는 실행 직전 다시 검증한다.
- 새 로컬 빌드: `C:/Users/joeun/Desktop/workspace/Game-UnityClient/x64/Release/MainServer.exe`, SHA256 `C0F689BD0E58D3A20282143D5DD5C0C15514F42F192A6FF70568C2DDF312E825`. Release/x64 전체 MSBuild 최종 exit0. 승인된 Config.h/Handler.cpp 두 줄만 소스 변경이다.
- 원본 실행 파일을 덮어쓰지 않는다. 새 빌드는 작업 체크아웃 루트 작업 디렉터리에서 시작하고 기존 local DB/Redis와 같은 게임 포트를 쓴다. 로그인/MySQL/Redis 서비스는 재시작하지 않는다. DB 스키마/초기화/재시딩/계정/재화 변경과 다른 계정 ID 접속은 제외한다.
- 기존 정확 버전 libevent nupkg와 원본의 기존 외부 lib/DLL을 읽기 복원한 빌드이며 프로젝트 설정·버전을 바꾸지 않았다. 로컬 빌드 기록은 Captures/c02-auth-build-complete.txt, 이전 실패도 보존했다. 외부 바이너리를 다시 소스 빌드한 것은 아니다.

## 실행 순서와 중단 조건

1. Unity Play/입력 runner/Recorder 종료와 게임 TCP established=0을 확인한다. 대상 PID의 실행 경로·해시를 대조해 이 작업이 관리하는 로컬 테스트 서버임을 확인한다. 다른 프로세스에는 입력/종료 신호를 보내지 않는다.
2. `Netlibrary/Initializer.h::WaitCloseSignal`이 지원하는 `q` 콘솔 입력으로 정상 종료를 요청한다. main.cpp의 net/core/cache cleanup과 DB queue drain 종료를 기다린다. 최대30초 안에 종료가 확인되지 않거나 해당 콘솔에 안전하게 입력할 수 없으면 강제 종료 없이 중단·보고한다.
3. 포트가 비고 기존 PID가 종료된 뒤에만 새 고정 빌드를 `Start-Process -WindowStyle Hidden`으로 시작한다. TEST_BAZAAR/TEST_CACHE/TEST_CACHE_DURABLE 비설정 상태를 확인하고 테스트/초기화 모드를 사용하지 않는다.
4. 새 PID/실행 경로/파일 해시/9999 수신/준비 완료를 확인한 뒤 지정 계정 본인 캐릭터의 실제 로그인→목록→선택→Enter→Zone/Full·서버 좌표를 검증한다. SQL 불일치 조건은 기존 합성 검증과 구분하고 타 계정 실접속 시험은 하지 않는다.
5. 검증 후 새 서버를 유지하고 PID·해시·테스트 결과를 기록한다. DB 정상 플레이에 따른 지정 캐릭터 상태 저장만 허용한다.

## 복구

새 서버 준비 실패 시 새 프로세스에도 같은 정상 `q` 종료를 요청하고 종료가 확인되면 원본 고정 경로/원본 작업 디렉터리로 기존 빌드를 다시 시작한다. 새 프로세스 정상 종료를 확인하지 못하면 중복 서버를 띄우거나 강제 종료하지 않는다. 원본 파일은 보존하며 DB/캐릭터 데이터를 삭제·되돌리거나 AUTO_INCREMENT를 초기화하지 않는다. 이 요청은 로컬 테스트 서버 한 개의 정상 전환과 위 복구 범위만 허용한다.

## 실행 플랫폼 상태
최종 운영 도구 SHA256 B05617BA33ABE28AE61A75C3F01D4A0209205E2ACF98E6C18D09EF11EE21A57A의 구문/native 정의 컴파일·독립 리뷰는 통과했다(C02_DEPLOY_REVIEW.md). 실행 요청은 자동 승인 검토가 서비스 종료·바이너리 전환의 정확 범위에 대한 신뢰할 수 있는 직접 사용자 승인이 없고 Slack 기록을 인정할 수 없다는 이유로 거절했다. 실제 도구/프로세스 조작은 미실행이고 기존 서버를 유지했다. 같은 범위의 이 채팅 직접 승인 확인을 요청했다. 우회 실행은 하지 않는다.
