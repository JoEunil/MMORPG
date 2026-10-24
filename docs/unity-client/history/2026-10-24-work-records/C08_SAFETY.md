# C08 전환 안전성 — 구현 결정

2026-10-19 서버 분석 담당. 기준 HEAD `d7524822a8237cfec023f94cb18dc6abb8bb5a16`; 조사 시 CoreLib/ClientCore 제품 diff 없음. `AGENTS.md`, `roles/server-analyst.md`, `HANDOFF.md`, `C08_CONTRACT.md`와 아래 실제 소스를 읽었다. 이 문서는 후보 설계이며 제품 수정·빌드·테스트·Editor/서비스 조작을 하지 않았다. 실행 바이너리 일치 여부는 Coordinator의 환경 확인 기록을 따른다.

## 결정

세션 shard → 로비 또는 Zone 잠금 순서로 전환과 종료를 직렬화한다. 원 Zone 제거와 목적지 삽입을 하나의 잠금 경계 안에서 처리하고, 실패 검사를 제거 전에 마친다. 기존 5종 simulation broadcast에 Zone ID와 **입장 세대**를 추가한다. 별도 전송 큐·NetLibrary 락·세션별 패킷 복제는 추가하지 않는다.

Coordinator가 입장 세대안을 수용했다. 초기 검토했던 snapshot별 sequence/ACK fence는 채택하지 않는다. 세대는 성공한 입장마다 증가하는 Zone의 단일 uint64이며 tick/시간/개인 세대와 다르다.

## B: 잠금과 전환 원자성

### 전체 검색으로 확인한 역순 경로

`rg stateManager->|m_shards|smutex CoreLib`와 Zone 구현 파일을 대조했다.

| 경로 | 현재 순서 | 후보 |
| --- | --- | --- |
| StateManager::Disconnect / CleanUp | shard → lobby 또는 zone | 유지 |
| LobbyZone::ImmigrateChar | shard → lobby | 유지; 이미 있는 세션/인증/zone0 검사 보존 |
| ZoneState::ImmigrateChar | zone → SetZoneID → shard | Zone 내부에서 SetZoneID 호출 제거; shard 소유 전환 메서드가 sessionMap 직접 갱신 |
| ZoneState::FlushCheat | zone → StateManager::Cheat → shard | zone 아래 목록을 로컬 vector로 swap한 뒤 해제하고 Cheat 호출 |
| UpdateProfileVersion | shard 획득 후 해제 → zone | 역순 아님; 기존 흐름 유지 |
| 기타 ZoneState Actions/Skills/Monsters/Snapshots | zone만 획득 | 새 shard 접근을 넣지 않음 |

`BroadcastThreadPool::GetZone`은 초기화 후 불변인 Zone map 조회이며 shard 잠금이 아니다. `ZoneHandler`도 GetZone 후 개별 Zone 메서드를 호출한다. FlushCheat를 놓치면 Immigrate를 고쳐도 동일한 교착이 남는다.

### 최소 파일과 경계

- `StateManager.h/.cpp`: 전환 진입점 하나를 둔다. `sessionID/op → 성공 여부, 확정 CharacterState, generation, 최초 chatID`를 반환한다. 별도 Manager/인터페이스/범용 transaction은 만들지 않는다.
- `ZoneState.h/.cpp`: StateManager에 좁은 friend 접근 또는 잠금이 이미 잡힌 private immigration/emigration helpers를 제공한다. 현재 public 메서드의 lock을 다시 얻지 않도록 구분한다. 컨테이너 변경 로직은 한 군데에서 유지한다.
- `LobbyZone.h`: 초기 ENTER도 같은 shard 경계 안에서 lobby와 destination을 검사·이동할 수 있게 좁은 접근을 제공한다. 기존 비동기 DB 결과 삽입의 shard→lobby 검증을 약화하지 않는다.
- `NonZoneHandler.cpp`: GetZoneID→독립 Emigrate→Immigrate→rollback 코드를 위 전환 호출로 교체한다. 결과 패킷 전송은 잠금 해제 뒤 한다. 일반 전환의 chatID=0 의미는 유지한다.

### 일반 전환 순서

1. 해당 session shard의 unique lock을 잡고 세션 존재·인증·캐릭터 ID·현재 Zone을 재검사한다. Handler가 앞서 읽은 zoneID만 신뢰하지 않는다.
2. 현재 Zone/op로 실제 행·열 이웃을 계산하고 목적지가 생성된 1~4 안인지 검사한다. Zone3 RIGHT→4, Zone4 LEFT→3은 거절한다.
3. source/destination mutex를 **Zone ID 오름차순**으로 획득한다. 두 스레드의 역방향 왕복도 같은 순서를 쓴다. shard→두 Zone 이외의 역순 경로를 만들지 않는다.
4. source 세션과 characterID 일치, 목적지 중복/용량/내부 ID 발급 가능 여부, generation 상한, 유한 좌표, 두 Zone의 버퍼 포함 영역 교집합 포함 여부를 검사한다. 좌표를 clamp하거나 재배치해서 전환을 성공시키지 않는다.
5. 필요한 복사/목적지 준비를 먼저 완료한다. 정상 거절은 source 좌표·HP/MP·내부 ID·쿨다운·generation을 전혀 바꾸지 않는다. 목적지 삽입이 실패할 수 있는 경로를 source 제거 뒤에 남기지 않는다. 기존 STL 할당 실패를 성공 결과로 처리하지 않는다.
6. 목적지 내부 ID를 새로 발급하고 상태를 삽입한 후 source를 제거한다. 두 Zone 잠금이 잡혀 있어 다른 tick/조회는 중간 상태를 관찰하지 못한다. source에서 제거되는 원래 internalID와 destination의 새 ID를 혼동하지 않는다.
7. 목적지 generation을 성공 commit에서 정확히 한 번 증가시키고, `session.zoneID`를 목적지로 바꾼다. 같은 잠금 아래 ACK용 상태/generation을 복사한다. 목적지 snapshot은 이 잠금이 풀린 뒤 새 generation을 stamp한다.
8. shard 소유 중 채팅 Join을 enqueue하여 전환 commit 뒤 Disconnect Leave/Delete보다 늦게 Join이 생성되는 경합을 피한다. 채팅 worker에는 StateManager 역참조를 추가하지 않는다. 네트워크 ACK 전송은 잠금 해제 뒤 수행한다.

Disconnect가 먼저 shard를 얻으면 전환은 존재하지 않는 세션을 거절한다. 전환이 먼저 얻으면 Disconnect는 commit된 목적지에서 정확히 한 번 제거/저장한다. 정상 실패에 rollback immigration을 사용하지 않으므로 원래 ID/세대가 바뀌거나 실패 복원이 다시 실패하는 경로를 없앤다.

초기 ENTER도 shard→lobby→destination 순서로 같은 원칙을 적용한다. 저장 Zone/좌표 유효성, 목적지 존재/용량/ID/generation을 먼저 확인하고, 목적지 삽입 성공 전 lobby 행을 지우지 않는다. 성공 시에만 최초 chat session 생성/Join을 enqueue한다. 닫힌 세션을 다시 만드는 늦은 ENTER 회귀를 유지한다.

`FlushCheat`의 로컬 목록은 zone 아래 swap만 하고, 시간 계산과 각 Cheat 호출은 해제 뒤 한다. 필요 이상으로 새 공유 상태/큐를 추가하지 않는다.

### 보존과 정리

최초 진입의 빈 skillSlot에만 기본 0/1/2를 채운다. 왕복 시 첫 3개 슬롯과 cooldown, MP를 보존하며 append를 반복하지 않는다. 이미 있는 불필요한 뒤 슬롯을 정리하더라도 첫 3개 값을 바꾸지 않는다.

성공 source 제거 때 원 세션의 character activeSkills와 옛 internalID의 actionResults/dirty 목록을 정리한다. `ProcessCellActiveSkills`는 caster를 sessionID로 찾으므로, 제거하지 않은 옛 스킬이 빠른 재입장 후 새 캐릭터를 caster로 재사용할 수 있다. 다른 캐릭터/몬스터 스킬은 보존한다. 실패 거절 때 이 정리를 수행하지 않는다.

## C: broadcast 입장 세대

### 정확한 wire

packed little endian, opcode/공통 8바이트 헤더/레코드 배열 stride는 유지한다.

| 대상 | 새 body |
| --- | --- |
| op7 캐릭터 Full, op8 Delta, op9 몬스터 Full, op10 Delta, op12 Action | `count:u16, zoneID:u16, zoneGeneration:u64, 기존 records[count]` |
| op17 ZoneChangeResponse | 기존 23바이트 body 뒤 `zoneGeneration:u64` 추가. 전체31→39바이트 |
| op16 ZoneChangeRequest | 기존9바이트 유지 |

5종 broadcast의 header+고정 body는 10→20바이트다. count는 offset8에 유지하여 `PacketWriter::AddChunk`의 count offset을 바꾸지 않는다. zoneID offset10, generation offset12, records offset20이다. ACK generation은 offset31이다. 실패 ACK는 body 전체 zero-init, status0, generation0이며 클라이언트는 성공 필드를 소비하지 않는다. 성공 ACK는 generation>=1이다. flags는 명시 초기화하고 SIMULATION만 설정한다.

### stamp 규칙과 보장

`ZoneState`에 `m_generation:uint64=0` 하나를 둔다. **성공 immigration commit 때만 ++** 한다. source 퇴장/실패 요청/packet 생성 때는 증가시키지 않는다. UINT64_MAX이면 다음 입장을 source 제거 전에 거절한다. 재시작은 연결도 새로 만들어지므로 이전 TCP 연결의 세대와 혼합하지 않는다.

각 snapshot 메서드는 현재처럼 Zone mutex를 잡은 상태에서 generation을 읽고 그 호출의 모든 cell header에 같은 Zone ID/generation을 넣는다. Broadcast worker에서 나중에 현재 generation으로 덮어쓰면 오래된 chunks가 새 세대로 위장하므로 금지한다. `PacketWriter::Get*Header(zoneID,generation)`에 전달하고, `ZoneState_Snapshots.cpp`의 5종 호출부를 수정한다. `AddChunk`는 length/count만 합치며 세대는 보존한다. record chunks는 wire 변경이 없다.

클라이언트는 ACK에서 수락한 zoneID와 entryGeneration을 저장하고 `packet.zoneID == currentZone && packet.generation >= entryGeneration`인 5종만 적용한다. `==generation`으로 검사하면 나중에 다른 플레이어가 입장하면서 증가한 유효 snapshot을 잃으므로 안 된다. 개인 전환 세대나 별도 tick 카운터를 추가할 필요가 없다.

증명: Zone 입장이 generation G로 commit되기 전에 만들어진 모든 해당 Zone snapshot은 generation<G다. commit과 stamp가 같은 mutex로 직렬화된다. 반대 Zone 데이터는 zoneID로 거르고, 같은 Zone 왕복 전의 지연 데이터는 G보다 작아 거른다. 다른 플레이어 입장으로 G+1이 된 snapshot은 현재 입장 이후 생성됐으므로 유효하다. Full/Delta/Action이 같은 tick에서 같은 G를 쓰는 것은 허용되며, G는 packet 순번이 아니다. G로 5종 사이의 순서를 정렬하거나 한 종류 수신 뒤 다른 종류를 버리지 않는다. 기존 Zone 내부 packet 재정렬 정책을 이번 변경의 새 보장으로 주장하지 않는다.

전환 요청 중 ACK 전 새 Full은 버려도 된다. 성공 ACK 뒤 일치 Full에 자기 새 internalID가 있을 때까지 조작을 닫고 다음 periodic Full을 기다린다(현 FULL_SNAPSHOT_TICK=1초). ACK 전 도착한 packet을 현 Zone 데이터로 적용하거나 오래된 pendingFull을 새 상태에 재사용하지 않는다. 실패 ACK는 기존 confirmed Zone과 worldReady를 보존한다. 새 Full 전 Delta/Action 적용과 C04/C05 효과 재생을 보류하고, 관찰자 이벤트도 gate 이후에 내보낸다.

### 파일/호환 영향

- `CoreLib/PacketTypes.h`, `PacketWriter.h/.cpp`: 위 6종 body/header 길이 및 생성 인자. `AddChunk` count offset 유지 확인.
- `ZoneState.h/.cpp`, `ZoneState_Snapshots.cpp`: generation commit/stamp.
- `ClientCore/PacketHelper/Packet.cs`, 실제 NetworkService parser, ViewModel/ViewData: 엄격한 새 크기/필드 검증과 gate. Unity 쪽에 wire 복사본을 만들지 않는다.
- 현재 wire와 비호환이다. 새 서버와 새 ClientCore DLL을 같은 후보로 교체한다. 구 길이를 동시에 받아들이는 fallback은 stale 보장을 없애므로 넣지 않는다. 기존 비Unity 클라이언트를 새 서버에 연결해 호환된다고 주장하지 않는다. snapshot 관찰 테스트/fixture/peer 도구의 offsets도 함께 바꾼다.

DB 스키마/저장 계약/계정/재화 변경 없음. 복구는 무접속 정상 종료 후 보존한 C07 서버와 C07 DLL을 짝으로 복원한다. 새 서버에 옛 DLL만 연결하는 혼합 복구를 하지 않는다.

## 구현 후 독립 리뷰·단일 배치 회귀

목표/예산/종료 조건은 Coordinator C08 작업 카드에 먼저 고정한다. 이 분석에서 테스트를 실행하지 않았다.

1. 서버 정적 락 검사: Zone lock 아래 StateManager shard 호출0, 왕복 두 Zone 잠금 순서 동일. transfer-vs-Disconnect/CleanUp 및 FlushCheat 경합이 제한 시간 안에 종료되고 캐릭터/세션 orphan이0인지 확인.
2. 서버 거절: 존재하지 않는/행 경계 이웃, 먼 좌표·NaN/Inf, 목적지 full/ID고갈/generation고갈, 닫힌 세션. source 상태/ID/스킬/세대 무변경. 초기 ENTER 늦은 응답/닫힘 회귀 포함.
3. wire: 5종 count/길이/stride/flags,31바이트 legacy ACK 및10바이트 legacy snapshot 거절, failed ACK zero, 성공 generation1 이상. AddChunk 후 count/zone/generation 보존.
4. ClientCore: 다른 Zone 동일 internalID, 왕복 이전 같은 Zone의 작은 generation Full/Delta/Monster/Action 모두 무반영; 타인 입장으로 큰 generation 정상 수용; ACK 전 Full 후 다음 Full 대기; 실패 ACK 기존 worldReady 유지. UI에 raw 관찰 이벤트만 먼저 새는 것도 금지.
5. 정상 왕복: source에서 스킬 발동→이동→빠른 복귀 시 옛 activeSkill이 새 caster로 부활하지 않음. 슬롯 수3/기존 cooldown·MP 보존, 중복 방향 요청으로 두 번 건너뛰지 않음.
6. 실제 UI 한 배치: 정상 로그인→예산에 맞춘 경계 이동→Zone2 ACK/자기 Full→조작→Zone1 복귀→정상 종료→재접속 첫 Full의 저장 Zone/위치/HP/MP 확인. 임의 DB/좌표 수정으로 경계 이동을 대체하지 않는다.

후보 diff와 위 결과를 독립 리뷰한 뒤 Coordinator가 PR 직전 한 번 Slack 승인을 요청한다. 일반 로컬 정상 재시작/재시험 승인은 따로 만들지 않는다.
