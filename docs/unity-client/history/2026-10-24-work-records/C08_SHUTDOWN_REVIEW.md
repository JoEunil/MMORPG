# C08 로컬 서버 정상 종료 로그 독립 검토

2026-10-19 서버 분석 담당. 소유는 이 문서뿐이다. 제품·도구 코드 변경, 서비스 조작, 빌드·테스트·Editor 조작 없음. `NetLibrary/IOCP.cpp`의 현재 diff는0이다.

## 결론

C07 서버의 이번 종료 로그는 **기존 IOCP 종료 중 Accept 슬롯 재보충 경합**과 일치한다. 이전의10038만 나온 경우와 이번10038/10022 혼합은 같은 경로로 설명된다. C08 후보는 아직 기동되지 않았으므로 이번 로그를 C08 기능 회귀로 볼 근거는 없다.

정상 종료 후 별도의 start-only 실행을 검토할 수 있다. 단, 이는 종료 로그가 깨끗했다는 판정이나 guard 우회가 아니다. Coordinator가 실제 프로세스·포트·WAL·DB의 정지 후 상태와 후보 이미지/복구 조건을 다시 확인해야 한다. 이 분석은 그 확인이나 새 기동의 성공을 대신하지 않는다.

## 직접 확인한 증거

증거 디렉터리: `UnityClient/Captures/r01-switch-20261019-145128-238-1760-evidence/`.

- `before-current-stop-error.log`138바이트가 `stopped-Original-error.log`2542바이트의 동일 prefix임을 읽기 비교했다. delta는 정확히16행이며 모두 timestamp `1792389097505`다.
- 신규 행: `GetQueuedCompletionStatus failed`,995×8; `Critical: failed to post AcceptEx`,10038×4와10022×4. 기존 CancelIoEx1168 경고는 prefix의 이전 행이며 이번 delta가 아니다.
- system.log의 `IOCP CleanUp`은1792389097504, worker stopped8개는7505다. cache MQ stopped3개, DB worker stopped4개와 WAL manager stopped까지 뒤따른다.
- 전환 receipt는 q helper native exit0 후 `stopped-Original` 로그/WAL 보존과 post-stop read-only persistence 기록까지 도달하고 stop evidence에서 중단됐다. 후보/복구 기동이나 강제 종료 기록은 없다.
- `Switch-R01LocalServer.ps1::Stop-Normally`는 server WaitForExit와 native exit0 검사, Assert-PortFree를 통과한 뒤 위 보존/DB 기록을 수행한다. 마지막 `Normal native exit0 confirmed` 문장은 error delta guard 뒤라 남지 않은 것이다. 따라서 이 한 문장의 부재만으로 native exit 확인도 안 됐다고 해석하지 않는다.

DB 값 동일·WAL 빈 상태·현재 포트 부재의 별도 실측은 Coordinator가 담당한다. thread stop 로그를 개별 SQL commit 영수증으로 해석하지 않는다.

## 실제 코드 경로와10022 해석

1. `IOCP::CleanUp`(106행 이후)는 `m_isRunning.exchange(false)` 뒤 listen socket을 closesocket한다. 닫은 `m_listenSock` 숫자를 INVALID_SOCKET으로 바꾸지 않는다.
2. worker는 while 조건을 이미 통과해 GQCS에서 대기 중이다. listen 종료로 accept 완료가995로 돌아오면 실패 branch(180행 이후)에서 accept buffer/소켓을 정리한 다음 running 상태를 재검사하지 않고 `PostAccept()`를 호출한다.
3. `PostAccept`(274행 이후)는 새 WSASocket을 만든 다음 여전히 보존된 `m_listenSock` 값을 AcceptEx의 listening socket 인자로 사용한다. 이 함수에도 종료 중 early-return이 없다.
4. 아직 그 숫자가 유효 socket이 아니면10038(WSAENOTSOCK)이 가능하다. 그 숫자가 새 socket으로 재사용되어 listening 상태가 아니면10022(WSAEINVAL)가 가능하다. 재보충8개의 실패가 모두 동일 밀리초에 기존 종료 직후 나타나는 패턴과 맞는다.

Microsoft는 closesocket 직후 descriptor가 재사용될 수 있으며, 이후 접근이 언제나 WSAENOTSOCK으로 실패하리라고 기대할 수 없다고 명시한다. [closesocket 공식 문서](https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-closesocket). WSAEINVAL 설명에는 listening하지 않는 socket의 accept가 포함된다. [Winsock 오류 코드 공식 문서](https://learn.microsoft.com/en-us/windows/win32/winsock/windows-sockets-error-codes-2).

이번 로그에는 AcceptEx에 전달한 listening handle/새 accept handle 값이 없으므로 **10022 각각의 descriptor 재사용은 직접 관찰이 아닌 코드와 공식 API 계약에 따른 추론**이다.10022라는 숫자 하나만을 일반적인 허용 목록으로 만들면 안 된다. 여기서는 종료 시점·실제 재보충 경로·8+8 delta·기존 제품 diff0를 함께 판단했다.

## 처리 경계

- 기존 error delta guard를 수정하거나 로그를 삭제/비우지 않는다. 중단 receipt와 모든 before/after 증거를 보존한다.
- 이번 분석만으로 성공 전환 처리나 자동 fallback을 수행하지 않는다.
- Coordinator가 종료 후 상태와 저장 증거를 별도로 확인하고, 검증한 C08 이미지/관련 DLL과 C07 복구 이미지를 확인한 뒤 start-only의 기존 엄격한 준비·ready·오류 조건으로 판단한다.
- NetLibrary 종료 재보충 경합은 잔존 결함이다. C08 Zone 후보와 무관하게 고치지 않았으며, 다음 정상 종료 때 재발할 수 있다. 별도 원인 단위 후보로 다루어야 한다.
