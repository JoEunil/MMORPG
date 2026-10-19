# C04 — 타 플레이어·기존 몬스터 관찰 표시 계약

2026-10-18 KST 사전 조사. C03 구현과 독립적으로 현재 Zone의 타 플레이어와 기존 몬스터 type0/1/2 표시 경로만 읽었다. 이동/전투/스킬 입력·제품 수정·Editor/Play/입력·DB/서비스 조작·하위 에이전트 실행은 하지 않았다. 이 문서만 작성했으며 C03 파일/동결 범위는 변경하지 않았다.

## 기준과 결론

- HEAD `1d5a3d69074a1de6bafde30f1ec4dfe5cac1a951`에 C01/C02 및 진행 중 C03 미커밋 변경이 있다. 현재 파일을 기준으로 읽었고 다른 작업자의 변경을 보존했다.
- 작업 체크아웃 MainServer.exe SHA256 `C0F689BD0E58D3A20282143D5DD5C0C15514F42F192A6FF70568C2DDF312E825`가 HANDOFF 배포 기록과 일치한다. 이번 조사는 실제 프로세스·실응답 검증이 아니다.
- **새 wire/서버 변경 없이 기존 Full/Delta를 표시할 수 있다. 정확한 즉시 despawn/AOI 이탈/타인 연결 종료 판정은 불가능하다.** 최근 Full에 표시 유효기간을 부여한 제한된 관찰 정책은 가능하며, 이를 서버의 사망/삭제 통보로 가장하지 않는다.
- 이름은 C04 최소안에서 생략할 수 있다. PROFILE24/25는 서버의 실제 경로가 있으나 현재 ClientCore는 미지원이다. 이름을 붙이기 위해 DB나 로그인 정보를 읽을 필요는 없다.

## 실제 전송 경로·프레임

`CoreLib/ZoneThreadSet.cpp` → `ZoneHandler.h:RunTick` → `ZoneState_Snapshots.cpp`의 Full/Delta/MonsterFull/MonsterDelta → `PacketWriter.cpp/.h` → `BroadcastThreadPool.cpp` → `IIOCP::SendDataChunks`. 클라이언트는 `TCPSocket` → `Handler`의 flags=1 분기 → `ReceiveTick` → `ViewModel` → dispatcher → Unity 메인 스레드 이벤트로 받는다.

공통 헤더는 pack1/little endian 8바이트(magic0xABCD/u16,length/u32,opcode/u8,flags/u8). 아래 snapshot flags=1, offset8 count/u16, offset10부터 실제 count개 필드다. 선언된 최대 배열 전체를 전송하지 않는다.

| 내용 | opcode | 필드 크기 / 전체 길이 | 식별자 |
| --- | --- | --- | --- |
| 플레이어 Full | 7 | 43 / `10+43*count` | Zone별 zoneInternalID/u32 |
| 플레이어 Delta | 8 | 10 / `10+10*count` | zoneInternalID/u32 |
| 몬스터 Full | 9 | 25 / `10+25*count` | Zone별 internalId/u16 |
| 몬스터 Delta | 10 | 8 / `10+8*count` | internalId/u16 |

근거는 `CoreLib/PacketTypes.h`, `ClientCore/PacketHelper/Packet.cs`, `PacketWriter`다. 네 형식은 현재 ClientCore와 일치한다. `ReceiveTick`은 opcode9/10을 이미 처리하며 `IViewModelUI.OnMonsterFullReceived/OnMonsterDeltaReceived`, 플레이어 OnFull/OnDelta 이벤트가 있다. 기존 파서는 count 할당 전에 정확한 길이를 검사한다. 별도 UI용 복제 네트워크 계층은 필요 없다.

## 플레이어와 몬스터 필드

플레이어 Full 상대 offset: internalID u32(0), profileId u32(4), profileVersion u32(8), hp/mp/maxHp/maxMp i32(12/16/20/24), exp u32(28), level u16(32), dir u8(34), x/y f32(35/39). 타인의 실제 characterID/accountID/로그인 ID는 이 wire에 없다. 본인은 C02의 InternalID로 제외하고, 표시용 타인 ID는 서버가 준 Zone-local ID를 사용한다.

플레이어 Delta 상대 offset: internalID u32(0), fieldID u16(4), raw u32(6). field0..9는 hp/mp/maxHp/maxMp/exp/level/dir/x/y/profileVersion. x/y는 raw 비트의 IEEE754 재해석이다. Full의 profileId는 Delta로 변경되지 않는다. 프로필 version 변경은 이름 무효화 신호일 뿐 생성/삭제가 아니다.

몬스터 Full 상대 offset: internalId u16(0), hp i32(2), maxHp i32(6), attacked u32(10), x/y f32(14/18), dir u8(22), monsterId u16(23). 합계는 25바이트다. 서버/ClientCore 구조체에 패딩이 없음을 검증 fixture에서 고정해야 한다.

몬스터 Delta 상대 offset: internalId u16(0), fieldId u16(2), raw u32(4). 실제 writer가 보내는 field는 0=hp/i32,1=x/f32,2=y/f32,3=dir/u8. 필드 슬롯의 남는 바이트는 0으로 채운다. MonsterState의 attacked 주석상 field4가 있으나 `DeltaSnapshotMonster`는 이를 전송하지 않는다. Full의 attacked는 writer에 있으나 이번 경로 조사에서 유효한 피해 누적 대입을 찾지 못했다. C04는 피해 숫자/VFX를 만들지 않는다.

`monsterId`는 `MonsterData.id`(type0/1/2)이며 엔티티 고유 ID가 아니다. `ZoneState_Monsters.cpp:InitializeMonster`는 Zone마다 internalID를 **0부터** 발급한다. 0을 invalid sentinel로 거부하면 첫 몬스터를 누락한다. 키는 `(현재 세션/Zone 세대, 종류, internalId)`로 구분해 플레이어와 몬스터 숫자가 같아도 충돌하지 않는다. 새 프레임워크 없이 두 명시적인 목록으로 충분하다.

기존 spawn은 5×5 Cell에서 중앙을 제외한 각 Cell에 type0 8마리/type1 2마리, 중앙에 type2 1마리, 합계 Zone당 241마리다. `MonsterData.h`의 기본 maxHp=1500/3000/10000은 참고 정적 값이며 화면 HP/최대HP는 실제 Full/Delta를 기준으로 한다. 현재 스냅샷 타입0/1/2에 맞는 시각 표현만 매핑하고 모르는 type을 기존 타입처럼 위장하지 않는다.

좌표축은 서버 (x,y)→Unity (x,0,z). dir 이동 관례는 0=+Y,1=-Y,2=-X,3=+X(스킬 방향 불일치는 C03_CONTRACT 참조). finite 좌표, dir0..3, 유효 hp/maxHp와 실제 타입을 검증한다. Full 미수신 ID의 Delta만으로 type/maxHP/이름을 추정해 생성하지 않는다. 모르는 Delta는 버리고 다음 Full을 기다리는 최소안이 가능하다.

## 생성·사망·AOI·종료 수명

- 플레이어: `ZoneState::ImmigrateChar`에서 Zone-local 단조 증가 ID를 발급한다. 다음 Full에 등장하고 이후 변경 Delta를 받는다. `Move`의 Cell 이동은 셀 목록을 이동시킨다. 다른 수신자에게 새 Cell/AOI enter 이벤트를 보내지는 않는다.
- 몬스터: Zone 초기화 때 생성되며 고유 ID는 재생성 후에도 같다. `MoveAround/MoveToward`는 자신이 배치된 Cell 영역에 제한되고 Cell 인덱스는 유지된다. 관찰자는 본인 이동으로 AOI가 달라질 수 있다.
- 사망: `ZoneState_Skills.cpp`는 HP를 0으로 clamp하고 HP dirty를 세운다. MonsterDelta HP0을 받으면 즉시 숨길 수 있다. `FullSnapshotMonster`는 HP0 몬스터를 **생략**한다. `MonsterState.Respawn`은 HP/스폰 위치/방향을 회복하지만 자체 dirty를 세우지 않으므로 새로운 Full이 생성의 확실한 근거다. 일부 이동 Delta가 먼저 와도 다음 Full을 기다린다.
- 플레이어 HP0은 죽은 상태 값이며 엔티티 삭제는 아니다. C04가 죽음 연출을 범위에 넣지 않으면 HP0을 표시하거나 비활성 외형으로 바꾸되 탈퇴로 처리하지 않는다.
- 타인 연결 종료: `StateManager::Disconnect` → `ZoneState::EmigrateChar`가 Cell/벡터/ID 맵에서 지우고 저장/채팅 정리를 한다. 관찰자에게 삭제 packet을 쓰는 호출은 없다.
- 타인 Zone 전환: `NonZoneHandler::ZoneChange`가 원 Zone Emigrate/목적 Zone Immigrate를 수행하고 전환 **본인**에게 opcode17을 보낸다. 관찰자에게 leave 통지를 보내지 않는다. 목적지에서 ID는 재발급된다.
- `PacketTypes.h` opcode1..25와 실제 처리/writer 경로에 entity delete/despawn/AOI-leave opcode는 없다. 내부 chat ZoneLeave/Disconnect를 외부 개체 삭제 메시지로 간주하지 않는다.

`ZoneState_Snapshots.cpp`의 Cell chunk를 `BroadcastThreadPool.cpp`가 수신자의 이웃3×3 AOI로 합친다. count0이면 전송 자체를 생략한다. Full은 세계/Zone 전체 목록이 아니며, chunk 확보/용량 문제로 일부 내용이 빠질 수도 있고, 여러 broadcast worker 때문에 생성순서를 수신순서로 보장할 seq/timestamp도 없다. **한 Full에서 빠졌다는 이유로 기존 개체를 즉시 삭제하지 않는다.** 본인 이동 후 AOI가 달라지면 전의 표시를 무기한 유지해서도 안 된다.

## 새 wire 없는 최소 표시 정책

1. C02의 실제 InWorld/Zone+ownFull 이후에만 관찰을 연다. 현재 세션의 타 플레이어/몬스터 Full을 검증한 뒤 생성/갱신한다. 본인 ID 제외. 각 유효 Full 수신에 `lastFullSeen` 단조 로컬 시간을 기록한다.
2. 예를 들어 **3초 표시 유효기간**(서버 Full 명목 1초 대비 여유)을 둔다. 유효기간은 UI 정책이며 서버 despawn 판정이 아니다. Delta는 알려진 개체의 위치/dir/HP만 갱신하고 유효기간을 연장하지 않는다. Full이 오지 않으면 숨기고 해당 관찰 항목을 제거한다. 빈 Full을 기다리지 않으며, 늦은 Delta만으로 부활시키지 않는다. 다음 유효 Full이면 다시 표시한다.
3. 몬스터 HP0 Delta는 즉시 숨긴다. 이후 Delta만으로 재생성하지 않는다. 살아 있는 Full만 재표시 근거로 삼는다. 과거 Full이 뒤늦게 올 수 있으므로 사망·부활의 정확한 순서를 보증할 수 없다.
4. 본인 연결 해제/Client 교체/컴포넌트 disable/새 Zone 준비 시작 시 즉시 전체 표시·표시시간·임시 상태를 지우고 이벤트를 해제한다. 현재 C04는 Zone 전환 기능을 구현하지 않으며 향후 전환에서는 이전 Zone in-flight frame을 식별할 wire가 없다는 한계도 유지한다.
5. 장시간 통신 정지에도 표시 유효기간 만료로 영구 ghost가 남지 않는다. 다만 이탈부터 최대 유효기간 동안 낡은 표시가 남거나 지연 시 살아 있는 개체가 잠시 숨을 수 있다. staleghost가 전혀 없는 즉시정확성이 요구되면 이 정책으로 완료를 주장하지 않는다.

이 정책은 클라이언트 작업으로 가능하다. 영구 캐시/예비 풀링/범용 EntityManager는 필요 없다. Full당 부재 삭제, 알 수 없는 Delta의 외형 추정, 화면 관찰만으로 계정/접속 상태 판정은 금지한다.

## 이름 조회 24/25

서버 실제 경로: `NonZoneHandler::Process(OP::PROFILE_BATCH)` → `GetProfileBatch` → `CacheLib/ProfileCache::GetBatch` → `PacketWriter::WriteProfileBatchRes`. ProfileCache는 캐릭터 진입 `CacheLib/Handler.cpp`에서 Load, 상태 저장/연결 종료 처리에서 Unload한다. 요청되지 않은 DB 전체 검색·계정 이메일/전화/로그인 정보 반환은 없다.

- 요청 opcode24/flags0: `10+4*count`, count u16=1..50, profileIds u32 배열. 서버는 count와 필요한 최소 길이를 검사하며 잘못된 count/부족한 길이면 Disconnect한다.
- 응답 opcode25/flags0: `10+40*count`, entry는 profileId/u32,version/u32,name UTF8 byte[32]. 캐시에 있는 것만 반환하고 수용 용량에 따라 count를 줄일 수 있다. 응답에 없는 ID를 오프라인/삭제의 확정 증거로 쓰지 않는다. 누락 즉시 무한 재요청은 하지 않는다.
- 현재 ClientCore enum은23까지이고 Profile 구조/PacketBuilder/API/handler가 없다. 기존 Full의 profileId/version은 이미 받는다. 이를 지원하려면 기존 wire24/25의 최소 ClientCore 연결을 별도 배정해야 하며 **새 서버 wire를 만들 필요는 없다**. 서버 보호 영역 수정도 필요 없다. C04 최소안은 표시 이름을 생략하고 외형·상태만 보여준다.
- 이름이 요구되면 실제 Full에서 본 ID만 중복 제거하여 최대50개씩 유한 배치 요청한다. profile/version별 in-flight·완료/누락을 구별하고 누락을 매 프레임 재요청하지 않는다. 응답은 요청한 ID/version과 비교하고 UTF8/NUL 길이 검증·TMP rich text 비활성/escape 후 표시한다. 숫자 ID 스캔·이름 열거·요청 스팸·응답 원문 로그는 필요 없다.

## 최소 변경 후보·보호 영역 경계·검증

제품 수정은 미실행이다. 구현 후보는 배정된 Unity 관찰 컴포넌트/프리팹·타입0/1/2 시각 매핑, 그리고 `ClientCore/ViewModel.cs`의 MonsterFull/MonsterDelta 이벤트 lifetime 검사다. 현재 몬스터 이벤트는 dispatcher Post 전/실행 시 취소 검사가 플레이어와 달리 없으므로 새 Client/disable 때 지연 이벤트가 살아남지 않게 기존 방식과 맞춘다. 이벤트 수신자는 세션 세대도 확인한다. 현재 packet 구조/수신 루프는 그대로 재사용 가능하다.

정확한 즉시 제거가 필수면 삭제/AOIleave 또는 완결된 AOI membership snapshot과 순서/Zone세대 식별을 갖춘 서버 계약이 필요하다. 이는 새 protocol 및 broadcast/Zone 기반 구조 보호 영역에 해당하므로 Coordinator가 정확 파일·호환성·테스트/복구 범위를 별도 변경 요청으로 준비하고 사람 승인 전 수정하지 않는다. C04 사전조사는 그 변경을 요청/승인한 것이 아니며 먼저 서버를 보완하지 않는다.

후속 작업 카드에 고정할 검증 후보:

- 패킷: 플레이어43/10, 몬스터25/8과 flags/count/정확길이, internalId0 허용, 타입과 entity ID 분리, float raw/dir/HP 검증, 모르는 Delta로 생성하지 않음.
- 수명: Full 부재 즉시삭제 금지, 유효기간 만료/재등장, HP0 숨김·Delta만으로 부활 금지, Client 교체/disable 후 지연 callback 거부, 동일숫자 플레이어/몬스터 충돌 없음.
- 입력 재개 후 공통 배치: 실제 진입→본인/타인/몬스터 Full·Delta 표시 대조, 지정 두 번째 계정의 기존 정상 진입/종료를 통해 유효기간 정책 확인, 본인 이동 AOI 이탈/유입 관찰. 두 번째 클라이언트 실행 범위는 Coordinator가 작업 카드로 정하며 임의 계정/캐릭터/DB 생성은 하지 않는다.
- 실행/입력 보류 유지. 지금은 통과/캡처 결과가 없으며 문서 작성만으로 기능 완료를 표시하지 않는다. txt 근거에는 실제 수신/표시와 유효기간 숨김을 구별한다.
