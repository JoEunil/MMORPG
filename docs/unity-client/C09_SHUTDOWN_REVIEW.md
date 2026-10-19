# C09 정상 종료 중단 — 독립 검토

2026-10-19 서버 분석 담당. 이 문서만 작성했다. 제품·도구 수정, 서비스·Editor·Git 조작과 DB 쓰기는 하지 않았다.

이번 C08 종료의 신규 오류는 기존 IOCP 종료 시 Accept 슬롯 재보충 경합과 일치한다. C09 후보는 아직 시작하지 않았으므로 이번 로그를 C09 거래소 기능의 실행 결과나 회귀로 볼 근거는 없다. 종료 로그가 깨끗했다거나 전환이 완료됐다는 판정은 아니다. 기존 중단 guard를 유지하고 정지 후 상태를 별도로 확인한 다음, Coordinator가 엄격한 start-only 준비 조건으로 새 기동을 판단할 수 있다.

## 직접 읽은 증거

요청에 적힌 161741 접두 파일은 없었다. 실제 최신 전환 receipt는 [r01-switch-20261019-161737-878-19764.txt](../../UnityClient/Captures/r01-switch-20261019-161737-878-19764.txt), 증거는 동일 stem의 `-evidence/`다. 161710 receipt는 앞선 preflight-only이며 종료 입력을 보내지 않았다.

- `before-current-stop-error.log` 416바이트가 `stopped-Original-error.log` 2824바이트의 동일 prefix임을 바이트 단위로 확인했다. 신규 2408바이트는 정확히 16행이다. `GetQueuedCompletionStatus failed`/995가 8개, `Critical: failed to post AcceptEx`/10038이 8개이고 10022는 없다. timestamp는 `1792394269008`과 `1792394269009`뿐이다. 기존 CancelIoEx/1168 3개는 prefix에 있으며 신규 delta가 아니다. 현재 `logs/error.log`도 보존된 stopped 파일과 동일했다.
- system 로그의 `IOCP CleanUp`은 `1792394269008`, IOCP worker stopped 8개는 9008~9010이다. JSON 행에서 cache MQ stopped 3개와 DB worker stopped 4개가 9023~9024에 확인되고, WAL manager thread stopped는 9039다. 종료 순서와 오류 발생 시점이 일치한다. 이들은 개별 SQL commit 영수증이 아니다.
- before/post-stop 보존 persistence 파일은 모두 `READ ONLY`이고 두 `characters` 배열이 동일했다. 보존된 stopped `redo.1`과 `backpressure.1`은 각각 0바이트다. 이는 보존 증거 비교이며 현재 프로세스·포트 관찰을 대신하지 않는다.

## helper 종료와 서버 종료를 구분한 판단

Receipt에 직접 찍힌 native exit 0은 quit helper PID 37076의 결과다. 이것만으로 서버 종료 성공을 주장하지 않는다.

별도로 `Switch-R01LocalServer.ps1::Stop-Normally`의 실제 순서를 확인했다. 서버 `WaitForExit`의 남은 30초 제한과 서버 native exit code 0 검사를 통과하고 `Assert-PortFree`까지 통과해야 `stopped-Original` 보존과 `post-stop-Original` 읽기 스냅샷을 기록한다. 두 기록이 receipt에 있으므로 그 선행 검사들은 통과한 경로다. 이후 `Assert-StopEvidence`의 첫 error-log 길이 비교에서 416→2824 증가를 발견해 중단했다. 마지막 `Normal native exit0 confirmed` 문장은 이 guard 뒤에 있어 남지 않았다. 중단 후 후보·복구 기동 기록은 없고, 강제 종료도 없었다.

system 로그 전체가 JSON 파싱을 통과한 것은 아니다. before 파일 1행에는 기존 `Connected to Redis!(session)`, stopped 파일에는 같은 1행과 새 158행 `server stop`이 비JSON이다. `Assert-StopEvidence`의 strict `ConvertFrom-Json`은 이번 실행에서 앞선 error-length 중단 때문에 도달하지 않았다. 위 종료 개수는 독립 검토에서 JSON 행을 구분해 관찰한 값이며, 도구의 strict parser가 통과했다고 바꾸어 보고하지 않는다. 이 문서 작성을 위해 parser나 guard를 수정하지 않았다.

## 코드와 이전 종료 검토의 비교

[이전 C08 종료 검토](C08_SHUTDOWN_REVIEW.md)의 GQCS/995 8개와 AcceptEx/10038·10022 합계 8개 패턴과 같은 재보충 경로다. 이번에는 AcceptEx가 모두 10038이었다. 현재 `NetLibrary/IOCP.cpp`는 PR28-P2의 C07 기준 동일 파일과 SHA256 `D87D5C3D76E2E7B5403600508ADA361F3E3565D7DDB6451D2B2321BC2DBC204B`로 일치한다.

`NetLibrary/IOCP.cpp`의 `CleanUp`(106행)은 running을 false로 바꾸고 listen socket을 닫는다. 이미 GQCS에서 대기하던 worker의 실패 branch(180행)는 ACCEPT buffer/socket을 반환한 뒤 running 상태를 다시 확인하지 않고 `PostAccept()`를 호출한다. `PostAccept`(274행)는 종료 중 early-return 없이 닫힌 listen 값으로 AcceptEx를 시도한다. 따라서 종료 취소 완료와 재보충 실패가 같은 시점에 나타나는 이번 로그를 설명한다. 각 handle 값을 계측한 증거는 없으므로 개별 handle의 내부 상태까지 직접 관찰했다고 주장하지 않는다. 이 잔존 종료 경합은 수정하지 않았다.

## start-only 판단 경계

Coordinator는 별도 escalated 읽기에서 현재 MainServer 프로세스 0, 9999 항목 0, 현재 WAL 두 파일 각각 0바이트, 전후 dedicated character JSON 동일을 확인했다고 전달했다. 이 담당의 sandbox는 앞선 작업에서 프로세스/CIM 시야가 달랐으므로 해당 현재 상태를 독립 재측정한 것으로 기록하지 않는다.

기존 error delta guard와 비JSON strict-parse 한계를 모두 보존한 채, 위 종료 분석과 Coordinator의 현재 상태 확인을 근거로 별도 start-only 실행을 검토할 수 있다. 실행 직전 C09 이미지와 DLL 고정 해시, C08 복구 이미지, TEST 환경 변수 부재, MainServer/포트 부재, 빈 WAL, 로그/WAL 보존, 두 번의 READ ONLY 스냅샷 안정성을 다시 확인해야 한다. 이후 30초 ready·실제 DLL 경로·stderr/error log 조건도 기존대로 적용한다. 로그 삭제·초기화, guard 허용 목록 추가, 자동 재시도·복구는 이번 분석에 포함하지 않는다. 실제 새 기동과 그 성공 여부는 Coordinator의 다음 실행에서 별도로 기록한다.
