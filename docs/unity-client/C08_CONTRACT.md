# C08 기존 Zone 경계 이동 — 선행 계약 조사

2026-10-19 서버 분석 담당. BRIEF의 기존 Zone 간 이동 흐름을 위한 읽기 분석이다. 조사 HEAD `7c83226`; 현재 C07의 LobbyZone 캐릭터 귀속·인벤토리 초기화 후보와 ClientCore 미커밋 변경을 읽기 기준으로 보존했다. C07 서버 빌드/실환경 배치 결과를 이 문서에서 추정하지 않는다. 제품·테스트·Editor·서비스·DB 조작, 승인 요청 없음. 소유 파일은 이 문서뿐이다.

## 추천 다음 단위

**현재 Zone 1에서 실제 공통 경계까지 이동한 뒤 Zone 2로 전환하고, 같은 위치·HP·MP·캐릭터로 새 Full을 받은 후 조작을 재개하는 왕복 흐름**부터 구현한다. 포탈·텔레포트·신규 Zone·지형 충돌·자동 경로 탐색은 추가하지 않는다. 현재 지면이 경계까지 이어지는지는 아트/구현 담당이 별도로 확인하고, 없는 지면은 기존 세계 좌표의 단순 연결 표현으로 보완한다. 임의로 캐릭터 좌표를 옮겨 성공시키지 않는다.

서버는 이미 방향 전환 메시지를 받지만 공간·수신 순서의 안전성이 완성됐다는 뜻은 아니다. 아래 B/C는 작업 카드와 리뷰에서 먼저 좁혀야 하는 보호 코드 후보다. 이번 문서는 복잡한 락/전송 순서 해법까지 확정하지 않는다. 그런 분석이 실제 착수 범위가 되면 Coordinator에게 high 분석 상향을 요청한다.

## wire

`CoreLib/PacketTypes.h`, `PacketWriter.cpp`, `ClientCore/PacketHelper/PacketBuilder.cs` 기준 packed little endian.

| 메시지 | 내용 |
| --- | --- |
| 요청 | 이름은 ZONE_CHANGE, opcode16, flags0, 전체9바이트(공통헤더8+op uint8) |
| op | 0 ENTER(로비→저장 Zone), 1 UP(+Y), 2 DOWN(-Y), 3 LEFT(-X), 4 RIGHT(+X) |
| 응답 | opcode17, flags의 SIMULATION 비트1, 전체31바이트 고정 |
| 응답 body | status uint8, zoneID uint16, chatID uint64, zoneInternalID uint32, startX/startY float32 |

성공은 status1. 최초 ENTER는 AddChatSession이 발급한 chatID, 일반 Zone 이동은 chatID=0을 보낸다. 따라서 이동 성공의 0을 채팅 ID 재설정/로그아웃으로 해석하지 않는다. 현재 C02WorldScreen은 chat 값을 저장하지 않는다. 실패 writer는 status0 외 payload를 초기화하지 않으므로 실패 body의 zone/좌표/ID를 소비하지 않아야 한다. 성공 writer의 `header.flags |= 0x00`는 초기화가 아니다. C08 후보에서 flags를 명시해 payload/header 잔여 값을 제거하는 작은 보완이 적절하다.

## 실제 지도와 이웃

`CoreLib/Config.h::ZONE_COUNT=4`; `Cell.h::ZONE_HORIZON=3`, ZONE_SIZE=100, TRANSITION_BUFFER=5. StateManager는 실제로 1~4만 생성한다. 3×3 상수 주석만 보고 Zone5~9를 제공하면 안 된다.

| Zone | core 영역 | 서버 이동 허용 영역(버퍼 포함) | 실제 공간상 이웃 |
| --- | --- | --- | --- |
| 1 | x0~100,y0~100 | x-5~105,y-5~105 | RIGHT2, UP4 |
| 2 | x100~200,y0~100 | x95~205,y-5~105 | LEFT1, RIGHT3 |
| 3 | x200~300,y0~100 | x195~305,y-5~105 | LEFT2 |
| 4 | x0~100,y100~200 | x-5~105,y95~205 | DOWN1 |

좌표는 전역 좌표이며 전환 때 원점/100단위 오프셋을 더하지 않는다. 현재 NonZoneHandler는 UP=zone+3, DOWN=zone-3, LEFT=zone-1, RIGHT=zone+1로 계산하고 결과1~4 여부만 검사한다. 그 결과 **Zone3 RIGHT→4 및 Zone4 LEFT→3**을 허용하지만 공간상 이웃이 아니다. 현재 클라이언트 InsideZone은 위 4개 영역과 일치한다.

Zone1↔2 공통 버퍼는 x95~105,y-5~105. 구현안으로 서버 확정 위치가 x100을 넘었을 때 RIGHT를 요청하고 되돌아올 때 x100 미만에서 LEFT를 요청할 수 있다. 정확한 전환선/재요청 방지는 작업 카드에서 하나로 정한다. 자동전환이면 복귀 직후 반복 왕복하지 않도록 목표 방향/경계 통과 조건을 사용한다. 새로운 서버 물리 경계를 만든 것으로 표현하지 않는다.

## 실제 이동 처리

`NonZoneHandler::ZoneChange` → 현 Zone EmigrateChar → 대상 Zone ImmigrateChar → 성공 응답 → ChatThreadPool.EnqueueZoneJoin. 실패하면 원 Zone ImmigrateChar로 복원하고, 복원도 실패하면 Disconnect한다. 상태의 x/y/HP/MP/캐릭터 ID는 복사하며 대상 Zone ImmigrateChar가 lastZone/zoneInternalID와 세션 ZoneID를 변경한다. 내부 ID는 Zone별 단조 증가이므로 Zone이 다르면 같은 숫자가 존재할 수 있다. ChatThreadPool::ProcessZoneJoin은 기존 Zone membership을 제거하고 새 Zone으로 옮기므로 별도 Leave 호출 누락이라고 단정하지 않는다.

`ZoneState::Move`는 현 Zone 영역으로 clamp하고 이동 예산을 적용한다. 별도 자동 Zone 전환은 하지 않는다. Zone 이동 요청에는 좌표가 없으며 현 캐릭터 상태를 읽어야 한다. 현재 `ImmigrateChar`는 GetCell에서 clamp된 cell만 얻고 state.x/y가 대상 영역 안인지 검사하거나 변환하지 않는다.

## 빠진 클라이언트 연결

- ViewModel.ZoneChange(op) / NetworkService.ZoneChange / PacketBuilder 경로는 존재하며 최초 ENTER에서 op0을 호출한다. 일반 이동용 op1~4 범위·worldReady·connectionFailed 검사가 부족하고 byte cast로 임의 int가 들어갈 수 있다.
- C02WorldScreen.ZoneReceived는 EntryPending일 때만 처리한다. InWorld의 응답은 CancelMovement 후 무시한다. 별도의 이동 요청 상태가 필요하다.
- 요청 시 이동/스킬을 멈추고 기존 화면의 확정 상태를 보존한 채 전환 중 표시. 서버 성공 후 새 ZoneID/internalID/position을 적용하고 관찰 개체/전투 효과/옛 pending Full을 비운다. 해당 ID의 새 Full을 확인하기 전 CanAct를 열지 않는다. 캐릭터 선택으로 돌아가거나 Client를 재생성할 필요는 없다.
- C04ObservedWorld/C05CombatScreen은 WorldStateChanged/ZoneID/internalID 변경을 보고 관찰을 초기화하는 경계가 있다. 이 경계를 재사용하되 Zone별 ID 충돌 회귀가 필요하다.
- ViewModel.ZoneChageReceived는 성공 여부를 `_worldReady=success`로 덮는다. 일반 이동 거절 후 기존 Zone이 유지되는 경우 최초 진입 실패와 구분해야 한다. 실패의 잔여 payload를 읽지 않고 원 상태를 보존하는 정책을 정한다.
- C07 인벤토리는 동일 캐릭터 데이터이므로 C08에서 DB 행 생성/빈 행 정정이나 아이템 정책 변경을 하지 않는다. 전환 중 요청 차단과 이미 보낸 응답 처리만 현 수명 규칙 안에서 확인한다. C06 Global 채팅을 Zone 전환 명목으로 새 기능으로 확대하지 않는다.

## 서버 안전성 후보와 남은 불확실성

**A. 이웃/위치 검증.** NonZoneHandler는 행 경계 이웃이나 현재 위치를 검사하지 않고 캐릭터를 이동시킨다. 최소 후보는 현 Zone과 op의 실제 공간 이웃 검증, 대상 Zone 존재, 캐릭터의 유한 좌표와 공통 영역 포함 검증을 **원 Zone에서 제거하기 전** 적용하는 것이다. 클라이언트만 검사하면 서버 권한 검증을 대신하지 못한다. 이 검증을 위해 상태를 안전하게 읽는 최소 경계를 구현 담당/서버 분석 후 확정한다. 임의 좌표 수정·새 DB 계약은 필요하지 않다.

**B. 전환·종료의 락/원자성.** 현 전환은 NonZone worker에서 source 제거와 destination 삽입이 분리돼 있고, ZoneState::ImmigrateChar는 zone mutex를 보유한 채 StateManager.SetZoneID의 shard mutex를 얻는다. 반면 StateManager.Disconnect는 shard mutex를 보유한 채 EmigrateChar의 zone mutex를 얻는다. 정적 코드상 역순 락 위험이 있으므로 정상 왕복 성공 하나로 안전성을 보장하면 안 된다. 기존 R01/C07의 shard→lobby 보호가 필드 Zone까지 자동 확장되는 것은 아니다. 새 매니저/큐 프레임워크 추가보다 전환/종료의 좁은 공통 락 순서·기존 세션 검사·실패 복원 원자 경계부터 독립 분석해야 한다.

**C. 옛 Zone 스냅샷의 구분.** 현재 Full/Delta/Monster/Action wire에는 Zone ID/전환 세대가 없다. BroadcastThreadPool은 work의 내부 ZoneID로 Zone의 snapshot reader를 얻고 해당 세션 목록에 전송한다. 이 목록은 전환 시점의 실제 membership 재검증이 아니며 여러 broadcast worker와 response 전송이 독립이다. 전환 성공 응답 뒤 옛 Zone 데이터가 전송되지 않는다는 순서 보장을 찾지 못했다. 단순 Client epoch나 pendingFull.Clear만으로, ack 뒤 도착한 옛 Zone의 같은 내부 ID를 새 개체와 구분할 수 없다. 시간 지연 후 수신 재개도 증명이 아니다. C08 안전성 분석에서 전송 순서/멤버십을 기존 wire 안에서 보장할 수 있는지 먼저 확인하고, 불가능하면 최소 wire 변경의 호환성·회귀 범위를 구체화해야 한다. 이 문서는 새 프로토콜을 확정하지 않는다.

**D. 기타 보존 주의.** ImmigrateChar는 기존 캐릭터의 skillSlot vector에 3개를 다시 append한다. 일반 이동/복구 왕복에서 반복 증가할 수 있다. 첫 3개 쿨다운/MP를 보존하고 불필요한 append를 막는 최소 후보를 검토하되 전투 밸런스 변경으로 확대하지 않는다. 진행 중 스킬/dirty/AOI 참조의 정리는 실제 전환 회귀 범위에 넣는다.

## 작업 카드에 넣을 완료 조건

1. 구현·리뷰 후 한 배치: 정상 로그인→서버 이동 예산에 맞춰 1/2 공통 경계까지 실제 이동→Zone2 응답/일치 Full→새 조작→Zone1 복귀→정상 종료. 좌표·캐릭터·HP/MP·Zone 변경과 두 번 클릭/연속 입력에 단일 전환만 발생함을 기록한다.
2. 실패는 원 Zone/확정 위치·HP/MP 유지, 전환중 이동/공격·채팅 입력의 키 충돌 없음. 패널 UI 클릭이 이동을 만들지 않음. 존재하지 않는 이웃/멀리 떨어진 위치 요청은 서버에서 거절.
3. 종료/거절/실패 복구/중복/old-ID 충돌·옛 Full/Delta/Action 주입 회귀를 완료하고, 전송 순서 보장이 증명되지 않으면 안전성 미해결로 남긴다. 기대를 완화해 통과시키지 않는다.
4. 정상 퇴장 후 재접속의 첫 Full에서 저장 Zone/위치/HP/MP 확인. 읽기 전용 관찰을 사용할 수 있으나 테스트용 직접 DB 수정은 하지 않는다. 원형 맵 전체/4개 Zone 전부 검증한 것으로 보고하지 않는다.

최신 정책에 따라 목표 안 후보 구현·로컬 검증은 별도 단계 승인 없이 진행하며, 중요한 서버/ClientCore 후보가 준비되면 최종 push/PR 직전에 Coordinator가 한 번 Slack 승인을 요청한다. 본 분석은 그 승인이나 구현 완료를 의미하지 않는다.
