# C03 — 한 Zone 내 본인 클릭 이동 계약

조사: 2026-10-18 KST. 목표는 C02의 실제 Zone/일치하는 본인 Full 이후 클릭 지점을 향해 이동 요청하고 서버 확정 위치를 표시하는 것이다. 몬스터·전투·타 플레이어·Zone 전환·새 지형 충돌은 제외한다. 제품 코드/설정/DB/바이너리 수정·빌드·서비스 실행·Editor 조작·Unity 실행/입력 테스트를 하지 않았다. 입력 검증은 사용자 재개 뒤 배치 대상이다.

## 기준과 한계

- HEAD `1d5a3d69074a1de6bafde30f1ec4dfe5cac1a951`. 기존 C01/C02 미커밋 변경이 있는 ClientCore, CacheLib 두 파일 및 Unity 산출물을 읽었으며 보존했다. 따라서 HEAD 단독이 현재 제품 기준은 아니다.
- HANDOFF의 실행 빌드 기록과 작업 체크아웃 `x64/Release/MainServer.exe` 파일 SHA256 `C0F689BD0E58D3A20282143D5DD5C0C15514F42F192A6FF70568C2DDF312E825`가 일치한다. 프로세스·실제 이동 응답·소스 전체와 바이너리의 일치는 이 읽기 조사에서 독립 검증하지 않았다.
- 서버 동시성/프로토콜 변경은 제안 최소안에 필요 없다. 기존 broadcast의 생성 순서와 수신 순서가 같다고 보장할 근거가 없으며, 현재 wire에는 이를 복원할 번호가 없다. C03 결과를 입력별 ACK/완전한 prediction reconciliation으로 표현하지 않는다.

## 송신 경로와 실제 wire

`IViewDataUI.UpdateMove(dir,x,y)` → `ViewData`의 lock 아래 마지막 좌표/dir와 dirty 저장 → `ClientTick`의 명목 50ms 루프 → `ViewModel.SendUpdate` → `NetworkService.Action` → `PacketBuilder.CreateActionPacket` → `TCPSocket.Send`. 한 ClientTick 사이 여러 UpdateMove는 마지막 값 하나로 합쳐진다. UpdateMove 호출 자체가 즉시 송신은 아니다. dirty가 없으면 송신하지 않는다.

기존 공통 헤더는 pack=1, little endian, 총 8바이트다. magic u16=0xABCD(offset0), 전체 length u32(offset2), opcode u8(offset6), flags u8(offset7).

| 메시지 | opcode/flags | 총 길이와 본문 |
| --- | --- | --- |
| ACTION | 11 / 1 | 18바이트. offset8 dir u8, offset9 x f32, offset13 y f32, offset17 skillSlot u8 |
| 본인/플레이어 Full | 7 / 1 | `10 + count*43`. offset8 count u16, offset10부터 FullStateField |
| 플레이어 Delta | 8 / 1 | `10 + count*10`. offset8 count u16, offset10부터 DeltaUpdateField |
| ActionResult | 12 / 1 | `10 + count*21`. offset8 count u16, offset10부터 ActionResultField |

근거: `CoreLib/PacketTypes.h:ActionRequestBody/FullStateField/DeltaUpdateField/ActionResultField`, `PacketWriter.cpp`, `PacketWriter.h`, `ClientCore/PacketHelper/Packet.cs`, `PacketBuilder.cs`. 현재 ClientCore의 위 구조는 서버와 일치한다. C02에서 Full의 구형 이름 배열은 profileId/profileVersion으로 이미 교정했다. 별도 UnityScripts의 구형 입력/표시를 복사하지 않는다.

- ACTION에는 characterID/zoneID/seq/client tick/timestamp가 없다. 서버가 TCP 세션과 현재 Zone으로 본인을 찾는다. `NetLibrary/ClientContext.cpp`의 `SetSeq(m_seq++)`는 서버 내부 PacketView 수명/처리용이며 wire ACK가 아니다.
- 이동 전용은 반드시 skillSlot=255(NONE_SKILL). 0은 실제 스킬 슬롯이다. `ViewData.GetActionState`가 읽을 때마다 다음 슬롯을 255로 초기화하나, `_actionState`의 최초 struct 기본값은 0이므로 최초 tick과 경쟁하지 않도록 생성 시 255 초기화가 최소 안전 수정 후보다.
- 좌표는 절대 위치이며 목적지/속도 명령이 아니다. 클릭 목적지 전체를 단 한 ACTION 좌표로 보내지 않는다. 서버 `(x,y)`는 Unity `(x,0,z)`에 각각 대응한다. 1단위=Unity 수평면 1단위. raycast는 승인된 열린 지면만 대상으로 하고 UI 위 클릭은 제외한다.

## 서버 처리·속도·경계

`NetLibrary/ClientContext::DequeueRecvQ`의 프레임 검사 → `NetPacketFilter::TryDispatch`의 세션 존재/인증/cheat 상태 검사 → `CoreLib/PacketDispatcher::Process`의 flags=1/current Zone 검사 → `ZoneThreadSet::EnqueueWork` → `ZoneHandler::RunTick/ProcessAction` → `ZoneState::Move`, 이어서 `Skill`, `DirtyCheck`다. 이동을 거부했어도 별도 Move 결과 ACK는 없고 Skill 호출과 DirtyCheck는 이어진다. C03은 255로 스킬을 끈다.

`ZoneHandler.h:RunTick`은 cooldown → 이동 예산 보충 → 최대 2000개 큐 처리 → 스킬 적용/몬스터/cheat flush → 스냅샷 순서다. `ZoneThreadSet.cpp`는 GAME_TICK=50ms, Delta=50ms, Full=1000ms 기준으로 동작한다. 실제 지연 시 밀린 tick을 보정하며 고정 실시간 20회 전달을 보증하지 않는다.

`ZoneState_Actions.cpp:Move`, `CoreLib/Config.h`, `CharacterState.h`:

1. 세션 캐릭터가 없으면 무시. dir>3 또는 NaN/Infinity 좌표면 cheat 목록에 1 추가하고 위치/방향 갱신 전 반환.
2. 유효 dir 변화는 좌표와 독립적으로 먼저 반영한다. 정지 상태의 방향 변경도 가능하다.
3. x/y를 현재 Zone 영역의 양끝 포함 범위로 각각 clamp한다. 경계 밖 요청 자체를 cheat로 보지 않는다.
4. clamp 후 현재 위치와 Euclidean 거리를 계산한다. 제자리면 반환. 매 tick 예산 +1, 이월 cap=3, 비교 여유 epsilon=0.001. 기본 예산은 0이다. 명목 지속 속도 상한은 1/0.05=20단위/초이며 캐릭터별 speed 필드가 내려오는 것은 아니다.
5. 거리>현재 예산+epsilon이면 그 방향으로 예산만큼 줄여 이동한다. 원래 clamp 후 거리 제곱>9이면 cheat 1을 추가한다. 예산을 소비하고 음수는 0으로 만든다. 요청 여러 개가 같은 tick에 들어와도 같은 예산을 공유한다. epsilon은 클라이언트 추가 이동 권한으로 사용하지 않는다.
6. 실제 변경 x/y에 dirtyBit 0x80/0x100, 방향에 0x40을 세우고 Cell을 갱신한다. 서버 지형/벽/고저차/NavMesh 충돌 판정은 이 경로에 없다.

`CoreLib/Cell.h:ZoneInit`: 열=(zone-1)%3, 행=(zone-1)/3, 각 축 `[index*100-5,(index+1)*100+5]`. Zone1은 x/y 각각 [-5,105]. 실제 활성 Zone 수는 4다. C03은 현재 Zone 범위 안 목표로 제한하고 자동 ZoneChange를 보내지 않는다. 클릭 표면 장식이 이동 불가를 뜻한다고 가정하지 않는다. 경계는 서버 규칙을 복제한 클라이언트 입력 제한이며 새 서버 판정이 아니다.

방향 주의: 좌/우 2/3은 -X/+X로 일치한다. `ZoneState_Monsters.cpp`와 구형 `MyHeroControl.cs`의 이동 관례는 0=+Y,1=-Y이나 `SkillData_Range.cpp` Rectangle 판정은 0=-Y,1=+Y다. `Move`는 dir과 좌표 방향의 일치를 검증하지 않는다. **C03 시각 facing은 이동 관례 0=+Y,1=-Y,2=-X,3=+X**로 명시하고 대각선은 주축으로 선택한다(동률 규칙 고정). 스킬 방향 통일/서버 수정은 이번 범위 밖이며 전투 착수 때 별도 계약으로 처리한다.

## 확정 상태 수신·시간·스레드

- FullStateField 43바이트: zoneInternalID u32(0), profileId u32(4), profileVersion u32(8), hp/mp/maxHp/maxMp i32(12/16/20/24), exp u32(28), level u16(32), dir u8(34), x/y f32(35/39).
- DeltaUpdateField 10바이트: internalID u32(0), fieldID u16(4), fieldVal u32(6). field0..9=hp,mp,maxHp,maxMp,exp,level,dir,x,y,profileVersion. x/y의 uint는 수치 cast가 아니라 IEEE754 비트 재해석. dir은 writer가 1바이트를 쓰고 나머지를 0으로 채운다.
- `ZoneState_Snapshots.cpp`는 변경 필드만 Delta에 쓰고 dirty를 지운다. Full은 AOI Cell chunk이며 대상의 부재는 삭제/이동 실패가 아니다. `BroadcastThreadPool.cpp`는 이웃 3×3 Cell을 합쳐 count/길이를 설정하고 count=0이면 보내지 않는다. 본인은 자신의 현재 Cell AOI에 포함된다. 표시 대상으로 C02 `InternalID`와 일치하는 필드만 사용한다.
- Full/Delta/ActionResult에 Zone ID·snapshot tick·timestamp·입력 ACK 번호는 없다. 여러 broadcast worker가 각 작업을 처리하므로 생성 순서 복원/오래된 Full 검출을 보장할 수 없다. 클라이언트는 실제 수신한 스트림 순서로 반영하고 다음 Full로 수렴을 관찰한다. 새 sequence/tick 추가나 broadcast 직렬화는 보호 영역/새 wire 승인 대상이며 C03 최소안에는 포함하지 않는다.
- ActionResult는 casterType/casterId/skillSlot/dir/시전 x/y/skillId/skillPhase의 **스킬 연출 결과**다. 이동 성공 ACK·이동 위치 보정 근거로 사용하지 않는다.
- PING22/PONG23의 serverTimeMs는 별도 heartbeat/RTT용이다. `ViewModel.PingReceived`는 RTT 이벤트 후 timestamp를 그대로 Pong으로 돌려주며 Full/Delta에 시간을 붙이지 않는다. `GetServerTimeMs`가 자동 서버 시계로 업데이트된다고 가정하지 않는다.
- `Handler.HandleResponse`가 flags=1 프레임을 `ReceiveTick` 큐로 넘긴다. `ReceiveTick` 50ms 작업 스레드에서 파싱하고 `ViewModel.FullReceived/DeltaReceived`가 dispatcher로 Post한다. C01LoginScreen.Update의 jobs 큐에서 Unity 메인 스레드로 실행한다. C02는 이미 이를 구독/해제하고 lifetime 취소를 확인한다. `DeserializeVariablePacket`은 count 할당 전 정확한 `10+count*itemSize == buffer.Length`를 검사한다.
- 기존 C02는 Full/Delta에서 x/y를 확정 표시하지만 dir은 Delta에서 아직 반영하지 않는다. `ShowPosition`은 본인 Transform·바닥·카메라를 재배치한다. C03은 시각 위치/카메라/필드 역할을 한 소유자로 유지하고 C02/C03 컴포넌트 둘이 본인 Transform을 덮어쓰지 않도록 해야 한다.
- C02 `DeltaReceived`는 같은 packet 안의 x/y를 모두 순회한 뒤 ShowPosition을 한 번 호출한다. 이 성질을 유지해 x만 반영한 중간 좌표를 화면이나 새 송신 기준으로 사용하지 않는다. 서로 다른 packet으로 온 축에는 공통 tick을 복원할 방법이 없다. field6의 raw uint가 0..3인지 확인한 뒤 dir을 같은 확정 상태에 합친다. Full에서도 dir 검증·회전을 적용한다. ClientCore ViewData.SetPosition은 진입/Zone에서만 호출되므로 이를 최신 수신 위치 저장소로 오인하지 않고 C02가 보유한 최신 본인 확정 상태에서 요청 후보를 계산한다.

## 권장 최소 구현과 수정 범위

서버 수정 없이 가능하다. 처음에는 **서버 확정 위치 표시**를 유지하고 클릭 목적지만 로컬 입력 상태로 둔다. 실제 Zone/ownFull/InWorld·연결 유효 조건에서만 이동한다. 최신 확정 위치에서 목표 방향으로 최대 1단위의 후보를 명목 50ms보다 자주 만들지 않고 기존 UpdateMove에 전달한다. 요청이 늦게 도착해도 목적지까지 긴 도약을 누적하지 않는다. dt 급증 시 여러 catch-up 송신/장거리 후보를 만들지 않는다. 동일 확정 위치에서 중복 후보를 보내는 것은 추가 거리 이동이 아니며, 이 방식은 지연만큼 느려질 수 있다. 부드러운 보간을 추가하면 화면의 보간 위치와 서버 확정 HUD를 구분한다.

목표 도착/클릭 교체/연결 해제/컴포넌트 비활성/새 Client/Zone 결과 때 목표와 송신 대기를 지운다. 응답이 없거나 유효 본인 좌표 갱신이 일정 시간 멈추면 유한한 타임아웃으로 요청을 중지해 무한 송신하지 않는다. UI/잘못된 지면/NaN/Infinity 입력은 요청 전에 거부한다. Full/Delta의 dir·위치·현재 Zone 범위를 확인하고 잘못된 값은 본인 이동을 중단하여 명확한 실패로 전달한다. 다른 ID의 잘못된 값 때문에 본인 위치를 오염시키지 않는다.

최소 후보(구현자는 Coordinator가 별도 배정):

- `UnityClient/Assets/C02/C02WorldScreen.cs` 또는 한 개 배정된 이동 컴포넌트: 목표/지면 클릭/UI 배제/50ms 제한/한 본인 표시 소유자/dir Delta/유한성·종료 정리. 실제 필요 참조만 연결한다.
- `ClientCore/ViewData.cs`: 최초 waitSkillSlot=255, UpdateMove의 dir/유한 좌표 경계 검증. 기존 lock·마지막 값 합치기는 유지한다.
- `ClientCore/ViewModel.cs`: 취소 뒤 SendUpdate 중지와 실제 ACTION 송신 실패의 사용자 연결 실패 전달이 필요할 경우 해당 경계만.
- `ClientCore/Services/NetworkService.cs`: 현재 Action이 오류를 로그로 삼키므로 C03에서 송신 실패를 기존 실패 경계로 전달하는 최소 변경. 변경 시 ClientTick에서 미처리 예외로 스레드를 죽이지 않도록 ViewModel과 함께 다룬다.
- 기존 Full/Delta/Action 구조·PacketBuilder·ReceiveTick·새 인터페이스는 현재 wire를 위해 수정할 이유가 없다. DLL은 원본 ClientCore에서 한 번 빌드해 갱신하고 출처를 기록한다.

입력 중단 전에 이미 dirty로 들어간 최대 한 개 segment는 다음 ClientTick에서 송신될 수 있다. 목적지/비활성 상태에서 이전 입력이 남아선 안 되는 동작을 요구하면 ViewData 안에서 명시적으로 pending 이동을 지우는 최소 수명 처리를 함께 검토한다. 새 범용 큐/Manager나 패킷 취소 프로토콜을 만들지 않는다. 연결 해제 시 기존 Initializer.Dispose의 socket/tick 정리를 사용한다.

보호 영역 요청 없음. 서버 속도/예산·Zone 경계·방향 정책을 바꾸거나 seq/timestamp/개별 ACK/충돌·소유권/큐/락을 추가해야 한다면 해당 추가분만 별도 변경 요청으로 올린다. 클라이언트 안전 검증·현재 규칙에 맞춘 입력/표시는 승인된 최소 호환 범위로 진행 가능하다.

## 검증 후보와 입력 배치

이 문서는 검증 계획이며 통과 기록이 아니다. 현재 사용자 게이트에 따라 Unity Play·runtime fixture 주입·마우스/키 입력·실행 캡처는 모두 미실행이다. Edit-mode 컴파일/정적 검토와 실행 게이트에 저촉되지 않는 좁은 순수 패킷 검증만 Coordinator가 작업 카드에 비용/종료 조건을 확정한 뒤 수행한다.

1. 순수 패킷 회귀: ACTION18/flags1/skill255·정확 offset, Full43/Delta10/Action21, x/y float 비트, 잘린/과장 count·길이 거부. 최초 UpdateMove 전후 skill255 유지와 같은 tick 입력 마지막 값 확인.
2. 유한성·종료: NaN/±Infinity/dir4 차단, 긴 프레임 뒤 후보거리≤1·송신 간격≥50ms, UI/비지면 클릭 무시, 같은 위치 목표/도착/새 클릭·타임아웃·disable/reconnect 목표 정리. 방향 4종과 대각선 동률 규칙.
3. 입력 배치: 최종 검토 버전 동결 → 실제 로그인/선택/진입 → Zone/ownFull 대기 → 가까운 점·먼 점·대각선·목표 교체 클릭 → 본인 Full/Delta·HUD·모델 좌표 대응 관찰 → UI 클릭 시 이동 없음 → 경계 안에서 정지 → disconnect/재진입 후 이전 목표 자동 송신 없음. 입력 전/대기 중 클릭도 이동하지 않아야 한다.
4. 필요 근거만 txt/비밀 없는 PNG 및 실제 이동 GIF에 남긴다. 서버 확정/시각 보간/클릭 목표 좌표를 구분한다. 송신/수신 횟수·최대 후보거리·비정상값 차단은 테스트 근거로 기록하되 제품 HUD에 구현 상세를 상시 노출할 필요는 없다. 새 빌드 AUTH 회귀도 같은 로그인 흐름에 합친다.

입력 배치 시간 상한과 종료 조건은 Coordinator 작업 카드에서 정한다. 실패 시 무한 재시도하지 않고 동일 원인 수정 2회 뒤 근거를 인계한다. 서버 생성 순서를 복원할 수 없는 제한은 통과 결과에 남긴다.
