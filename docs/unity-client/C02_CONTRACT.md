# C02 캐릭터 선택 → 기존 Zone 진입 계약

조사일 2026-10-18 KST. 범위는 서버 목록의 캐릭터 하나 선택→Enter→기존 Zone 입장→내 ID/좌표와 기본 도형 표시다. 서버/DB/ClientCore/제품 코드·설정·빌드·프로세스·계정은 이 조사에서 변경하거나 사용하지 않았다.

## 기준과 한계

- HEAD `1d5a3d69074a1de6bafde30f1ec4dfe5cac1a951`. 조사 시 `CoreLib/`, `CacheLib/`, `Resources/`, `Login/` 추적 diff 없음. 원본 ClientCore 7파일은 C01 미커밋 수정 상태이며 보존했다. 다른 작업자의 변경을 원복하지 않는다.
- 기존 실행 후보 `C:/Users/joeun/Desktop/workspace/Game/x64/Release/MainServer.exe` SHA256 `529D16A75BD84417ADCFC4DD632830758D6F2939362B367FF0170B41511A9BD6`를 파일에서 재확인했다. 소스/바이너리 빌드 일치는 미검증. 아래는 소스 계약이며 실행 관찰로 확인해야 한다.
- HANDOFF의 C01 두 전용 계정은 빈 목록 검증 상태. 캐릭터 생성은 계정 생성/로그인 권한과 별개다. DB 직접 조회·계정 자격 증명 사용은 하지 않았다.

## 실제 처리 순서

1. C01 `IViewDataUI.GetCharList()`의 count개 중 하나를 선택한다. 목록 외 ID, 숫자 입력 ID, 타 계정/기존 사용자 캐릭터는 사용하지 않는다. `IViewModelUI.Enter(charID)`→`NetworkService.Enter`→`PacketBuilder.CreateEnterWorldPacket`.
2. `CoreLib/NonZoneHandler.cpp:GetCharacterState`는 세션에서 userID를 얻고 channel=1/선택 charID를 내부 요청에 담는다. `CacheLib/Handler.cpp:CharacterStateRequest`→QUERY_3로 profile 조인 캐릭터 상태를 읽는다. 없음/실패는 status=0, 있으면 status=1. 현재 SQL은 charID만 조건으로 사용한다. 소유권 누락은 별도 [C02-AUTH-01/r1](SERVER_REQUESTS.md)이며 클라이언트 제약으로 해결됐다고 주장하지 않는다.
3. `CoreLib/MessageQueueHandler.cpp:CharacterStateResponse`는 성공 시 `LobbyZone.h:ImmigrateChar`로 세션/캐릭터를 로비 Zone0에 넣고 opcode6을 송신한다. 이 응답에는 charID/internalID가 없으며 아직 필드 입장 완료가 아니다.
4. 기존 `ViewModel.EnterReceived`는 status!=0일 때 좌표를 seed하고 `ZoneChange(ENTER=0)`를 자동 송신한다. Unity UI가 별도로 ENTER를 다시 보내지 않는다. 실패라면 자동 후속 요청/도형 표시를 하지 않는다.
5. `NonZoneHandler.cpp:ZoneChange`의 ENTER는 현재 Zone0인지 확인→로비에서 캐릭터 꺼냄→lastZone=0이면1→해당 기존 Zone의 `ZoneState.cpp:ImmigrateChar`→서버가 발급한 internalID 확보→chat 세션 등록→opcode17 응답. 실패는 status=0이며 나머지 필드로 성공 상태를 만들지 않는다.
6. `ViewModel.ZoneChageReceived`(기존 철자)는 성공 시 서버 좌표로 재시드하고 메인 스레드 dispatcher에 `OnZoneChageReceived(zoneID,chatID,internalID,x,y)`를 전달한다. C02 완료 상태는 이 응답과 동일 internalID의 Full 수신을 확인한 뒤 표시한다. 선택한 영속 characterID와 Zone internalID는 다른 ID다. internalID는 Zone별이며 0은 실패 sentinel, 재접속 후 값이 달라질 수 있다.

## wire format

헤더는 C01과 같은 pack=1, little endian, 8바이트(magic u16=0xABCD, total length u32, opcode u8, flags u8). 다음 offset은 패킷 시작 기준이며 float는 IEEE754 binary32다. 근거: `CoreLib/PacketTypes.h`, `PacketWriter.cpp`, `PacketWriter.h`, `BroadcastThreadPool.cpp` 및 `ClientCore/PacketHelper/PacketBuilder.cs`.

| 방향/opcode | 전체 길이/flags | 본문 절대 offset |
| --- | --- | --- |
| C→S Enter/5 | 16 / 0 | 8:characterID u64 |
| S→C Enter/6 | 76 / 0 | 8:status u8; 9:name[32]; 41:attack u16; 43:level u16; 45:exp u32; 49:hp i32; 53:mp i32; 57:maxHp i32; 61:maxMp i32; 65:dir u8; 66:startX f32; 70:startY f32; 74:currentZone u16 |
| C→S ZoneChange/16 | 9 / 0 | 8:op u8 (ENTER=0) |
| S→C ZoneChange/17 | 31 / 1 | 8:status u8; 9:zoneID u16; 11:chatID u64; 19:zoneInternalID u32; 23:startX f32; 27:startY f32 |
| S→C Full/7 | 10+43×count / 1 | 8:count u16; 10:FullStateField[count] |
| S→C Delta/8 | 10+10×count / 1 | 8:count u16; 10:DeltaUpdateField[count] |

FullStateField의 상대 offset: 0:internalID u32, 4:profileId u32, 8:profileVersion u32, 12:hp i32, 16:mp i32, 20:maxHp i32, 24:maxMp i32, 28:exp u32, 32:level u16, 34:dir u8, 35:x f32, 39:y f32. 서버에 charName[32]가 없다. 현재 ClientCore 구형 struct는 67바이트이므로 단순히 후미 이름만 무시해도 되지 않는다. 중간 profile 필드 때문에 hp 이하 offset도 전부 다르다.

DeltaUpdateField는 상대 0:internalID u32, 4:fieldID u16, 6:raw value u32(10바이트). field 0 hp,1 mp,2 maxHp,3 maxMp,4 exp,5 level,6 dir,7 x,8 y,9 profileVersion. x/y는 uint 수치 변환이 아니라 float bit 재해석한다. 정수의 짧은 필드는 서버가 남은 바이트를 0으로 채운다. C02는 내 internalID의 좌표/서버 상태만 반영하면 충분하다.

서버 Full은 `ZoneState_Snapshots.cpp`→셀별 header/chunk→`BroadcastThreadPool.cpp`가 수신자 주변 AOI 셀들의 count와 payload를 합친 단일 프레임으로 보낸다. 전체 Zone/세계 스냅샷이라고 간주해 다른 개체를 삭제하지 않는다. Full 주기는 소스상 1000ms, 일반 tick은 50ms(`Config.h`, `ZoneThreadSet.cpp`). 초기 표시를 위해 서버 응답과 Full을 짧고 한정된 timeout 안에 기다리고 늦거나 실패하면 상태를 명확히 표시한다. Full과 opcode17 도착 순서를 강제 가정하지 않고 자신을 확인할 때 사용할 최신 수신값을 최소로 보관한다.

## C02에 필요한 ClientCore 최소 호환 후보

기존 wire 의미를 변경하지 않고 현재 서버에 맞추는 Unity 최소 호환 작업이다. 적용 여부·소유권은 Coordinator가 구현자에게 배정한다.

| 파일 | 최소 필요 동작 |
| --- | --- |
| `ClientCore/PacketHelper/Packet.cs` | FullStateField를 정확한 43바이트 profileId/profileVersion 형식으로 교체. charName 제거. 가변 payload는 allocation/Marshal 전에 count×itemSize+10과 실제 frame 길이가 일치하는지 검사(정수 overflow 포함). 기존 MonsterFull/Delta/Action 형식을 새로 설계하지 않고 공통 길이 검사만 유지 |
| `ClientCore/Network/TCPSocket.cs` 또는 기존 파서 경계 | opcode6=76/flags0, opcode17=31/flags1, Full/Delta의 flags1·길이/count 검증. 잘린 패킷이 unmanaged read나 ReceiveTick으로 넘어가지 않도록 할 것. 검증 책임을 여러 계층에 불필요하게 중복하지 않음 |
| `ClientCore/Network/ReceiveTick.cs` | 가변 패킷 검증 실패가 수신 thread의 처리되지 않은 예외로 끝나지 않도록 기존 오류/연결 실패 전달 경로로 정리. 안전을 위해 서버에 맞춘 Full 구조 변경은 표시 기능 제외 여부와 관계없이 필수 |
| `ClientCore/ViewModel.cs` | Enter/Zone 응답에 기존 lifetime 취소 검사 적용, Zone 실패도 dispatcher에서 전달. Enter 성공과 Zone 성공을 구분. 종료/재시도 후 이전 응답으로 도형을 만들지 않음 |
| `ClientCore/Services/NetworkService.cs` | Enter/ZoneChange 송신 예외를 일반 로그로 삼켜 UI를 영원히 대기시키지 않음. 기존 오류 경계로 전달 |

`Handler.cs`, `Interfaces.cs`, `ViewData.cs`는 기존 이벤트/필드로 충분하면 수정하지 않는다. 정확한 필요가 드러날 때만 배정한다. 기존 `OnEnterSuccess`, `OnZoneChageReceived`, `OnZoneChageFailed`, `OnFullReceived`, `OnDeltaReceived`를 재사용한다. OnEnterSuccess만으로 필드 표시하지 않는다.

profileId/profileVersion은 Full 구조 정렬에 필수지만 **PROFILE_BATCH 24/25 요청·이름 캐시·채팅 파서 개편은 C02 필수 아님**. 내 이름은 목록/Enter 응답에서 확보 가능하다. 다른 플레이어·몬스터·전투·채팅 UI도 제외한다. 이미 수신되는 몬스터/Action 메시지는 기존 타입이 서버와 일치하며 안전한 길이 파싱을 유지하고 C02 표시에는 구독하지 않아도 된다. 사용자가 보내지 않은 채팅을 UI 성공으로 처리하거나 요청하지 않는다.

## 표시·검증 범위와 승인 분리

- 승인 대기 없이 가능한 클라이언트 작업: 목록 count행 선택/중복 진입 방지/취소·오류 상태, 기존 Enter→Zone 흐름 연결, 고정 쿼터뷰 카메라·평면·단일 도형, 서버 `(x,y)`→Unity `(x,0,z=y)` 1:1 좌표, characterID와 internalID/Zone/좌표의 구분 표시, 최소 parser fixtures·컴파일·리뷰. 입력 이동·전투를 새로 연결하지 않는다.
- 실서버 입장 검증 전 필요한 보호 운영 승인: [C02-DB-01/r1](SERVER_REQUESTS.md)의 지정 계정 캐릭터 1개씩 준비. 현재 빈 목록을 가짜 성공/유령 캐릭터로 바꾸지 않는다. 승인 전에는 fixture 결과와 실제 Zone 진입 미검증을 구분한다.
- C02-AUTH-01/r1은 발견된 기존 소유권 누락의 별도 보호 영역 변경 후보다. 서버 코드 수정/빌드/재시작을 자동 수행하지 않는다. C02에서는 목록 밖 ID나 타 계정 접속 시험을 제외한다.
- 필요한 검증: exact Full43 fixture/profile 값·x/y offset, 잘린/과대count 거부, Enter 실패/Zone 실패/종료 후 응답 안전, 실제 목록의 선택 ID→opcode6→opcode17→자신의 Full ID/좌표 일치, 동일 계정 정상 접속 종료→재로그인 후 유령 도형/이벤트 중복 없음. 수정한 경계만 검증하고 넓은 전투·채팅 회귀 체계는 만들지 않는다.
- 복구: 적용한 C02 ClientCore/DLL/UI만 기능 단위로 되돌리고 C01/다른 사용자 변경을 보존. 테스트 캐릭터 생성은 자동 되돌리기 대상이 아니며 DB 요청의 별도 복구 경계를 따른다. 이번 조사 자체에는 실행/테스트/캡처 근거가 없다.
