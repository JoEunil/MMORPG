# R01-DEPLOY-04/r1 — 로컬 게임 서버 정상 전환 승인 요청

2026-10-18. Coordinator 확인: **정확한 운영 요청의 준비·독립 리뷰를 완료했으며 사람 승인과 실제 운영 실행은 아직 없다.** R01-CLOSE-03의 제품 구현·격리 빌드 승인과 이 운영 승인은 구별한다. 아래 기술 범위는 독립 검토한 요청본 SHA44E1E9BF…와 동일하며 이 후속 갱신은 준비 상태만 확정한다. R01_DEPLOY_REVIEW.md(SHA0CA90A8F1D706FD62F3886D1FDE84865C6DA582990FDA8B21262FDEA8D5FCCB2)의 최종 F965 정적 판정은 차단 문제 없음이다.

## 전환 대상

- 종료 대상은 PID **20024**, StartTime UTC ticks **639279045410504905**, 인자 없음, `C:/Users/joeun/Desktop/workspace/Game-UnityClient/x64/Release/MainServer.exe`, SHA256 **C0F689BD0E58D3A20282143D5DD5C0C15514F42F192A6FF70568C2DDF312E825**만이다. 이는 배정 직전 Coordinator의 읽기 관찰값이며 실행 직전에 다시 대조한다.
- 후보는 `C:/Users/joeun/Desktop/workspace/Game-UnityClient/UnityClient/Builds/R01-Candidate/bin/MainServer.exe`, SHA256 **01C6261AB5C3448E2BA5F336CE70FB20E4DBA4B9ED47AD70002F22C88594D108**다. 빌드 exit0·미실행. cwd는 저장소 루트, 인자 없음, 기존9999를 사용한다.
- 복구는 현재 **C0** 이미지와 현재 인접 DLL을 보존하여 사용한다. 원본 `Game`의529D16A… 빌드는 복구 대상이 아니다. 실행 중 이미지/DLL을 덮어쓰지 않는다.
- 후보 인접 DLL6개의 전체 해시는 `R01_OPERATIONS_ANALYSIS.md` 및 `UnityClient/Captures/r01-close-bin-manifest.txt` SHA **A5326D020D7343B0A223DADD81460712FDB4265C11DF7359E185F3F0BBEF8D54**에 고정했다. 실제 로드 DLL 목록과 동일하다고 가정하지 않는다.

## 준비 근거 — 실행 직전 상태 재확인 필수

| 조건 | 현재 기록 |
| --- | --- |
| R03 제품/fixture 독립 리뷰 | R01_CLOSE_IMPLEMENTATION_REVIEW.md SHA F655EB90F03034CDA9F17620AFAC17B169C13692422FEEAB95A178290F6B2A95 고정4파일 차단없음 |
| UnitTests 고정5504D57C… 타깃10·Net19 별도 실행 | r01-close-candidate-regression.txt SHA BF8752238343E6D1948B616366D0E0B7289688FEFFC4FCB2FB73833FF696241F 실제10/10·19/19/각1회exit0·재시도0 |
| 새 전용 운영 도구 | UnityClient/Tools/Switch-R01LocalServer.ps1 SHA **F965C508C5E9B4802DDC59704CF423869944F092C9B8D95D2070CF41265C21BA**. 최종Parser0/신규read-only UPDATE실패count0. 이전5DA의native2정의compile(호출0). 최종 독립 리뷰 차단 문제 없음 |
| 현재C0 인접 DLL·실제 PID loaded DLL manifest | 인접 필수5 hash동일; 최종 evidence/loaded-20024.txt SHA079C3D2EFBD5D617D6227F727CEF427054BEE84BC6AAAC7D180F4A4E4826BC68에 실제 event/hiredis/mysql3종의 x64/Release경로·디스크hash 확인. core/extra는 미로드 |
| repo redo/backpressure 숫자segment replay 방지 | 현재redo.1/backpressure.1 모두0bytes; q전/각기동전 `^(redo\|backpressure)\.[0-9]+$` 전부length0 guard |
| read-only preflight | **5DA8B9E0…에서** r01-switch-20261018-224845-836-20632.txt SHA6E972AAE2FB68DE2C3705A117E72E46490A1A25E6868FAC6E5FA53C5996DDCB0 PASS exit0/q입력·기동·DB쓰기0. F965는preflight불변·Stop전용delta 정적검증이며 전체재실행claim없음 |
| 최종 로그·WAL·DB gate | active pre-q emptyWAL 공유/lock위반32/33만metadata예외; postexit/prestart실사본필수, nativeexit0/Cache3·DB4/errorbyte delta0/알려진UPDATE실패JSON수증가0/postq→prestart행stable. 실제q/drain/startup은미실행 |
| 현재 Unity/9999/관련 서비스 무변경 확인 | preflight는 정확PID9999/무접속 확인. Unity는22:37대읽기관찰 C02/Play·changing·compiling·dirty=false; 실행 직전 Coordinator 재확인 필수 |

최종 독립 리뷰가 끝나기 전에는 이 초안을 실행 요청으로 보내거나 승인된 것으로 표현하지 않는다. 실행 직전 재확인은 승인 후 실제 실행 전에 수행할 필수 조건이며 프로젝트 범위의 중복 승인 요청을 뜻하지 않는다. 도구/이미지/대상/범위가 바뀌면 Coordinator가 개정을 명시한다. snapshot·preflight는 SQL 저장/정상 q/drain/후보 readiness/복구 실행 성공의 증거가 아니다.

## 승인받을 정확한 동작과 DB 영향

1. 위 조건·지정 사람 승인·플랫폼 허용을 확인한 뒤 현재 PID/path/startticks/image 및 현재/후보 필수5DLL hash, 실제로드3종의경로·디스크hash, Process/User/Machine의 TEST* unset, Unity Play/runner/자동접속 정지와 Assets 동결,9999 Established0/정확PID Listen을 확인한다. **repo의 숫자suffix redo/backpressure segment 전부length0인지 q전/각기동전 검사**하여 비어 있지 않으면 보존하고 중단한다. pre-q 로그4종·WAL·전용 두 행 READ ONLY를 보존한다. 실행중 emptyWAL이 exclusive handle로 잠겼으면 pre-q만metadata를 남기며 사본이라고 주장하지 않는다. 다른 서비스는 유지한다.
2. 고정한 현재 PID에 전용 hidden native helper로 **q 정상 종료1회**를 보낸다. helper/console 독점·deadline·native handle exit를 확인하고 전체30초 내 target/helper 모두 native exit0·port free를 확인한다. **원본 q의 어떤 실패/native nonzero/30초 초과·불확정/오류·marker 미달이라도 후보/C0복구 기동을 모두 금지한다.** 기존 C02의 stop 실패 후 자동복구를 사용하지 않는다. forcekill/중복기동/다른 경로 재시도 없이 중단한다.
3. **정상 q가 메모리에 남은 전용 테스트 캐릭터 상태를 DB에 저장하는 것을 이 운영 범위에 포함한다.** TCP0이어도 session15 같은 논리 상태가 남았을 수 있다. 5012 과거 live HP0/MP9980/(49.2,51)과 이전 DB HP10000/MP9999/(42.764038,49.472141)이 달랐으며 q로 DB가 바뀔 수 있다. q 직전 메모리값은 미관찰이므로 과거 Full을 정확 저장 기대값으로 고정하지 않는다. HP0 저장도 정상 결과로 보존하며 수동 회복하지 않는다.
4. 종료 후 Cache MQ stopped 정확3/DB worker stopped 정확4·q 전 대비 error.log byte 변화0·game.log JSON의 **event=cache handler/msg=character state update failed 수 불변**·정상 exit 및 전용2행 READ ONLY를 확인한다. 이 INFO 실패가 새로 생겨도 후보/복구 기동은 차단된다. **다음 시작 전에 최종 system/game/error/perf 로그와 redo/backpressure 숫자 segment 실사본을 별도 receipt에 보존**한다. post-exit/pre-start에서는 locked metadata fallback을 허용하지 않는다. 서버 시작은 로그를 truncate하므로 사본 실패면 중단한다. 기동 직전 두 행을 다시 읽어 종료 후 snapshot과 다르면 중단한다. StateManager.CleanUp의 `Enqueue Disconnect` 성공로그/SQL성공 receipt는 없으며, 알려진 실패 부재/error byte/stop marker/READ ONLY값 안정만으로 특정 행 최신 저장 성공을 단정하지 않는다. 그 밖의 진단은 보존 사본에서 별도 검토하며 도구가 모든 로그 오류를 포괄 판정한다고 표현하지 않는다.
5. 다시 양쪽 WAL prefix 숫자 segment 전부 length0·실사본 보존을 확인하고 고정 후보를 저장소 cwd·인자 없이 hidden으로1회 시작한다.30초 내 정확 새 PID의9999 Listen+이번 stdout의 `서버 준비 완료`+error.log empty+Established0·실제 로드3종 경로/디스크 hash 일치를 확인한다. empty error.log가 모든 오류 부재/SQL 성공 증명은 아니다. 이번 운영은 **비어 있지 않은 redo와 backpressure의 replay/절단/복구를 허용하지 않는다.** guard 제거/WAL 삭제/새 cwd로 우회하지 않는다. 기존 정상 q의 캐시 flush·빈 WAL fsync/파일 관리 외 **직접 SQL UPDATE/restore/재시딩·수동 WAL 삭제/재생은 제외**다. length0 guard는 DB 전체 무변경 증명이 아니다.
6. **실제 후보** 준비 실패 때만 조건부 복구한다. 살아 있는 후보는 동일 정상 q/native exit0·동일 stop gate를 확정하고 하나라도 실패하면 복구도 금지한다. startup 중 이미 종료된 후보는 시작 직후 고정 handle의 native exitCode를 조회하고259/조회실패면 중단한다. 해당 Process 종료/port free·최종 로그/READ ONLY를 확인하며 정상 q 성공이라고 기록하지 않는다. 후보 종료 후/복구 직전 두 행은 원본 q 종료 후 baseline과도 같아야 한다. empty WAL·실사본·고정 C0 이미지/DLL이 모두 일치할 때만 C0를1회 시작하고30초 readiness를 확인한다. 실제 후보 시작 전 manifest/관찰 guard 실패나 `$newProcess=null`은 복구 사유가 아니다. **DB 값을 이전 snapshot으로 되돌리지 않는다.** rollback 실패 시 추가 시도 없이 실제 상태를 보고한다.

정상 캐릭터 저장·기존 종료 cache flush/빈WAL 파일 관리 외 기능/정책/DB schema/계정/수치 수정, 비어 있지 않은 WAL replay, 로그인/MySQL/Redis/Slack 수신기 시작·종료, 원본Game 변경, 서비스 강제종료, push/병합/공개는 제외한다. 현재 도구를 복사해 준비하는 것은 실행 승인이 아니다. 기존 `Switch-C02LocalServer.ps1`은 오래된 PID/원본529 빌드가 고정되어 있어 이 요청으로 실행하지 않는다.

## 이후 검증과 요청 문구

전환은 readiness까지다. 이후 V04/B05는 기수용 재개 범위에서 별도 tester helper를 동결·리뷰하고 새 서버 fresh Full을 먼저 확인한다. 실제 native 이동/시야/HUD·정상퇴장→추가요청 없는 저장관찰→재접속 첫Full 비교를 한 배치로 검증한다. 실제 HP0이면 재접속도HP0인지 검사하고 더미HP/수동회복은 하지 않는다. 실 SQL/HP 영속 성공을 격리 회귀나 전환 readiness로 대체하지 않는다. rollback된 C0에서 B05를 자동 계속하지 않는다.

모든 빈칸/미확정 조건을 채우고 Coordinator가 검토한 뒤 지정 변경 스레드에 요청할 명령 초안:

`!coord 승인 R01-DEPLOY-04/r1 정상전환·조건부복구 재개`

이 명령은 아직 발신/수신/수용되지 않았다. 사람 승인은 프로젝트 운영 범위의 승인이고 실행 플랫폼 managed sandbox/auto-review는 별도 통제다. 플랫폼 거절이 있으면 실제 사유를 같은 변경 스레드에 기록하며 다른 실행 경로로 우회하지 않는다.

Coordinator 발신 확인: Unity Coordinator bot1792331719.054649(부모1792326328.900739), https://aiagent-vqu1541.slack.com/archives/C0C9VFZMV43/p1792331719054649 . 아직지정사람의이운영승인은없고실제q/기동/복구0. 위미발신문구는분석완료시점이력이다.
