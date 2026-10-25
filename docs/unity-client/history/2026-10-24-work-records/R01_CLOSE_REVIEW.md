# R01-CLOSE-03/r1 후보 독립 리뷰

2026-10-18. **정상 transport 종료 후 두 재삽입을 막기 위한 후보 구현·격리 검증 요청 범위에는 코드상 차단 문제 없음.** 아직 사람 승인·제품 적용·빌드·회귀·배포 통과 판정은 아니다. 리뷰어는 문서/patch와 현재 제품 소스를 읽었으며 이 리뷰 문서 및 배정된 이전 실행결과 리뷰만 갱신했다. 제품·테스트·빌드·Unity·DB·서비스는 변경하거나 실행하지 않았다.

## 고정 검토 대상

| 대상 | SHA256 |
| --- | --- |
| R01_CLOSE_ANALYSIS.md | `8085429BE78369EC837DC188FDC6AC13BFC08ADC378E65B62C41CEDBE8E78CE3` |
| R01_CLOSE_03_candidate.patch | `E3D832EA214D27FC0985816E8E91E527E38BDABD47FB637BFB1ED687ADCA7D7B` |
| R01_CLOSE_REQUEST.md | `1B68AF10E0BB944AA989E680C58ADE5EB4CAF0D1894E5A62C75A1D5C067FF220` |

제품 기준 StateManager.h `835DF6B2927908BD154BB1057BEED64EC5D597A291C87D65E23F47E5E8ED3777`, LobbyZone.h `A0FDA201C94FB15328D129B27218668DB8149E2F079654B414E94B0A7E80B1DD`, NonZoneHandler.cpp `8FADE9AE7DBE53535CDAA4A9CD8033B087B4AC3D957AD7CB53A9F10AE42B8BEA`를 직접 hash 확인했다. 분석 담당의 read-only git apply --check 통과는 전달 근거이며 리뷰어가 patch를 적용하지 않았다.

별도 tester의 고정 exe F60BFB7D… 단일 실행은 기존3 PASS/late2 FAIL이다. AUTH Core zone0/user6982 재생성, ENTER lobby=true는 기대값 변경 없는 실제 격리 실패이며 R01_LATE_COMPLETION_REVIEW에 소스/exe 일치와 함께 기록했다. 이 실패가 새 제품 수정 승인이나 실제 TCP/SQL 검증을 대신하지 않는다.

## AUTH 경계·정책 보존

BeginAuth는 AUTH PacketView가 살아 있는 CheckSession 안에서 외부 auth 호출 전에 기존 shard lock 아래 try_emplace만 한다. pending의 UID를 요청 순서로 미리 확정하거나 기존 session을 초기화하지 않는다. Net Context는 workingCnt가 0이 되어야 transport 종료를 통지하므로 정상 TCP close의 Core Disconnect는 이 요청의 pending 등록보다 먼저 끝날 수 없다. 다른 미처리 AUTH PacketView도 같은 종료 장벽의 참조다.

CompleteAuth와 Disconnect가 같은 shard mutex를 사용한다. callback이 먼저면 entry를 완료한 뒤 Disconnect가 제거하고, Disconnect가 먼저면 CompleteAuth는 부재에서 false로 끝나 재삽입하지 않는다. 상태 완료 락을 놓고 writer/IOCP를 호출하므로 외부 auth 대기나 송신을 shard lock 안으로 옮기지 않는다. 상태 완료 직후 close가 와서 송신이 늦어져도 새 상태 삽입은 없다.

CompleteAuth의 bool은 인증 성공값이 아니라 **현재 entry가 존재해 callback 응답을 허용하는지**다. 최초 실패도 authCompleted=true/authenticated=false로 완료하고 실패 응답을 보낸다. 이미 완료된 entry에는 상태를 덮어쓰지 않되 true를 반환하여 기존 duplicate callback 응답을 보존한다. 따라서 첫 callback의 UID/인증 상태 고정, 실패 후 성공 응답과 Core 인증 상태가 다를 수 있는 기존 특성까지 임의 변경하지 않는다. 기존 AddSession의 Core 호출점은 NonZoneHandler 한 곳이라 교체 사용처 누락을 찾지 못했다.

authCompleted 한 bool은 pending(false 인증)과 완료된 실패(false 인증)를 구별하는 현재 필요가 있다. UID0을 완료 여부의 sentinel로 쓰지 않는다. SessionData 내부 필드이며 wire/SQL 직렬화 변경은 없다. pending의 HealthCheck는 미인증 상태이고 기존 NetPacketFilter가 AUTH/PONG 외 요청을 거부하는 경계가 유지된다. pending EXIST와 관련 진단 로그가 이전보다 일찍 나타나는 변화는 분석 문서가 명시한다.

## ENTER 원자성·락·rollback

기존 LobbyZone::ImmigrateChar 안에서 StateManager shard→lobby mutex 순으로 잠그고, 같은 shard lock을 잡은 채 session 존재/authenticated/zone0 검사와 삽입을 진행한다. Disconnect/CleanUp과 순서가 같고, 기존 lobby→SetZoneID의 shard 재획득을 없앤다. zone0 사전조건을 lock 아래 통과하므로 제거한 SetZoneID(0)의 정상 최종값은 동일하다. 송신/DB 호출을 lock 안에 추가하지 않는다.

상태 삭제가 먼저면 삽입이 거부되고, 삽입이 먼저면 Disconnect가 이어서 지운다. 새 `friend class LobbyZone`은 테스트 seam이 아닌 두 기존 클래스의 제품 내부 접근 확대이며 요청서에 정확히 포함됐다. 새 범용 Manager/생존 검사 API·queue·tombstone은 추가하지 않는다.

MQ 응답과 ZoneChange의 lobby rollback 두 호출은 기존 ImmigrateChar를 유지한다. 실제 ZoneState::ImmigrateChar의 capacity/duplicate/ID 소진 실패는 SetZoneID 전에 반환하므로 정상 lobby rollback의 session은 여전히 zone0이고 인증된 상태다. 이미 삭제된 session이면 rollback도 거부된다. 기존 중복 lobby 삽입 거부/로그와 실패 응답 흐름도 유지된다. component에서 lobby Emigrate→재삽입을 검사하는 것은 실제 ZoneChange 전체 실패를 실행한 증거와 구분해야 하며 요청서가 이를 명시한다.

## 정확한 요청 범위·검증

Coordinator 요청서는 세 제품 파일의 위 patch, 기존 NonZoneThreadPool fixture 한 파일 확장, 격리 UnitTests/참조 라이브러리 및 MainServer **빌드만**을 지정한다. 네 기존 fixture friend나 reset을 되돌리지 않고 추가 제품 seam을 임의 적용하지 않는다. MainServer.vcxproj의 post-build는 $(OutDir)로 DLL을 복사하며 실행 명령은 없다. 프로젝트별 IntDir/격리 OutDir을 유지하고 실행 중 x64/Release/MainServer.exe를 덮어쓰지 않는 조건에서 MainServer 후보 build-only를 포함하는 데 차단 근거는 없다. 실제 후보 exe 실행·전환은 포함하지 않는다.

기존5개 기대를 유지하고 최초 성공/실패·중복/다른 UID·UID0·pending 미인증·완료/종료 양순서, 정상 lobby/중복/rollback 경계를 실제 handler/callback 또는 명시한 component 경계에서 확인하는 계획이 필요 범위와 맞는다. 두 작업의 동시 출발 검사는 결과만 관찰하며 두 lock 순서를 결정적으로 모두 실행한 증거로 과장하지 않는다. 새 hook 없이 불가능한 구간이 있으면 정확한 추가 seam을 별도로 검토한다. 이번 리뷰는 아직 구현되지 않은 테스트의 통과를 보증하지 않는다.

새 상태/락 변경 후 UnitTests 타깃과 관련 Net19를 각각 고정 exe로 한 번 실행하는 계획이 타당하다. 기존 2초 내부 대기·60초 외부 제한, 실패 원문 보존·같은 원인 수정2회 한도, 성공 후 불필요한 반복 중단을 요청서가 명시한다. 실 SQL·Unity 입력으로 자동 확대하지 않는다.

## 제한·복구·최종 판단

직접 Core Disconnect는 transport를 닫지 않는다. 이후 같은 transport/sessionID에서 재AUTH한 새 pending에 과거 callback이 섞일 수 있는 경로는 기존 코드에도 있었고 이 후보에 세대 구별은 없다. 이 정상 TCP close 한정 후보의 새 위험이라고 판단하지 않으며, AUTH만 generation을 추가해 ENTER 메시지 세대까지 일반화하는 변경을 이번 요청의 필수 조건으로 요구하지 않는다. 대신 일반 세션 재개 안전을 해결했다고 주장하면 안 된다.

기존 Zone mutex→shard와 Disconnect의 shard→Zone 역순, 중복 ENTER/전체 ZoneChange 원자성, 여러 DB worker 사이 저장 완료→load 순서, 서비스 종료 중 외부 callback/전역 객체 수명도 그대로 남는다. 이 한계를 문서/요청서가 명시하므로 후보 구현 자체를 그 전체 재설계로 확대하지 않는다. 실제 HP 보존이나 안전한 운영 전환 완료 판정은 별개다.

복구는 세 제품 파일의 정확한 새 hunk와 테스트 확장만 되돌리고 기존 R01 reset·friend·다른 변경을 보존한다. 서버 시작/종료·배포·DB 복원은 승인 범위에 없으며 별도 당시 PID/이미지·잔존 상태/저장·보존 빌드 복구안이 필요하다. 현재 후보는 승인 요청의 정적 타당성까지만 통과하며, **제품 적용·회귀 통과·배포 안전의 판정은 앞으로 남는다.**
