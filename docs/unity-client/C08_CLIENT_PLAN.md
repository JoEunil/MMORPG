# C08 클라이언트 변경 계획 — Zone 1↔2 경계 이동

## 조사 기준

현재 C07 작업 트리의 `ClientCore/ViewModel.cs`, `ViewData.cs`, `Network/Handler.cs`, `PacketHelper/Packet.cs`, `UnityClient/Assets/C02/C02WorldScreen.cs`, `C04ObservedWorld.cs`, `C05CombatScreen.cs`, `C06ChatScreen.cs`, `C07InventoryScreen.cs`를 읽었다. 제품 코드, 테스트, Unity Editor는 수정·실행하지 않았다.

## 현재 클라이언트 경로

- `ViewModel.ZoneChange(int)`는 현재 최초 ENTER와 같은 `_zonePending`만 사용하고 `op`를 byte로 바로 바꿔 전송한다. 응답은 실패·성공 모두 `_worldReady = success`로 처리한다. 이동 거절이면 기존 Zone에서 계속 플레이할 수 있어야 하므로 일반 이동 실패와 최초 진입 실패를 분리해야 한다.
- `C02WorldScreen.ZoneReceived`는 최초 진입 단계에서만 Zone 응답을 받고, `InWorld` 응답은 이동만 취소한 뒤 무시한다. `CanAct`는 `Stage == InWorld`에 묶여 있어 기존 Zone 상태를 보존하며 잠시 조작을 막는 상태를 추가하기 좋다.
- 이동은 서버 확정 위치에서 매 프레임 한 보폭 후보를 만들어 `ViewData.UpdateMove`에 저장한다. `CancelMove`는 대기 스킬이 있으면 dirty 상태를 유지하므로 경계 전환 시작 때 이동만 취소하면 대기 공격이 새 Zone으로 나갈 수 있다. 전환 때 이동과 스킬 action을 모두 취소하는 경로가 필요하다.
- C04는 `WorldStateChanged`에서 Zone/InternalID를 비교해 플레이어·몬스터 표시와 구독 세대를 초기화한다. C05도 같은 경계에서 전투 관찰과 버튼을 초기화한다. 전환 시작과 새 Zone 확정 시점의 알림을 분리해, 대기 중엔 입력을 잠그고 서버 확인 뒤 기존 관찰을 버리도록 해야 한다.
- C06 채팅, C07 가방은 `Stage == InWorld && HasOwnFull`일 때만 입력 가능하다. 가방은 숨겨도 이미 전송된 요청은 ClientCore에 pending으로 남을 수 있다. 인벤토리는 캐릭터 단위라 전환과 무관한 기존 응답을 유지하되, 전환 중 새 요청은 막고 현재 UI가 가려진 동안 응답을 유실/오표시하지 않는 처리가 필요하다. Global 채팅·응답 계약은 확대하지 않는다.

## 지형과 경계

V03 `Zone1Meadow`는 중심 (50,50), 크기 240×240m로 문서화되어 있어 X/Z 범위는 [-70,170]이다. Zone 1→2의 공통 전환대 x=95~105는 연속 지면 위에 있다. Zone 2 중심 구간 x=100~170도 바닥이 있지만, 실제 Zone 2의 오른쪽 끝 x=205보다 35m 짧다. 따라서 경계 왕복에는 충분한 바닥이 있으나 Zone 2 전체를 덮는 지형이라고 표시하면 안 된다. 런타임 씬 값은 C08 아트 작업/통합 시 Editor에서 확인해야 한다.

## 필요한 최소 변경 위치와 방향

1. **ClientCore 전환 상태**: `ViewModel`의 기존 `_zonePending`와 `_worldReady`를 재사용해 최초 진입/일반 이동 요청을 구분한다. 요청 가능 작업 상태와 실제 op 범위를 확인하고 유효하지 않은 op를 전송하지 않는다. 서버 응답 실패 시 최초 진입만 진입 실패 이벤트를 보내고, 일반 이동은 기존 월드 준비 상태를 유지한다. 성공 응답은 새 Zone·내부 ID·좌표를 Unity에 전달한다. 이 계약은 새 wire 결정에 맞춰 확정한다.
2. **C02WorldScreen 전환 흐름**: 경계 통과는 서버가 확정한 현재 좌표로만 감지한다. Zone 1에서 x≥100일 때 RIGHT(4), Zone 2에서 x<100일 때 LEFT(3)을 요청하고 요청 중 중복 전송을 막는다. 조작 단계 동안 기존 확정 상태는 보존하지만 `CanAct`를 닫고 이동/대기 스킬을 취소한다. 성공 ACK에서 Zone·InternalID·좌표를 교체하고 현재 Full을 비운 뒤 같은 새 Zone/내부 ID로 확인된 Full을 받을 때까지 대기한다. 거절 응답은 이전 Zone/좌표/HP/MP를 보존하고 조작을 다시 허용한다.
3. **관찰/액션/가방 UI**: 기존 `WorldStateChanged` 경계를 이용해 C04/C05의 Zone별 표시·전투 결과를 정리하고 전환 대기 동안 C05 스킬, C06 채팅 입력, C07 가방 열기/새로고침, 필드 이동 입력을 잠근다. 서버 응답과 예전 Zone 패킷이 구분되기 전에는 관찰 구독 세대나 로컬 캐시 초기화만으로 안전하다고 간주할 수 없다. C07의 이미 진행 중인 요청은 동일 캐릭터 응답 계약에 맞춰 유지 또는 폐기 정책을 따로 확인한다.

## 제품 코드 착수 전에 해결해야 할 계약 의존성

현재 Full/Delta/Monster Full/Delta/Action 결과에는 ZoneID나 전환 세대가 없다. Zone ACK를 받은 뒤 도착하는 옛 Zone 스냅샷을 Unity가 새 Zone 상태와 구별할 수 없다. `pendingFull.Clear()`, 화면 이벤트 generation 변경, 첫 Full 대기만으로는 옛 패킷과 새 패킷을 판별하지 못한다. 따라서 클라이언트가 정상 전환을 시작하기 전에 서버 분석에서 다음 중 하나를 증명해야 한다.

- 기존 전송 경로가 Zone 응답보다 먼저 해당 Zone의 모든 이전 broadcast/action 전송을 끝내는 순서·멤버십 보장을 갖는다는 근거, 또는
- 모든 Zone 종속 메시지에 적용할 최소 세대/Zone 식별 wire 계약과 ClientCore 파서·이벤트 변경 범위.

또한 서버 Zone 이동과 정상 연결 종료의 동시성 안전성, 실패 복원, 실제 공간 이웃/위치 검증, 재진입 시 skill slot 중복 추가도 클라이언트 단독으로 해결할 수 없다. 이 의존성이 해소되기 전에는 UI가 전환된 것처럼 보이는 후보를 만들지 않는다.

## 테스트 배치 후보 (실행하지 않음)

- 정적/ClientCore 회귀: 유효 op만 송신, 중복 입력 한 번만 전환, 일반 이동 거절 후 기존 월드 조작 지속, 최초 진입 거절 시 기존 실패 동작 유지, Full/Delta/Monster/Action의 이전 Zone 식별·폐기, 동일 내부 ID 충돌, 전환 중 이동/스킬/채팅/가방 차단, 기존 가방 응답, disconnect·복원 실패.
- 통합 입력 한 번: 정상 로그인 → 실제 이동 예산을 사용해 x=100 경계 통과 → Zone 2 ACK와 일치 Full → 재조작 → 같은 경계로 Zone 1 복귀 → 정상 종료/재접속 시 저장 좌표·HP·MP 확인.
- 기대 결과: 왕복 후 같은 캐릭터·전역 좌표 의미·HP/MP를 유지하고, 현재 Zone의 Full을 받은 뒤에만 조작 가능. 테스트는 변경 동결과 독립 리뷰 후 공통 Unity 입력 배치에서 수행한다.

## 이번 조사에서 미실행

제품 코드 변경, 서버 빌드, Unity 컴파일/Play/입력, 화면 캡처, 서비스·DB 조작은 수행하지 않았다. 이 문서는 C08 클라이언트 설계 준비만 기록한다.
