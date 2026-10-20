# 스킬 재사용 대기 표시 계약 분석

조회 대상 절대 루트: `C:/Users/joeun/Desktop/workspace/Game-UnityClient/UnityClient/Builds/C10-Delivery/source`.

## 결론

현재 wire로 **서버가 판단하는 정확한 남은 재사용 시간은 표시할 수 없다.** 서버는 캐릭터의 슬롯별 `skillCoolDownTick`을 보유하지만 `FullStateField`와 `ActionResultField` 어느 쪽도 그 값 또는 남은 tick을 보내지 않는다. 서버 프로토콜 변경 없이 가능한 최소 후보는 **서버가 확정한 cast 이벤트를 받은 뒤 기존 `C14ServerContent`의 공개 cooldownTicks로 client-side 예상 타이머를 그리는 표시**다. 이것은 예상 UI일 뿐 authoritative ready 상태가 아니다. 입력을 잠그거나 서버 결과를 대신해서는 안 된다.

## 서버·클라이언트 경로

- `CoreLib/CharacterState.h`의 `SkillSlotEntry`에는 `skillID`와 `skillCoolDownTick`이 있다. `CoreLib/ZoneState.cpp`의 `ImmigrateCharLocked`는 빈 슬롯을 `{0,0}, {1,0}, {2,0}`로 초기화한다.
- `CoreLib/ZoneState_Actions.cpp`의 `ZoneState::Skill`은 slot·skill ID를 검증한 뒤 cooldown이 0이고 MP가 충분할 때만 MP를 차감하고 `skillCoolDownTick = skillInfo.coolDown`을 설정한다. cooldown 또는 MP 부족으로 거부된 요청은 별도 거부 packet을 만들지 않는다.
- `CoreLib/ZoneState_Skills.cpp`의 `SkillCoolDown`은 매 Zone tick에 양수 cooldown을 1 감소시킨다. `CoreLib/Config.h`는 `GAME_TICK=50ms`; `CoreLib/ZoneHandler.h`의 `RunTick`이 tick마다 감소 → 요청 처리 순서로 실행한다. 따라서 `Skills.inc`의 20/120/80 ticks는 서버 기준 nominal 1/6/4초다. 이는 클라이언트가 packet을 받은 시점부터의 정확한 남은 시간과 같지 않다.
- 승인된 스킬은 server `Cell::activeSkills`에 추가되고 `CoreLib/ZoneState_Skills.cpp`가 phase별 `ActionResult`를 기록한다. phase 0 event는 accepted cast의 server-side 관찰 신호다. `PacketWriter.cpp`가 caster type/ID, slot, skill ID, phase, 방향과 시전 위치만 serialize한다. 결과 packet에는 remaining cooldown이나 server timestamp가 없다.
- ClientCore `ViewModel.ActionResultReceived`는 `AcceptSimulation(zoneID,generation)`과 `_worldReady`를 확인한 뒤 UI로 이벤트를 전파한다. `ClientCore/PacketHelper/Packet.cs`의 `ActionResultField` 필드도 cooldown이 없다. `FullStateField`는 HP/MP/max/EXP/level/direction/position이며 skill slots를 포함하지 않는다.
- `UnityClient/Assets/C02/Content/C14ServerContent.asset`은 기존 스킬 0/1/2의 cooldownTicks를 20/120/80으로 갖고 있으며 `C14ServerContentExport`가 이 데이터를 `UnityClient/Exports/ServerData/Skills.inc`로 export한다. 현재 생성된 `Skills.inc` 값은 asset과 일치한다. 기존 생성 파일이 stale이거나 server가 다른 export로 빌드됐다면 client 예상값도 다를 수 있으므로, 이번 HUD는 이 동일 exporter/content 소스를 신뢰해야 하며 수치를 별도 코드에 복사하지 않는다.
- `C05CombatScreen`은 현재 action result에서 내 caster, slot/skill ID 관계 및 phase 0/1을 확인하고 `cast`/`released` 문구만 표시한다. local 입력은 즉시 `requested` 문구를 쓰지만 cooldown confirmation으로 취급할 수 없다.

## 표시 가능한 의미와 한계

phase 0 action result를 받은 순간부터 C14 asset의 cooldownTicks×50ms로 남은 시간을 추정하는 ring/bar 또는 초 단위 label은 가능하다. 승인 이벤트가 오기 전에는 타이머를 시작하지 않는다. phase 1은 적중/시전 후속 단계이므로 타이머를 다시 시작하지 않는다. 표시는 `예상 재사용 대기`처럼 클라이언트 추정임을 명시한다.

정확성이 불가능한 이유는 server cooldown이 요청을 처리할 때 시작하지만 클라이언트는 그 뒤 phase 0 event를 네트워크 지연과 snapshot 전달 이후 받기 때문이다. event에 server tick/time 또는 remainingTicks가 없고, 거부 요청도 응답하지 않는다. 그러므로 local countdown은 server보다 늦게 끝나 ready 상태를 과대 추정할 수 있다. button을 countdown 동안 비활성화하거나 local timer로 재사용 가능을 판정하지 말고, 모든 요청을 server로 보내 server가 허용/거부하게 둔다.

## 최소 후보 1개

프로토콜 변경 없이 가능한 후보는 C05의 **confirmed-cast 기반 추정 cooldown 표시**다. C05에는 현재 C14 참조가 없으므로 이를 구현하려면 기존 C14 content asset을 명시 연결해 slot 0/1/2의 cooldownTicks를 읽어야 한다. 본인 `casterType==0`, 현재 `InternalID`, 현재 Zone/generation에 해당하는 `skillPhase==0` event만 사용한다. C02/C05에 새 manager, 임의 cooldown 상수, server-ready 판정은 넣지 않는다. 이번 생존 이동 묶음에는 구현하지 않았으며 본 문서는 test 결과가 아니다.

## 수명·경계 처리

- Zone 이동 때 server `StateManager::ChangeZone`은 현재 `CharacterState`를 destination으로 복사하므로, `skillSlot` cooldown은 이어진다. UI도 같은 client/selected character의 예상 timer를 Zone 이동만으로 초기화하면 server와 다르게 보일 수 있다. 반대로 새 Zone의 `zoneInternalID`와 generation은 변하므로 이전 Zone의 늦은 event는 timer를 갱신하면 안 된다. ClientCore의 simulation-generation guard와 현재 own ID 검사에 의존한다.
- 새 client/account/character 연결, world exit, screen disable에서는 C05의 기존 generation/model/lifecycle 해제를 따른다. 예상 timer와 표시를 비운다. 새로운 연결로 캐릭터를 다시 읽으면 DB/FullState에 cooldown 값이 없고 새 슬롯이 0에서 초기화되므로, 재접속 후에는 서버가 보내는 새 confirmed cast 이전에 cooldown을 추정할 수 없다.
- `C05CombatScreen.WorldChanged`는 HP 0에도 InWorld 패널을 남기고 상태를 보여주지만 `InputAvailable`과 `ApplyResults`는 HP 0에서 동작을 막는다. 예상 표시를 유지하더라도 요청 가능 상태로 보이게 하지 않는다. logout/disable 및 panel hide에서 기존 표시를 정리한다.
- UI 버튼이나 Z/X/C 입력을 countdown 값으로 차단하지 않는다. request를 보냈다는 이유만으로 timer를 시작하지 않고, delayed/no result는 서버 확인 전 상태로 남겨 둔다.

## 최소 검증 후보

자동 검증은 기존 C05 관련 시나리오에 직접 영향 assertion만 통합한다.

1. 자신의 phase 0만 해당 slot의 추정을 시작하고 phase 1/다른 caster/잘못된 slot/늦은 이전 generation은 timer를 시작·재설정하지 않는다. 현재 slot 예상값이 20/120/80 ticks 및 50ms 해석을 읽는지 확인한다.
2. Zone 이동은 같은 character 예상 cooldown을 보존하되 client/character 교체와 disable/world exit는 지운다. HP 0에서는 신규 타이머/입력이 시작되지 않음을 확인한다.

그 다음 동결한 실제 입력 배치에서 생존 캐릭터로 Z/X/C를 사용해 server phase 0 확인과 예상 표시를 한 번 관찰하고, 표시가 server exact remaining이나 server-ready 판정이라고 주장하지 않는다. 정확한 남은 시간이 제품 요구라면 별도 승인된 서버 계약 변경으로 remainingTicks 또는 server tick/time을 전송해야 하며, 현재 최소 후보에는 포함하지 않는다.
