# R01 — 정상 종료·후보 전환 운영 분석

2026-10-18. 서버 변경 요청 역할의 **읽기 분석/운영안 초안**이다. 이 문서와 `R01_DEPLOY_REQUEST.md`만 소유한다. 서버/Tools/DB/Unity/설정/빌드/프로세스/네트워크를 수정하거나 실행하지 않았다. R01-CLOSE-03의 후보 구현·격리 빌드 권한은 실행 서버 전환을 포함하지 않는다. Coordinator의 정확한 별도 운영 요청은 R01-DEPLOY-04/r1이며 아직 이 문서로 승인되지 않았다.

## 대상과 현재 증거

Coordinator가 이번 배정 직전 읽기 확인한 기존 프로세스는 다음과 같다. 분석자가 직접 PID/포트를 조회한 결과가 아니며 실행 직전에 재확인한다.

| 항목 | 고정값 |
| --- | --- |
| 기존 PID / StartTime UTC ticks | 20024 / 639279045410504905 |
| 기존 이미지 | C:/Users/joeun/Desktop/workspace/Game-UnityClient/x64/Release/MainServer.exe |
| 기존 이미지 SHA256 | C0F689BD0E58D3A20282143D5DD5C0C15514F42F192A6FF70568C2DDF312E825 |
| 기존 인자 / 실행 cwd | 인자 없음 / C:/Users/joeun/Desktop/workspace/Game-UnityClient |
| 후보 이미지 | C:/Users/joeun/Desktop/workspace/Game-UnityClient/UnityClient/Builds/R01-Candidate/bin/MainServer.exe |
| 후보 이미지 SHA256 | 01C6261AB5C3448E2BA5F336CE70FB20E4DBA4B9ED47AD70002F22C88594D108 |
| 후보 실행 cwd / 서비스 포트 | 위 저장소 루트 / 기존 9999 |
| 복구 이미지 | 위 **현재 C0 이미지**. 원본 Game의 529D16A… 빌드는 복구 대상 아님 |

`UnityClient/Captures/r01-close-candidate-ready.txt`와 `r01-close-candidate-regression.txt`를 직접 읽었다. UnitTests·MainServer 격리 빌드 exit0이며 MainServer는 미실행이다. UnitTests SHA5504D57C…의 별도 tester 타깃10/10(8ms)·Net19/19(1ms)은 각1회·exit0·재시도 없이 통과했다. 구현 source 리뷰는 `R01_CLOSE_IMPLEMENTATION_REVIEW.md`의 고정4파일에 차단 문제가 없다는 판정이다. 운영 도구 최종 리뷰/미확정 확인이 끝나기 전에는 실행 요청을 확정하지 않는다. 실행 중 이미지 전체와 현재 소스가 일치한다고 단정하지 않는다.

후보의 인접 DLL 전체 manifest는 `r01-close-bin-manifest.txt` SHA `A5326D020D7343B0A223DADD81460712FDB4265C11DF7359E185F3F0BBEF8D54`다. 이는 distribution 파일 목록이며 모두 MainServer가 실제 로드한다는 뜻은 아니다.

| 인접 DLL | SHA256 |
| --- | --- |
| event_core.dll | 028EF81FD65415BC48F5E2AB0AC073CC80E4F0496CFEF8F5CBB3380672710B17 |
| event_extra.dll | 400755B1387783C49DAA61192069947355BB9AF2A252AF1D6B919A845CBB5780 |
| event.dll | FBA74431B5441EB7E5A4F48DEE890796A331E923FCFDE003EC71C8CAAF618FD0 |
| hiredis.dll | FC0D6F13F43ADE48A71F9FF3AFD9B9B0751165FC325D3A05D19A01999D29FE5C |
| mysqlcppconn-9-vs14.dll | 2070CD998CDF625425DE4AA5476F131D92165068998CC634B13C4D3138E0F088 |
| gtest.dll | 4C6C96787FD9B6B39D76F2A487C15D4ED36F3BF0E6BA524AEE090C093EBE9B04 |

Coordinator가 현재x64와 후보의 필수5 DLL(event3/hiredis/mysqlcppconn)이 같은 hash임을 읽기 확인했다고 전달했다. gtest.dll은 테스트 배포 파일이며 필수5에 넣지 않는다. 직접 읽은 최종 preflight evidence `r01-switch-20261018-224845-836-20632-evidence/loaded-20024.txt`는 현재PID가 실제 로드한 event.dll/hiredis.dll/mysqlcppconn-9-vs14.dll의 경로가 모두 기존x64/Release이며 위 hash와 같음을 기록한다. manifest SHA는 `079C3D2EFBD5D617D6227F727CEF427054BEE84BC6AAAC7D180F4A4E4826BC68`다. event_core/extra는 인접 배포 파일로 hash를 검사하되 현재 로드된 것으로 표현하지 않는다. 이 검사는 **로드 모듈 경로와 그 경로의 디스크 hash**이며 메모리 이미지 hash가 아니다. 시작 후 후보/조건부 복구 PID에도 이 실제로드3종 검사를 적용한다. 모든 비시스템 모듈을 허용목록 검사하는 도구는 아니며 Windows 시스템 DLL을 임의 교체하지 않는다.

## q 종료가 기존 미저장 상태에 미치는 영향

`MainServer/main.cpp`의 실제 순서는 `net.CleanUp1 → core.CleanUp1 → core.CleanUp2 → net.CleanUp2 → cache.CleanUp`이다. `_getch()`의 q/Q는 이 경로로 들어간다. `Net::WaitCloseSignal`도 CleanUp1을 부른 뒤 main에서 재호출하며 Stop은 현 코드상 idempotent다. 문서의 오래된 순서 설명보다 아래 소스를 기준으로 한다.

1. `NetLibrary/Initializer.h::CleanUp1`은 ping·새 수신 재게시·net perf를 멈춘다. `CoreLib/Initializer.h::CleanUp1`은 Zone tick/broadcast/chat/perf를 멈춘다. 현재 TCP0이라도 이미 남은 논리 캐릭터는 Zone 정지 전까지 tick 영향을 받을 수 있다.
2. `core.CleanUp2`는 NonZoneThreadPool Stop/join → Core recvMQ Stop/join → StateManager::CleanUp 순이다. NonZone의 기존 마지막 PacketView도 worker 종료 때 해제될 수 있다. Stop 중 이미 m_running=false면 새 Disconnect enqueue는 무시될 수 있으므로 최종 저장은 아래 StateManager 순회까지 확인한다.
3. **Core recvMQ는 drain이 아니라 hard stop**이다. `CoreLib/InMemoryQueue.cpp`는 running=false에서 worker를 끝내고 남은 응답 Message를 pool로 반환한다. 늦은 load 응답을 반드시 실행한다고 주장하지 않는다. 이미 월드/로비에 들어간 상태의 저장과 별개다.
4. `StateManager.cpp::CleanUp`은 shard lock 아래 등록된 session을 순회한다. zone0은 lobby.Emigrate, 다른 zone은 ZoneState.Emigrate로 현재 CharacterState를 복사하고 실제 캐릭터를 제거한다. 성공한 각 snapshot을 `MSG_CHARACTER_STATE_UPDATE`로 Cache 큐에 직접 넣는다. 찾지 못한 Zone/character는 건너뛰고 enqueue 실패는 로그를 남긴다. 이 경로에는 정상 Disconnect의 **`Enqueue Disconnect` 성공 로그 호출이 없다.** 따라서 q 뒤 그 문자열 부재만으로 저장 실패를 판정하면 안 된다.
5. 이 시점 Cache는 살아 있다. `CacheLib/InMemoryQueue.h/.cpp`는 Important UPDATE를 복사하고 Drain=true queue로 처리한다. `CacheLib/Initializer.h::CleanUp`은 recvMQ Stop/join 뒤 bazaar/game DBWorker Stop/join을 수행한다. `DBWorker.h`는 queue가 비어야 종료하므로 이미 수락한 캐릭터 UPDATE 작업은 반환 전에 실행된다. 이후 dispatcher.Stop → flush.Stop → WAL.Stop → CacheTimer.Stop이 진행된다. 이 순서를 반대로 기록하지 않는다.
6. `Handler.cpp::CharacterStateUpdate`는 body 값을 캡처해 QUERY4 UPDATE를 실행한다. 실패는 로그를 남기지만 **캐릭터별 성공/commit receipt는 없다.** 정상 exit0와 worker stopped만으로 특정 행의 최신값을 입증하지 않는다. queue 거부/SQL 오류/생략/내구성 fallback도 각각 따로 확인한다.

기존 로그의 마지막 session15는 TCP 종료 뒤 저장 enqueue가 없었다. 12:24:34Z READ ONLY에서 5012는 HP10000/MP9999/(42.764038,49.472141)이었고 과거 마지막 live Full은 HP0/MP9980/(49.2,51)이었다. 이 차이를 DB 초기화나 로그인 회복으로 설명할 근거는 없다. **정상 q는 현재 남아 있는 서버 메모리 상태를 저장하여 이 두 전용 캐릭터의 DB 값을 바꿀 수 있다.** 그 정상 저장은 별도 운영 승인 범위에 명시해야 한다. q 직전 메모리 snapshot을 직접 읽지 않았으므로 과거 live Full과 q 저장값의 정확한 동일성을 가정하지 않는다. TCP 종료 뒤 ghost가 계속 시뮬레이션됐을 가능성도 있다.

q는 로드된 모든 상태를 순회하므로 전용 두 캐릭터만 존재했다고 source만으로 보장할 수 없다. 운영 전 기존 game.log의 세션/사용자 이력과 지정 전용 환경을 확인하고 예상 밖 사용자/로드 상태가 발견되면 중단한다. TCP0은 논리 상태 부재/저장 완료/DB무변경의 증거가 아니다.

## 증거 보존과 READ ONLY 관찰

`ExternalLib/Logger.cpp::CreateSink`는 `logs/system.log`, game/error/perf 로그를 시작 때 truncate한다. **q 후 최종 로그를 후보 시작/현재 빌드 복구 시작 전에 보존하지 않으면 이번 종료 증거가 지워진다.** 새 도구는 실행별 Captures receipt 디렉터리에 pre-q와 post-q/pre-start 로그4종·숫자 suffix의 redo/backpressure 파일을 보존한다. stdout/stderr/helper 출력도 분리한다. post-exit/pre-start 보존 실패면 새 이미지/복구 시작을 중단한다. WAL을 임의 삭제·교체·재생/복구하지 않는다.

최초 read-only preflight에서는 실행 중 redo.1의 exclusive handle 때문에 Copy IOException으로 q/start 전 실패했다. 실패 receipt `r01-switch-20261018-224052-960-20176.txt`를 보존한다. 보완한 경계는 **실행 중 pre-q의 length0 WAL만** 공유/lock 위반 native HResult32/33 IOException 시 크기0/경로/수정시간 metadata를 기록하고 계속하는 것이다. 다른 IOException은 중단한다. 이 경우 byte copy를 했다고 표현하지 않는다. 최종 `r01-switch-20261018-224845-836-20632-evidence`의 redo.1/backpressure.1 metadata도 `ByteCopy=not taken`이라고 명시한다. q로 프로세스가 끝난 뒤와 다음 시작 전에는 실제 byte 사본이 필수이며 metadata fallback을 허용하지 않는다. pre-q 로그 사본 역시 그 시각의 사본이고 q후 확정 사본과 구분한다.

DB 관찰은 기존 `UnityClient/Tools/Inspect-R01Persistence.cjs`만 사용한다. 이 도구는 외부 Game/Login의 .env를 메모리로 읽고 READ ONLY transaction에서 5011·5012의 지정 필드만 SELECT한 뒤 rollback/connection close한다. 이 역할은 실행하지 않았다. 암호/연결설정/토큰은 인자/receipt/스크린샷에 남기지 않는다. 정상 서버 저장과 이 읽기 전용 관찰을 구별한다.

관찰은 pre-q 1회, 정상 exit 후 1회, 로그 보존/검토 뒤 후보 기동 직전 1회로 제한한다. 출력 UTC·행 값·명령 exit를 txt receipt로 보존한다. 종료 후와 기동 직전 사이에 외부 쓰기/두 행 차이/관찰 실패가 있으면 기동을 보류한다. pre-q→post-q 차이는 정상 flush 가능성이며 조작/복구 사유로 삼지 않는다. post-q 값은 **관측한 새 DB 기준값**이고, q 직전 메모리값을 읽지 않았으므로 최신 snapshot 저장 성공의 단독 증거로 쓰지 않는다. 값이 같아도 미저장 ghost 부재를 입증하지 않는다.

post-q 도구 gate는 pinned native target exit0, Cache MQ stopped 정확3/DB worker stopped 정확4, q 직전 대비 error.log byte length 변화0, **game.log JSON의 `event=cache handler, msg=character state update failed` 수가 q 전후 동일**, post-stop 두 행과 pre-start 두 행 동일이다. Handler의 res==0 실패가 INFO game.log에만 기록되어 초기 error.log gate를 통과할 수 있다는 분석/리뷰 P2에 따라 이 한정 guard를 추가했다. 과거 실패를 새 실패로 오인하지 않으며 JSON parse 실패도 중단한다. q 수신/서버 종료·NonZone/Core 정리 구간과 나머지 game.log 진단은 Coordinator가 보존 사본에서 별도 검토하며, 이 자동 도구가 모든 로그의 모든 오류를 포괄 판정한다고 표현하지 않는다. marker/관찰이 다르면 기동하지 않는다. error byte·알려진 UPDATE 실패 수·drain marker·행 안정은 캐릭터별 SQL 최신 저장 성공 증명이 아니다. `GracefulShutdown.md`의 광범위 성공 서술·기존 테스트 성공만으로 현재 C0 종료 결과를 보장하지 않는다.

시작 자체도 DB무변경이라고 일반화할 수 없다. 현 `WALManager.h`는 기존 redo 기록을 읽고 DB lastLSN보다 최신 inventory를 dirty cache로 복구해 정상 flush할 수 있다. `DurableLogger`의 backpressure도 같은 Base::WAL 생성/Replay 경로를 지나며 torn tail 절단·손상 segment 처리가 가능하다. 따라서 **repo의 `^(redo|backpressure)\.[0-9]+$` 파일을 모두 length0일 때만 q/각 기동을 허용**한다. Coordinator의 읽기 관찰과 preflight metadata는 redo.1/backpressure.1 모두0bytes/2026-10-18 16:11:50 KST이며 해당 다른 숫자 segment는 없었다. 이는 그 시각의 관찰이고 q전·후보 시작 전·조건부C0 복구 시작 전에 재확인한다. 하나라도 비어 있지 않으면 보존하고 중단하며 삭제/이름변경/새cwd로 replay를 피하지 않는다. 숫자 suffix의 두 고정 prefix가 guard 대상이며 임의의 다른 파일까지 검사했다고 표현하지 않는다.

이 guard는 위 두 WAL prefix의 nonempty replay 입력을 배제하는 좁은 조건이다. 다른 캐시 dirty 상태나 DB 전체 무변경을 입증하지 않으며 정상 q의 CharacterState 저장·기존 종료 cache flush/빈WAL fsync·파일 관리는 남는다. 예상 밖 재화/거래/flush/WAL 오류가 보이면 범위를 자동 확대하지 않는다. 수동 WAL 삭제/재생/파일 되돌림·DB 직접 UPDATE/restore/재시딩은 제외다. 새 WAL corruption/DB 실패는 readiness 문자열만 보고 통과시키지 않는다.

## 새 Switch-R01LocalServer.ps1의 최소 요건

Tools는 Coordinator/구현 역할이 별도 소유한다. 기존 `Switch-C02LocalServer.ps1`은 PID28924/원본Game529 빌드→현재C0 전환용 고정값이므로 **이 운영에 실행하지 않는다.** 전용 새 도구는 다음을 고정한다.

- 위 정확 PID20024/path/SHA/start ticks/인자 없음과 신규 후보 path/SHA/cwd, 현재C0 복구 image+DLL manifest. 실행 직전/각 q 직전/각 시작 전 hash·PID 재사용·실제 listener 소유권을 다시 확인한다. 필수5DLL hash를 current/candidate 양쪽에 고정하고 숫자 suffix의 redo/backpressure 전체 length0 guard를 q전/각 시작 전에 확인한다. 기존 이미지/인접 DLL을 덮어쓰지 않는다. 기존 manifest 불일치면 다시 빌드/복사하여 맞추지 말고 중단한다.
- `TEST*` nonempty 환경변수를 Process/User/Machine 모든 scope에서 거부한다. 실행 중 Unity는 Play=false/runner·native 입력·Recorder·자동접속 도우미 정지, Assets 동결·Editor 조작자 없음이어야 한다. 전용 테스트 앱을 포함하여9999 Established0, listener는 정확 대상 PID만이어야 한다. 쿼리 실패를0으로 처리하지 않는다. 로그인/DB/Redis/수신기/원본Game은 시작·종료하지 않는다.
- q는 전용 hidden child에서만 target console에 붙고, target+helper 이외 console client가 없음을 확인하여 정확 q down/up 두 입력만 보낸다. 현행 native 입력 구조/CONIN$ ReadWrite share·deadline 재검사·부모 console 미분리·helper native exit handle 확인을 보존한다. helper 완료/exitCode 불명은 중단이고 forcekill/Ctrl+C/taskkill/다른 입력경로로 바꾸지 않는다.
- target/helper handle을 이른 시점에 고정하고 두 native exitCode0을 판정한다. helper 최대15초/입력 deadline12초는 기존 상한, 대상 정상 종료는 q 시도부터 **전체30초**다. **원본 q에서 어떤 실패라도 발생하면 후보/C0복구 모두 기동하지 않는다.** 시간이 넘거나 결과/marker/오류검토가 불명확한 경우도 같다. 기존C02의 stop검증실패 후 자동원본복구 분기는 제거한다. retry q/자동 재실행은 하지 않는다.
- 기존 q 성공 뒤 위 로그/READ ONLY 사본·검토 및 port free를 완료하고 후보를 hidden·인자 없이 저장소 cwd에서1회 기동한다. 새 PID/path/start ticks/SHA/실제 로드 DLL을 기록한다. 준비는30초 안에 정확 PID의9999 Listen+이번 stdout의 `서버 준비 완료`+신규 startup/DB/WAL 실패 없음+Established0을 모두 확인한다. readiness stdout은 writer와 공존 가능한 FileShare.ReadWrite|Delete로 읽는 현행 수정 이력을 유지한다. 단순 TCP 접속 probe는 만들지 않는다.
- 실제 후보 준비 실패 시에만 승인된 조건부 복구를 검토한다. 후보가 살아 있으면 그 정확 PID/start/image로 동일 정상 q를1회 수행하고30초 내 nativeexit0·동일stop evidence를 확정한다. 그 q에서 하나라도 실패하면 복구도 금지한다. 후보가 startup 중 이미 종료됐으면 해당 Process.HasExited와 시작 직후 고정한 native handle의 exitCode(259가 아님)·port free를 확인하고 실패후 최종 로그/READ ONLY를 보존한다. native 조회 실패/미종료는 복구를 금지한다. 후보 startup 종료 분기를 정상 q 성공으로 기록하지 않는다. 두 경우 모두 후보 종료후/기동직전 DB두 행은 원본 종료후 snapshot과도 같아야 한다. currentC0 manifest·emptyWAL·사본보존 조건까지 통과할 때 C0를1회 시작하고 같은30초 readiness를 확인한다. DB 값은 이전 snapshot으로 되돌리지 않는다.
- 관찰/manifest/PID/예상 경로 차이나 q 후 저장 오류는 **후보 startup 실패와 다른 차단**이다. 실행자가 임의로 보완/강제복구하지 않는다. root가 새 정확 범위를 검토한다. 복구 실패도 반복/강제종료 없이 receipt와 실제 상태를 남긴다. 승인/플랫폼 심사 거절을 다른 실행 경로로 우회하지 않는다.

최종 직접 읽기 확인한 `UnityClient/Tools/Switch-R01LocalServer.ps1` SHA는 **`F965C508C5E9B4802DDC59704CF423869944F092C9B8D95D2070CF41265C21BA`**다. 사전가드 실패/Start-Process 후 반환·handle고정 불명으로 `$newProcess=null`이면 복구도 하지 않는 중단 경로를 확인했다. 5DA8B9E0…의 Parser오류0/native2정의 컴파일(호출0) 뒤 최종 변경은 위 Stop 전용 game.log 실패 수 guard다. Coordinator는 F965의 Parser오류0 및 새 함수만 현재 game.log에 읽기 실행해 count0임을 확인했다고 전달했다. **도구 최종 독립 리뷰 결과는 인계 대기**이며 이 분석을 그 리뷰로 대신하지 않는다.

**PreflightOnly 전체를 실제 실행한 hash는 5DA8B9E0E4FEE6F34637236F4E58E4FC604AAF16D028CE1FFBD0DED2464EC2F5**다. 해당 read-only 실행 exit0이라는 Coordinator 인계와 `r01-switch-20261018-224845-836-20632.txt`를 대조했다. receipt SHA는 **`6E972AAE2FB68DE2C3705A117E72E46490A1A25E6868FAC6E5FA53C5996DDCB0`**이고 q입력/서버기동/DB쓰기0을 기록한다. 최종 F965는 preflight 실행 분기를 변경하지 않은 Stop 전용 delta이므로 이 preflight 근거와 최종 delta 정적 검증을 결합한다. **F965 전체로 preflight를 다시 실행했다고 주장하지 않는다.** 같은 evidence 폴더의 READ ONLY UTC13:48:48.583Z는5011 HP9975/MP10000/(39.000019,39.000008),5012 HP10000/MP9999/(42.764038,49.472141)다(snapshot SHA `E637B9D50E090342F0B911D748ED8C15EDA3BF3549147EC5C8EA4B991CBB9498`). 이는 pre-q 기준 관찰이며 캐릭터 저장 성공·정상 q/drain·후보 startup/readiness·조건부 복구 통과가 아니다. 이전224052 copy실패와 중간224612 preflight는 보존된 이력이며 최종 실행으로 합치지 않는다.

Coordinator의22:37대 Unity 읽기 관찰은 C02 scene/Play·changing·compiling·dirty=false다. 도구가 Unity를 직접 검사한다고 표현하지 않으며 실제 운영 실행 직전에 Coordinator가 다시 확인한다. 아직 R01-DEPLOY 운영은 허용/수행되지 않았다.

## 전환 이후 B05/V04와 남는 한계

R03의 source 리뷰/타깃10+Net19 PASS는 정상 transport 종료의 두 late 완료 경계에 대한 격리 근거다. 기존 C0의 이번 q 저장·새 서버의 실 SQL·HP 영속 성공이 아니다. 전환 readiness까지 확인한 뒤 Coordinator가 별도 tester의 B05 helper를 동결·리뷰하여 한 명에게 Editor 조작권을 준다. 테스트 중 Assets/Packages/ProjectSettings 쓰기를 멈춘다.

기수용 V04 재개를 중복 승인 요구로 바꾸지 않는다. B05는 새 서버 fresh Full에서 실 HP/MP/좌표부터 읽고, 살아 있는 전용 캐릭터가 있으면 안전한 외곽에서 native 이동/시야/HUD와 최소 행동→정상 Disconnect→**다른 요청/다른 접속 없이** bounded 구간의 해당 session 저장요청·READ ONLY SQL값→재접속 첫 Full을 연결한다. HP0이면 새 연결도HP0인지 확인하며 수동 회복/더미 HP/DB 수정은 하지 않는다. 두 계정 모두HP0이면 이동/전투 검증은 차단으로 구분하고 새 정책을 만들지 않는다. 미래 B05 검증의 구체 비용/종료조건은 작업 카드에서 정한다.

실패 원문을 보존한다. SQL UPDATE→다음 LOAD 순서, 직접 Core 재인증 세대, 일반 Zone lock/이동 동시성, 종료 중 외부 callback 수명은 이번 제품 후보의 잔존 한계다. 원본C0 복구는 알려진 packet 참조 지연이 있는 빌드로 돌아가는 가용성 복구이며 R01 해결을 뜻하지 않는다. rollback 뒤 B05는 자동 진행하지 않는다.
