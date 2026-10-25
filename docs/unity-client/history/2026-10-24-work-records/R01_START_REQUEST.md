# R01-START-05/r1 — 후보 서버 1회 기동 승인 요청

**현상:** 수정 후보는 빌드·격리 회귀 29개 통과 상태다. 기존 서버가 이미 없어 R01-DEPLOY-04/r1의 정상 종료 증거를 만들 수 없다. 기존 서버가 왜 종료됐는지는 미확인이다.

**요청 동작:** 별도 Slack 변경 스레드에서 지정 사용자의 승인을 받은 뒤, `UnityClient/Tools/Start-R01Candidate.ps1`로 준비 증거를 보존하고 고정 후보를 한 번 시작한다. 후보는 `UnityClient/Builds/R01-Candidate/bin/MainServer.exe`, SHA256 `01C6261AB5C3448E2BA5F336CE70FB20E4DBA4B9ED47AD70002F22C88594D108`; 저장소 루트에서 인자 없이 hidden으로 실행하며 포트9999를 사용한다. 필수 DLL 5개의 SHA256은 도구에 고정돼 있다.

도구는 시작 전에 MainServer 프로세스와 포트9999 사용이 모두 없고, Process/User/Machine 범위의 TEST* 변수가 unset인지 확인한다. 네 로그와 숫자 suffix `redo.*`/`backpressure.*` WAL을 증거 폴더에 복사하고 WAL이 모두 0바이트인지 확인한다. 고정 READ ONLY inspector로 지정 캐릭터 5011·5012를 두 번 읽어 값이 같은지 확인한다. 하나라도 불일치·조회실패·사본실패면 시작하지 않는다.

시작 후 30초 안에 PID·이미지 경로/hash, 포트9999 listener, 서버 준비 문구, 실제 로드된 DLL 3개의 경로/hash, 빈 error.log, 접속0을 모두 확인한다. 모호한 시작 결과나 readiness 실패에서는 재시도·정상종료 입력·강제종료·C0 복구를 하지 않고 증거와 실제 PID를 보고하고 멈춘다.

**서버 변경:** 서버 소스 변경 없음. 후보 MainServer 시작은 기존 DB 초기화/cache 경로를 실행할 수 있어 데이터 전체 무변경을 보장하지 않는다. 두 캐릭터 READ ONLY 조회 외 DB 쓰기, 스키마/계정/재시딩/복구, WAL 재생, Login/MySQL/Redis/원본 Game 변경, 서버 강제종료는 범위에 없다.

**검증:** 수정본의 이전 격리 테스트 10+19=29개 통과. 신규 도구 최종 SHA256 `846FAF75C273C4E8FB2E4FC57DC0AF2D81AF08BC7B92D55DEA990D97147DD0D0` / PowerShell 파서 오류0. 실제 사전검사는 이전 `67F283FC…` 버전으로 통과했다(`UnityClient/Captures/r01-start-20261019-124142-072-25560.txt`): 로그4종·빈 WAL2종의 사본/hash, 두 번의 READ ONLY 결과 일치, 프로세스/포트 없음, 고정 파일과 환경 확인. 최종 버전은 시작 분기의 30초 제한만 보완했고 사전검사 경로는 동일하다. 서버 기동은 하지 않았다. 실제 시작 직전 모든 조건을 다시 확인한다. 일반 preflight는 다음처럼 호출한다. 이 명령은 로그/WAL/READ ONLY 증거만 준비하고 종료하며 서버를 시작하지 않는다.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File UnityClient/Tools/Start-R01Candidate.ps1 -PreflightOnly
```

실제 시작은 별도로 승인된 식별자를 명시하는 호출에서만 가능하도록 도구를 막아 두었다. 요청 명령:

`!coord 승인 R01-START-05/r1 후보 서버 1회 기동 재개`
