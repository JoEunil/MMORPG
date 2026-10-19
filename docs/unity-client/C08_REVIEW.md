# C08 독립 리뷰

2026-10-19, C08 구현 후보 정적 검토. 제품·테스트 코드 수정, 테스트 실행, Editor/서비스/Slack/Git 변경은 하지 않았다. 루트 AGENTS·reviewer 역할·C08_TASK·C08_SAFETY·HANDOFF를 대조했다. 서버 계약·동시성 부분은 high 상향을 요청했으나 역할 도구 설정 변경 없이 검토했으며 Coordinator도 해당 경로를 직접 대조한다.

## 지적

1. **P1 / 신규 회귀 차단 (수정 확인)** — `UnityClient/Assets/Tests/C02/C08ZoneTransitionTests.cs`의 InventoryFixture.Arrive는 Transform으로 저장한 ownCharacter를 GameObject로 캐스팅해 fixture 생성 중 InvalidCastException을 낸다. mapping 외 신규 UI 회귀를 실행할 수 없다. Coordinator가 Transform.gameObject로 바꾼 현재 파일을 재확인했다. 제품 코드는 영향 없다.
2. **P2 / 신규 회귀 기대값 불일치 (수정 확인)** — 같은 파일 FailedInventoryReplyDuringTransitionAllowsManualRetryAfterArrival의 `Does.Contain("after travel")`은 C07InventoryScreen이 실제 출력하는 `refresh after arrival.`과 달라 정상 동작에도 실패한다. 실패 수신·이동 중 잠금·도착 후 재시도 assertion은 유지하고 문구 기대값만 실제 문장에 맞춘다. Coordinator가 `after arrival`로 수정한 현재 assertion을 재확인했다.

## 검토 범위와 결론

ClientCore Interfaces/Handler/ReceiveTick/Packet/ViewModel, Unity C02WorldScreen/C07InventoryScreen/C02SceneBuilder, C08 신규 fixture 및 기존 C02~C07 fixture migration, Switch/Start-R01Candidate C08 모드, CoreLib StateManager/ZoneState/LobbyZone/NonZoneHandler/PacketTypes/PacketWriter/Snapshots와 NonZoneThreadPool 신규 회귀를 읽었다. 서버 파일은 최종 const CharacterState& immigration 버전까지 대조했다.

- 새 ACK 39바이트와 simulation envelope 20바이트, count/zone/generation offset 및 packed 레코드 stride가 양쪽에서 일치한다. legacy 길이는 의도적으로 거절하므로 C07 서버·DLL과 섞지 않고 짝으로 전환/복구해야 한다.
- shard → lobby 또는 오름차순 Zone 잠금과 destination 준비 후 source 제거를 대조했다. FlushCheat는 Zone 잠금을 해제한 뒤 shard를 호출한다. 쿨다운·MP 복사, 슬롯 중복 누적 방지, 옛 caster/action/dirty 제거, 초기 ENTER 거절의 lobby 보존을 확인했다.
- 수락 Zone 및 entry generation보다 작은 5종 simulation 패킷은 dispatcher에서 관찰 이벤트 전에 거른다. ACK 후 자기 internalID Full까지 새 입력을 잠그며, 일반 실패 시 이전 worldReady를 복구한다. C04/C05는 기존 WorldStateChanged 수명 경계를 재사용한다.
- C08 전환 UI timeout은 조작을 잠근 채 재접속을 요구하고, 이동 중 같은 캐릭터의 기존 인벤토리 응답은 저장하되 새 조회 UI는 열지 않는다.
- C08 서버 전환 모드는 고정된 기존 C07 PID/시작시각/경로/해시와 별도 C08 후보를 사용하고, native q 자식에도 C08 인자를 전달한다. 기존 무접속·로그/WAL·정상 종료·후보 검증 gate를 제거하지 않았다.

**최종 결론: 코드상 차단 문제 없음.** 지적 2건의 Coordinator 수정까지 정적으로 재확인했으며 배치를 수행할 수 있다. 컴파일·C++/Unity 회귀·실제 왕복·캡처·실행 바이너리 일치는 이번 리뷰에서 실행하거나 통과로 판단하지 않았다. 코드 리뷰는 실행 검증을 대신하지 않는다. 구체적인 추가 단순화 요구는 없다.


추가 정적 검토: `C08LiveZoneUiTests.cs`는 실제 입력을 생성하지 않고 서버 OwnFull/WorldStateChanged만 관찰한다. Finish가 전환 2회와 정상 UI 퇴장을 요구하므로 단순 화면 표시로 성공하지 않는다. 경계에서 이미 전송된 이동이 남으면 마지막 수신 좌표와 ACK가 정상적으로 다를 수 있으므로 이동 정지 후 Full 안정화 뒤 버튼을 누르는 실제 배치 전제를 Coordinator에게 전달했다. 전환 중 정상 피격도 HP 정확 비교에 영향을 주므로 무피격 경로에서 보존을 확인한다. PNG는 비동기 생성 후 파일 완료/시각 검수가 별도로 필요하다. 이 helper는 재접속 저장 확인을 대신하지 않는다.

## 최종 서버 동결 차이 재검토

StateManager.cpp / ZoneState.cpp·h / NonZoneThreadPool.cpp의 최종 diff만 추가 검토했다. ChangeZone은 source CharacterState를 const 참조로 전달하며, 목적지 ImmigrateCharLocked 내부의 한 번의 복사만 수행한다. source와 destination이 서로 다른 Zone이고 두 mutex가 계속 잡혀 있으므로 destination 준비 동안 source 참조 수명은 유지된다. 할당 실패는 기존 source 제거 전에 처리하며, 원본 source의 ID/lastZone을 바꾸지 않는다. public ImmigrateChar는 성공한 경우에만 호출자 출력 ID/lastZone을 갱신한다.

AcquireInternalID의 선언/구현 제거 후 CoreLib·UnitTests에 남은 참조가 없으며, ID 읽기·상한 검사·commit 저장은 m_mutex 안에 있다. FlushCheat 신규 회귀는 shard를 잡은 채 별도 flush를 시작하고, Zone 잠금을 얻어 빈 목록을 관찰해야 통과하도록 하여 flush가 swap을 수행하기 전의 우연한 try_lock 성공을 통과로 삼지 않는다. 이후 shard를 해제하고 future를 회수한다. 추가 차단 결함 없음, 테스트 실행 없음.

정적 검토 파일 SHA256: StateManager.cpp E8002EB82FD545B972A9933B719FA309064B439F1B099C1F82A2873533DCC2FC; ZoneState.cpp 1AE1F1420E43836573C1A126CE49E53558DD11D40F4B7BF1A087EAEC46E226D0; ZoneState.h 2930EB502B8FDAC9DC2D6C07AD72A0FCB18073AE7341EFD32741C3AF71FA1E74; NonZoneThreadPool.cpp 0A3CED2EE44C0725CF0C35159E490DEFFF20EB4727123B7AB05611497A10321D.

## 첫 공통 배치 후 fixture 수정 재검토

Coordinator가 보존한 `Captures/c08-unity-first.txt`의 71개 중 69 통과/2 실패 기록을 읽고, 요청된 수정만 추가 정적으로 확인했다. 직접 테스트 실행은 하지 않았다.

- 중복 전환 회귀는 fixture 초기 ENTER 횟수를 baseline으로 빼고 두 번 버튼 요청의 증가량이 정확히 1인지 확인한다. 전환 단일 요청 요구를 완화한 것이 아니다.
- inventory fixture는 UI 비공개 handler 직접 호출을 없애고 IViewModelNet.InventoryReceived를 통해 실제 ViewModel pending 해제·ViewData 저장·UI 이벤트 경로를 통과한다. SetItems는 응답 배열만 보관하고 Receive에서 이를 전달한다. 실패 후 새 요청 회귀가 production 수명 경계를 제대로 검증한다.
- NullLogger의 ClientCore.ILogger 한정은 UnityEngine.ILogger와의 이름 충돌만 제거한다.
- 사용처가 없는 public ZoneState.ImmigrateChar wrapper의 선언/구현 삭제를 확인했다. 저장소의 cpp/h/cs 참조 검색에서 남은 일반 ImmigrateChar는 별개 LobbyZone 경로뿐이다. 모든 field admission은 StateManager의 shard 경계 아래 private ImmigrateCharLocked로 들어간다. 위 이전 절의 public wrapper 설명과 당시 ZoneState 해시는 이 삭제 이전 기록이다.

수정에서 추가 차단 결함 없음. 영향받는 신규 UI fixture 재검증 결과는 Coordinator가 별도로 기록해야 하며 첫 실패 기록은 유지한다.

## 실제 ENTER 실패 후 TCP 수신 경계 수정 재검토

최초 리뷰 범위에 TCPSocket이 빠져 legacy ACK 31바이트 검사가 남은 것을 발견하지 못했다. Coordinator가 실제 입력 a 실패를 보존하고 확인한 원인이다. 이번에는 TCPSocket.cs와 C01TcpFrameTests.cs만 추가 정적 검토했다. 직접 실행하지 않았다.

TCPSocket은 op17 길이를 정확히 39로, 5종 simulation 최소 길이를 20으로 변경했다. 기존 magic·1MiB 상한·flags·불완전 프레임 대기·소켓 해제 검사는 유지한다. 내부 Packet parser의 ACK39/envelope20과 일치한다. 새 loopback 회귀는 ACK 앞 2바이트를 먼저 보내고 나머지 ACK+Full을 이어 보내 실제 분할/병합 수신이 두 원본 프레임 그대로 순서대로 dispatch되는지 확인한다. invalid-frame 루프는 기존 6종을 유지하고 legacy31 ACK/10 snapshot을 추가해 안전 종료 및 dispatch0을 요구한다. 새 검사는 상위 parser 의미 검증을 대신하지 않고 실제 TCP 경계의 누락을 검증한다.

수정에서 추가 차단 결함 없음. 영향 TCP 3개 회귀와 실입력 재검증은 Coordinator 기록을 따른다. 다른 71개 회귀를 이 수정만으로 다시 요구하지 않는다.

## 실입력 b 종료 observer 수정 재검토

C08LiveZoneUiTests만 추가 검토했다. ResetWorld 도중 Stage가 아직 InWorld여도 HasOwnFull=false이면 arrival 판정을 하지 않도록 바뀌어 Zone0 임시 초기화 상태를 정상 입장으로 오인하지 않는다. 실제 전환은 기존 awaiting·좌표·HP/MP·횟수2 검사를 그대로 유지한다. Observe의 assertion 예외는 실패 문자열과 FAIL OBSERVATION 로그로 보존하고 Finish가 observationFailure=null을 필수 요구하므로 실패를 성공으로 바꾸지 않는다. observer가 제품의 ClientChanged/Reset 콜백 체인을 끊던 경로를 제거한 수정이다.

추가 차단 문제 없음. 제품 변경 없음/리뷰어 실행 없음. b의 부분 성공·종료 실패 기록은 유지하고 c 실제 배치로 정상 퇴장까지 다시 확인해야 한다.

## 최종 UI 위치·TMP 생성 에셋 추가 검토

C02SceneBuilder 전환 버튼의 anchoredPosition.x=40을 확인했다. 이전 0에서 가로 위치만 40 이동했으며 버튼 크기·높이·핸들러·전환 판정 변경은 없다. 실제 화면의 채팅 겹침 해소와 최종 c 캡처 검수는 Coordinator의 실행 근거이며 리뷰어는 정적 확인만 했다.

기존 LiberationSans SDF - Fallback.asset의 diff를 읽고 optimize-text-mesh-pro SKILL.md의 동적 fallback 기준을 적용했다. 기존 source font GUID·material/atlas 참조를 유지하며 U+2192/U+2190 문자와 glyph 2개가 추가됐다. 두 glyph의 scale=1, atlas index=0, glyph rect가 512×512 안에 있고 실제 atlas payload는 262144 bytes로 Alpha8 크기와 일치한다. 동적 population=1, multi-atlas=1, clearDynamicDataOnBuild=1, padding9/renderMode4169가 유지된다. Unity6000 material/texture serializedVersion 갱신과 기존 필드 재배치는 확인했으며 새 폰트·패키지 추가는 없다. 관련 없는 크기/품질 변경은 요구하지 않는다.

추가 차단 문제 없음. 코드/에셋 수정·기능 재시험·Editor 조작 없음. c 실제 왕복/정상 종료와 저장 데이터 일치 여부는 Coordinator 결과 기록을 따른다.
