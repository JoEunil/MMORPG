# R01-REGRESSION-02/r1 독립 리뷰

2026-10-18. **추가 회귀 도구의 정적 판정: 코드상 차단 문제 없음. 실제 늦은 AUTH/ENTER 두 실패를 확인했으므로 제품 배포 차단을 유지한다.** 최종 동결 hash/diff와 담당자의 단일 실행 기록을 읽어 대조했다. 리뷰어는 이 문서만 작성하고 제품·테스트·빌드·Unity·DB·서비스를 변경하거나 실행하지 않는다.

## 승인·소유권 범위

Coordinator가 지정 사람 U0C9QEUHPMK의 V04 부모 `1792326340.552749` 아래 회신 `1792328764.170199` 원문 `R01 스레드의 !coord 승인 R01-REGRESSION-02/r1 추가회귀 재개`를 전체 스레드에서 확인했다고 인계했다. 리뷰어의 직접 Slack 조회가 아니다. 이전 HANDOFF/요청서의 승인 대기 문구보다 이 최신 확인을 적용한다.

허용 변경은 PacketWriter.h·MessageQueueHandler.h·LobbyZone.h·StateManager.h의 동일 NonZoneThreadPoolFixture friend 각 한 줄과 기존 UnitTests/CoreLib/NonZoneThreadPool.cpp의 회귀 확장이다. 기존 격리 후보 빌드만 사용한다. 제품 경합 수정·새 API·인증 정책·운영·DB·Unity는 제외한다. 기존 reset 후보와 다른 작업자의 변경을 보존한다.

## 동결 후 확인할 경계

- AUTH는 실제 worker/NonZoneHandler::Process에서 받은 callback을 보류하고, PacketView 반환과 실제 Disconnect 처리 완료를 먼저 확정한 뒤 실제 saved callback을 호출해야 한다. 결과는 Core session 부재다. 요청 복사값만 검사하거나 callback을 생략하는 기존 테스트와 구분한다.
- ENTER는 유효한 인증 session에서 실제 요청을 처리하여 제어 가능한 queue에 복사된 요청을 관찰해야 한다. 실제 zone0 Disconnect 완료 뒤 실제 MessageQueueHandler::Process의 성공 로드 응답을 재개한다. 결과는 Core session 및 lobby character 부재다. 유효 요청 전제 없이 임의 성공 응답을 보내는 것으로 대신하지 않는다.
- 대기는 제어 신호/worker join 등으로 순서를 결정하고 bounded 종료를 유지한다. sleep/생존 선조회로 경합을 해결했다고 하지 않는다. callback·queued data가 의존 객체보다 오래 살아 teardown에 오염을 남기는지 확인한다.
- 네 header는 friend 외 제품 동작을 변경하지 않아야 한다. baseline에서 ghost 상태가 드러나면 올바른 기대의 실패로 보존하며, 결과를 통과시키는 제품 수정·assertion 완화는 이번 범위에서 금지한다. fixture의 정확성 판정과 제품 배포 차단을 별도로 기록한다.

최종 source 검토 뒤 별도 tester가 고정 exe를 실행할 예정이며, 리뷰어는 회귀를 직접 실행하지 않는다.

## 최종 동결 정적 검토

아래 다섯 파일의 SHA256을 직접 계산하여 구현자 동결과 일치함을 확인했다.

| 파일 | SHA256 |
| --- | --- |
| CoreLib/PacketWriter.h | `C47D016187195C983F2CF710D94379FE465E0CB9804D35AC9BAFC7D671282207` |
| CoreLib/MessageQueueHandler.h | `6D6C007A55F87892122BC37848B6070FF3EB9B7460EE4533ED06D54D0B6670D8` |
| CoreLib/LobbyZone.h | `A0FDA201C94FB15328D129B27218668DB8149E2F079654B414E94B0A7E80B1DD` |
| CoreLib/StateManager.h | `835DF6B2927908BD154BB1057BEED64EC5D597A291C87D65E23F47E5E8ED3777` |
| UnitTests/CoreLib/NonZoneThreadPool.cpp | `57362DEFBD45C6F5DF21874121F170F7196DF196844629E02EA740ECD51059FF` |

네 header의 diff는 동일 fixture friend 각 한 줄뿐이다. StateManager 접근은 lobbyZone 의존성 하나만 연결하고 전체 Initialize/Zone 생성·worker topology를 시작하지 않는다. 원 후보 reset과 승인된 두 friend/프로젝트 등록 외 새 제품 동작 수정은 없다.

AUTH 테스트는 실제 worker/handler가 보관한 SessionCallbackData를 ControlledAuth가 소유한다. WaitForRelease 후 Disconnect를 enqueue하고 production Stop의 join으로 해당 이벤트 완료를 확정한다. Core session과 lobby 부재를 먼저 확인한 뒤 CompleteSuccess가 저장된 **실제 ResponseSession callback**을 호출한다. callback 뒤 GetZoneID=-1/GetUserID=0/lobby 부재를 다시 검사한다. callback을 생략하거나 ghost를 기대값으로 삼는 경로가 아니다.

ENTER 테스트는 실제 AUTH callback 성공으로 user6982/zone0과 AUTH_RESPONSE를 확인한다. 실제 worker의 ENTER_WORLD(5012)가 만든 복사 load request의 message type·user6982·character5012를 대조한다. 요청 관찰만으로 처리 완료를 가정하지 않고 Enter PacketView 반환까지 기다린 뒤 Disconnect와 Stop join을 진행한다. 그 뒤 pool에서 얻은 성공 응답 Message를 실제 MessageQueueHandler::Process에 전달한다. 응답의 sessionID/charID는 보류한 요청에서 가져오며 Process가 MessagePool에 반환한다. 종료 전과 늦은 응답 후 모두 Core/lobby 부재 기대를 유지한다. 안전한 요청/응답 복사 수명을 통해 로드 완료 순서를 고정하며 실제 DB/Redis를 부르지 않는다.

세 기존 fixture의 다음 enqueue 없는 반환·정확히 한 번 Release·stop/drain·복사 요청 기대는 유지된다. 새 TestPacket/Pool 및 RecordingIOCP는 실제 writer 출력을 in-memory로 받는 데 한정되고 새 범용 추상화가 아니다. callback/response 호출은 테스트 스레드에서 제어하며 worker를 join한 뒤 확인한다. teardown은 보류 gate를 열고 worker를 종료한 다음 packet storage/의존 객체를 해제한다. 비밀 파일·실계정 토큰은 읽지 않고 합성 token을 출력하지 않는다. TEST_F 파생 body의 private 직접 접근도 없다.

## 실행 인계·판정 한계

기준 소스의 ResponseSession은 종료 뒤에도 AddSession을 호출하고, CharacterStateResponse는 종료 뒤에도 LobbyZone::ImmigrateChar를 호출한다. 따라서 **AUTH의 Core ghost 및 ENTER의 lobby ghost로 새 두 테스트가 실패할 것으로 예상**한다. 이는 정적 예상이고 실행 결과가 아니다. 도구의 기대가 올바르다는 판정과 제품이 그 기대를 만족한다는 판정은 다르다.

현재 추가 회귀 빌드는 구현자 진행 중이며 최종 exe hash·실제 테스트 결과는 아직 이 판정에 없다. 별도 tester가 최종 고정 exe의 NonZoneThreadPoolFixture 5개를 한 번 실행하고 실패 원문을 보존한다. 새 두 회귀 실패 시 제품 수정은 별도 분석·승인 대상이며, 이번 범위에서 고치거나 assert를 완화하지 않는다. 기존 후보 3/3·별도 Net 관련 19/19 결과는 이전 exe/단계의 근거이며 이번 새 회귀의 통과로 재사용하지 않는다. 이번 변경은 제품 동작을 바꾸지 않아 Net19를 다시 반복할 필요는 없다. 리뷰어는 소스·테스트·Editor·실행 환경을 변경하지 않고 역할을 반납한다.

## 실제 실행 결과 수용 — 후속 갱신

별도 실행 담당의 `UnityClient/Captures/r01-late-completion-regression.txt`를 읽었다. 고정 exe `F60BFB7D520C7E8FA3DDD5AABA57D78904D6D7A06A1914402833DA9248DA0E5C`로 NonZoneThreadPoolFixture 5개를 정확히 한 번 실행했다는 기록이다. exit1, 기존3 PASS/늦은 완료2 FAIL, gtest5ms/프로세스1.492초이며 60초 종료 예산 안이다. 리뷰어가 exe와 위 다섯 소스의 SHA256을 다시 직접 읽어 기록과 일치함을 확인했다. 리뷰어 직접 실행 결과는 아니다.

- AUTH 실패: 늦은 성공 callback 후 GetZoneID(42)=0, GetUserID(42)=6982로 종료된 Core session이 다시 생겼다. 기대값 -1/0은 그대로 유지됐다.
- ENTER 실패: 늦은 성공 응답 후 HasLobbyCharacter(42)=true로 종료된 session의 lobby character가 남았다. 기대 false는 그대로 유지됐다.

이는 기존 정적 가설의 두 결정적 순서가 고정 후보에서 실제로 드러난 결과다. 회귀 도구의 실패를 테스트 결함으로 재분류하거나 제품 안전 통과로 표현하지 않는다. 원본3개는 계속 통과했지만 두 경합은 미해결이며 제품 보완·운영 승인은 별도다. 이 실행에서 Net19를 반복하지 않았고 기존 결과를 두 경합의 증거로 대체하지 않았다. 코드·기대값 변경 없이 결과를 수용한다.
