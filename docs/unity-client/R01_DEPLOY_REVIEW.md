# R01-DEPLOY-04/r1 운영 후보 독립 리뷰

2026-10-18. **최종 도구 F965C508…의 정적 판정: 코드상 차단 문제 없음.** 아래 초기 P2는 후속 수정으로 해소됐다. 이는 구체 운영 요청 범위의 정적 판정이며 사람의 q/시작 승인·실제 전환·정상 저장 성공을 뜻하지 않는다. 리뷰어는 이 문서와 배정된 R03 결과 리뷰만 갱신했으며 도구 실행·native 호출·프로세스·DB·Unity·제품/테스트 변경은 하지 않았다.

## 기준과 선행 결과

초기 도구 `UnityClient/Tools/Switch-R01LocalServer.ps1` SHA256 `575AAFBCC6E0152ACCE4A60A9C6308A6351AC05B9691A03B206494038BC6136B`를 읽었다. 운영 분석 초안 SHA `452AC3D9DDD0C6419974FF3051D5B3F31E09617A2B1E81F462FAB30CB4F90CA9`, 요청 초안 SHA `78B58D83E8BC7FBCA14FEE426E614A122B27B1ECE0FB2176D8FF436C78ABA97C` 기준이다. 아직 실제 q/기동 운영 승인은 없다.

R03의 고정 소스4/UnitTests5504D57C…·build-only MainServer01C6261A…를 실제 hash와 대조하고 담당자의 타깃10/Net19 각1회 PASS를 수용했다(R01_CLOSE_IMPLEMENTATION_REVIEW 후속 절). 후보 MainServer는 아직 실행되지 않았으며 R03 통과는 이번 전환/실SQL·HP영속 성공의 증거가 아니다.

## PreflightOnly 정적 확인

QuitHelperPid>0 분기는 PreflightOnly와 함께 지정하면 console 입력 전에 throw한다. 기본 main 분기는 TEST flags·고정 image/DLL·PID/path/시작ticks·9999 listener/접속을 읽고 증거 폴더/receipt를 만든 뒤 로그·redo 사본과 전용2행 snapshot을 저장한다. `if ($PreflightOnly) ... exit0`은 Stop-Normally와 Start-Fixed 호출보다 앞에 있다. main의 Add-Type은 exit조회 native 정의만 하며 이 분기에서 native method를 호출하지 않는다.

고정 hash의 Inspect-R01Persistence.cjs도 읽었다. Game/Login .env를 메모리에서만 사용하고 READ ONLY transaction의 지정5011/5012 SELECT→rollback→close 경로다. 실패 출력은 연결 세부정보를 생략한다. preflight의 증거 파일 쓰기와 직접 DB 갱신/서비스 변경은 구분한다. 이 경로를 읽기·증거 사본 전용으로 실행할 정적 차단 근거는 없다. 리뷰어는 실행하지 않았다.

## 정상 전환의 초기 차단 지적

1. **[P2] 대상 서버 native exit0 검증 누락** — Stop-Normally의 target WaitForExit 뒤에는 종료code 검사가 없고 정상 종료로 기록한다. q 뒤 crash/nonzero 종료도 후보 시작으로 이어질 수 있어 요청서의 정상 exit0 조건과 다르다. helper에만 있는 handle pin/native exit 검증을 정확 대상 서버에도 적용하고 종료 실패/불명은 startup 실패와 구분해야 한다.
2. **[P2] post-q 저장 관찰·오류 gate가 없어 자동 기동됨** — 현재 snapshot은 pre-q와 Start-Fixed의 prestart뿐이다. 종료 직후 별도 관찰→종료 새 로그/Cache·DB drain 오류 확인→기동 직전 동일 두 행 비교는 없으며, 로그를 복사한 뒤 즉시 새 시작으로 간다. SQL 실패가 있는 q 종료도 진행할 수 있다. 요청서의 해당 검토를 실제 실행 경계에 넣거나 정확한 단계 분리로 운영자가 확인하기 전 기동하지 않도록 해야 한다. drain/exit만으로 특정 캐릭터 저장 성공을 주장하면 안 된다.
3. **[P2] WAL 증거/guard가 redo에만 한정됨** — Assert-EmptyWal/Preserve-LogsAndWal은 redo.*만 본다. MainServer의 Core::DurableLogger는 backpressure.*도 사용하며 Base::WAL startup은 replay·손상 격리·활성 꼬리 절단을 할 수 있다. 해당 파일은 실패한 캐릭터 저장의 fallback 증거일 수 있다. 시작 전 backpressure 계열도 보존/상태 확인하고 승인하지 않은 nonempty replay/파일변형은 중단하는 정확한 정책이 필요하다. 파일을 삭제하거나 DB를 복원하는 해법은 아니다.
4. **[P2] 실제 로드 DLL/새 startup 오류 조건이 도구에 없음** — 현재 Assert-Image는 인접5DLL hash만 검사하고 Wait-Ready는 stdout 준비 문구+listener/접속만 확인한다. 요청서의 실제 loaded path/hash와 새 startup/DB/WAL 오류 부재는 아직 검사하지 않는다. root의 별도 확인으로 처리할 부분과 도구의 자동 성공 gate를 명시적으로 맞춰야 한다. 미확인 조건을 포함한 전체 성공으로 기록하지 않는다.

고정 PID/path/hash/startticks, TEST flags 거부, 전용 console target+helper 확인, 12초 입력 deadline/15초 helper 제한, helper handle pin/native exit, 전체30초 target 종료, readiness 공유 파일 읽기, helper 불명 시 후속 process action 중단은 기존 C02 보완 경계를 재사용한다. source에서 forcekill/Ctrl+C/다른 service 시작·종료·직접 SQL write는 찾지 못했다. 보완 후 이 경계가 유지되는지 해당 delta를 재검토한다.

## 도구 재검토 — ADF2D554 수정본

SHA256 `ADF2D554756EA6731CADADD9C8DF685FE8653DE4F40B7BFCF556B92F7510F420`를 직접 확인했다. 초기 지적의 target handle pin/native exit0, post-stop snapshot/Cache MQ3·DB4 종료 marker/error log byte 증가 차단, prestart 두 행 비교, redo·backpressure 숫자 segment 보존/zero guard, 실제 loaded3DLL path/disk hash와 startup error log gate가 추가됐다. 원본 stop의 모든 실패에서 후보/복구 기동을 차단하도록 단순화하여 30초 timeout 뒤 늦은 종료를 복구로 바꾸는 이전 흐름도 제거됐다. preflight는 여전히 Stop/Start 전에 exit한다.

정확 active empty WAL이 전용 handle에 잠겨 pre-q 사본을 못 얻는 경우에는 현재 길이0을 다시 확인하고 metadata만 저장한다. post-exit/모든 prestart에서는 실제 사본 실패를 허용하지 않는다. metadata를 byte copy로 보고하지 않는다는 점은 타당하다. 다만 현재 catch가 모든 IOException을 sharing failure로 간주하므로 정확한 sharing/lock 위반만 허용하는 정책과 분류가 맞는지 보완 권고했다.

함수 success pipeline을 검토했다. Save-PersistenceSnapshot은 경로만 반환하고 Record의 Write-Host는 success stream을 오염시키지 않는다. Start-Fixed의 사본/검사 결과도 불필요한 객체를 출력하지 않아 반환 hashtable을 array로 만드는 흐름은 없다.

**남는 차단:** Start-Fixed의 실제 Start-Process 이전 manifest/관찰 guard가 throw하면 newProcess=null인데 outer catch가 C0 복구를 시도한다. 이는 prestart 경계 실패와 startup 실패를 구분하는 요청 요건에 맞지 않는다. 분석 담당과 리뷰어가 같은 흐름을 확인했으며 null 경로는 추가 기동 없이 중단해야 한다. 또한 candidate already-exited 복구는 현재 HasExited만 확인하므로 시작 직후 handle pin과 native 종료 조회/불명 차단을 보완하도록 전달했다. 비정상 후보 startup 자체의 nonzero 종료는 기록된 실패로 취급할 수 있으나 종료 자체가 확인되지 않으면 복구하지 않는다.

## 도구 재검토 — 5DA8B9E0 수정본과 남는 저장 gate

최종 전달본 SHA256 `5DA8B9E0E4FEE6F34637236F4E58E4FC604AAF16D028CE1FFBD0DED2464EC2F5`를 직접 확인했다. newProcess=null이면 복구도 중단하고, Start-Fixed는 새 PID를 기록한 뒤 handle을 pin하여 반환한다. 이미 종료된 후보는 pinned handle의 native code를 조회하고 259/조회 불명은 복구를 금지한다. pre-q metadata 예외도 empty segment + sharing/lock HRESULT32/33에만 한정됐다. 앞 절의 해당 추가 지적은 해소됐다.

Coordinator가 이 hash로 read-only PreflightOnly를 실행해 exit0이라고 인계했고 `r01-switch-20261018-224845-836-20632.txt`(SHA `6E972AAE2FB68DE2C3705A117E72E46490A1A25E6868FAC6E5FA53C5996DDCB0`)와 동명 evidence를 읽었다. 본문22:48:48의 사본/READ ONLY2행/실제loaded3 확인 뒤 PREFLIGHT_ONLY·q/start/DB쓰기0 기록과 정합한다. 리뷰어 실행이 아니다. 선행224052의 WAL Copy IOException은 q/start 전 실패이며, active empty WAL metadata 보완 후 통과한 것이지 실제 전환 retry가 아니다.

**[P2] 명시적인 캐릭터 UPDATE 실패 INFO 로그가 post-q gate에서 빠져 있다.** `CacheLib/Handler.cpp:166`은 ExecuteUpdate(4) 결과0을 errorLogger가 아니라 gameLogger->LogInfo의 `event=cache handler, msg=character state update failed`로 남긴다. 따라서 error.log byte 증가0·worker stopped·두 행 안정만으로 이 실패를 배제하지 못한다. 요청서는 game.log의 새 저장 오류도 Coordinator가 검토한다고 하지만 도구는 q 후 바로 startup을 이어 해당 수동 확인 경계가 없다. q전 count/offset과 비교해 새 이 메시지를 차단하거나 실제 검토 전 startup을 멈추는 최소 경계가 필요하다. 과거 실패 전체를 새 종료 오류로 오인해서는 안 된다. 이 지적을 제품 변경 없이 Coordinator에게 전달했으며 현재 정상 전환 판정은 아직 차단이다.

## 최종 도구 판정 — F965C508

최종 SHA256 `F965C508C5E9B4802DDC59704CF423869944F092C9B8D95D2070CF41265C21BA`를 직접 확인했다. Get-CharacterUpdateFailureCount는 실제 game JSON의 `event=cache handler`/`msg=character state update failed` 개수만 반환한다. q전 기준을 잡고 post-q Assert-StopEvidence에서 비교하므로 새 명시 UPDATE 실패는 모든 후속 startup을 막는다. 원본 stop catch는 복구도 하지 않고 종료하며, 후보 stop 실패 역시 rollback으로 이어지지 않는다. 이로써 마지막 저장 gate P2도 해소됐다. malformed/읽기 실패도 throw로 중단하며 과거 실패를 새 실패로 단정하지 않는다.

preflight는 실행된 5DA8B9E0 버전과 같은 경로이고 F965 변경은 Stop-Normally의 새 저장 실패 gate에만 해당한다. **실제 PreflightOnly PASS receipt는 5DA 기준**이며 F965 전체 스크립트를 실행했다고 기록하지 않는다. Coordinator가 Parser0과 새 읽기 함수만 선택 로드해 현재 count0 확인을 보고했다. 리뷰어는 이 함수도 실행하지 않았다. 실제 q/native console 입력/서버 기동/drain/조건부 rollback은 여전히 미실행이다.

최종 source에서 PID20024/startticks639279045410504905·C0 path/hash와 후보01 path/hash, 인접5DLL/실로드3 경계, TEST flags/9999 쿼리 오류 차단, 전용 console·12/15/30초 제한·native code, WAL zero/사본 및 post-q 두 행 안정, startup/복구 fail-closed가 유지됐다. 함수 pipeline도 경로·count·hashtable의 의도한 값만 반환한다. 새 제품/DB 정책이나 다른 프로세스 조작으로 범위가 확대되지 않았다.

## 최종 분석·요청서 동결 대조

운영 분석 SHA256 `6FF95C50EAED2C32481ECA35DA7AB6F2D2784F50D83E7F220224CA25C00035A7`, 요청서 SHA256 `44E1E9BFC51B408CE45B8CC456D5BDFBF3FEF2A09117838086CE7D3654DAE638`를 직접 대조했다. 최종 F965 tool·새 UPDATE 실패 gate·5DA 실제 preflight와 F965 정적 delta의 구분·명확한 q/후보/복구 중단 조건이 반영돼 도구와 요청 범위가 맞는다. 실제 기동 전 guard 실패/null이면 복구도 하지 않으며, 정상 q의 캐릭터 저장 가능성과 nonempty 두 WAL prefix replay 제외가 명시됐다.

**최종 판정은 도구 및 정확 운영 요청 범위에 코드상 차단 문제 없음**이다. Coordinator가 이 구체 범위의 사람 승인을 요청할 수 있는 정적 준비 상태이며, 리뷰어의 판정 자체가 운영 허가는 아니다. 실행 직전 재확인·승인·플랫폼 통제는 유지하고 실제 운영 결과는 별도로 기록해야 한다. 리뷰어는 추가 파일/실행 변경 없이 역할을 반납한다.

## 운영안의 남는 판단

정상 q는 StateManager::CleanUp→Cache MQ→DBWorker의 현재 상태 저장을 수행할 수 있으므로 전용 행 값이 달라질 수 있음을 승인안에 명시해야 한다. TCP0은 논리 세션0이 아니다. StateManager::CleanUp에는 정상 Disconnect의 Enqueue Disconnect 성공 로그가 없고 개별 SQL 성공 receipt도 없으므로, 로그/drain/두 행 관찰의 증거 한계를 유지한다. 과거 live Full을 q 직전 메모리 snapshot으로 가정하거나 HP를 임의 복원하지 않는다.

후보 startup 실패의 조건부 C0 복구와, q후 저장오류·manifest/관찰 차이에 대한 중단은 다른 경계다. 대상/helper 종료 불명은 새/복구 기동 없이 중단하고, 복구는 port free·원본C0 manifest 보존·최종 로그/관찰을 충족한 정확 경로만 허용한다. 기존529 빌드/예전 C02 전환 스크립트 재사용은 범위 밖이다. 정적 차단 해소는 사람의 정확한 별도 운영 승인과 실행 직전 환경 재확인까지 대신하지 않는다.
