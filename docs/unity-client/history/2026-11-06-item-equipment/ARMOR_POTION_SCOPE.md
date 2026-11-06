# 다음 장비·포션 범위 — 읽기 분석 (2026-11-06)

## 기준과 제외
- 정본은 Game-UnityClient/UnityClient/Builds/ClientGrowthHUD/source, 조사 대상 codex/item-equipment의 PR70 후보 코드다.
- 이 문서는 다음 범위 제안이다. 방어구·포션 등급 구현이나 수치·저장 변경 승인으로 읽지 않는다.
- 제품 코드·DB·실행·테스트·Git·Slack은 변경하지 않았다. 사용자 Game/refactor·몬스터 AI는 제외한다.

## 확인한 사실
- ItemData.h는 철검3만 weaponAttackBonus를 가진다. 철방패4·가죽갑옷5는 이름/type/무게만 있고 방어 효과가 없다.
- CharacterState는 기본 attack을 유지하고 equippedWeaponItemID에서 공격 보너스를 파생한다. 방어력·방패·갑옷 장착 필드는 없다.
- StateManager::Equipment는 장착 ID를 0/3으로 제한하고, requestID/epoch·session→zone→inventory 잠금으로 처리한다.
- 현재 장착 응답은 weaponID/baseAttack/bonusAttack/effectiveAttack 한 슬롯이다. 새 슬롯을 담을 계약은 없다.
- InventoryCodec.h는 360바이트 prefix + tag/weaponID로 368바이트만 쓴다. 검증도 장착 ID 0/3만 허용한다.
- 구형 84/352/360·전체 zero2404 읽기와 type1/type2 WAL 읽기가 있다. 새 장착 필드를 기존 tag/slot/padding에 숨기면 안 된다.
- CacheStorageInventory::ChangeItemLocked와 PartialUpdate는 음수 차감 때 equippedWeaponItemID와 같은 ID의 모든 스택을 보호한다.
- ZoneState_Skills.cpp의 몬스터 ApplyHit는 공격×phase 계수→기존 critical→특수 공격 패링→HP 차감 순서다. 방어 계산은 없다.
- 외형은 WorldScreen.SetOwnWeaponVisible의 자기 철검 표시뿐이다. 방패·갑옷 교체와 다른 플레이어 장비 외형 동기화는 제공되지 않는다.
- 서버 ItemData/StateManager::UseItem은 HP·MP 회복 필드 기반이다. ClientCore RequestItemUse/응답 파서는 양수 itemID를 일반적으로 받는다.
- 반면 InventoryScreen은 가방 사용 허용·사용 응답·안내 문구를 ID1/2로 제한하고, 숫자1/2와 두 퀵슬롯 수량도 ID1/2에 고정한다.
- ItemPresentation의 이름/효과는 서버 items를 읽지만 아이콘·badge·색 배열은 기존 ID1~5만 제공한다.
- 현재 처치 보상은 골드뿐이며 포션 자동 지급은 없다. 신규 포션의 획득 경로는 정해지지 않았다.
- 기존 기록의 ID33은 실제 가방에서 관찰됐지만 이번 조사에서는 DB를 읽지 않았다. 의미·예약 여부·실DB 카탈로그는 미확인이다.

## 제안: 방어 장비의 최소 확장
| 안 | 변경과 판단 |
| --- | --- |
| 방어 수치만 임시 표시 | 실제 장착·저장·피해 감소가 없어 게임 기능 완성이 아니다. 추천하지 않음. |
| 방패·갑옷을 명시 슬롯으로 함께 추가 | 추천 후보. 슬롯과 효과를 먼저 정하고 한 번의 저장 버전 확장으로 함께 처리한다. |
| 별도 장비 테이블로 분리 | 368바이트 자체는 유지하지만 DB 스키마·가방과의 원자성·WAL/복구 계약이 추가돼 더 작은 작업이 아니다. |
- 독립 슬롯을 영속화하면서 현재 weapon 전용 368바이트를 그대로 쓰는 안전한 빈 필드는 없다.
- 다음 저장 버전은 기존 prefix와 무기 상태를 보존하고 명시 shield/body ID를 추가하며, 기존 368/type2 읽기와 새 WAL 식별을 함께 지원해야 한다.
- slot을 일반화하더라도 새 프레임워크 대신 실제 필요한 무기·방패·몸통 책임만 둔다. 손상·미지원 버전을 빈 장착 성공으로 바꾸지 않는다.
- 가방 권위의 장착/해제·초기 선적재·구역 복사·조회/변경 응답·클라 모델/가방 표시·내보내기를 동일 묶음으로 확장한다.
- 장착품 차감 보호는 무기/방패/몸통 모두를 같은 inventory 잠금 안에서 검사한다. UI 판매 차단만으로 대체하지 않는다.
- 방어 효과의 최소안은 데이터의 방어 합계를 캐릭터에서 파생하고, 기존 패링 성공 분기 뒤 HP 차감 직전에 원피해를 줄이는 것이다.
- 고정 감소/비율 감소, 최소 피해 허용, 중첩·타격 시점 기준은 먼저 선택해야 한다. 현재 몬스터 선택·이동·phase·aggro 로직은 변경할 필요가 없다.
- 외형은 방패 파츠 표시와 기존 갑옷 재질/파츠 선택 범위를 정한다. 원격 외형까지 요구하면 full/delta·ClientCore 계약 확장이 별도 필요하다.

## 제안: 포션 등급의 경계
- 회복량/쿨 데이터와 기존 사용 요청은 재사용 가능하다. 등급 추가만으로 새 opcode나 inventory 저장 형식 변경이 필수는 아니다.
- 다만 가방 사용 허용·응답 문구와 두 퀵슬롯을 효과 데이터 기반으로 바꾸고, 등급 선택·수량·아이콘 연결을 함께 구현해야 한다.
- 새 ID는 실DB의 기존 항목과 ID33 의미를 읽기 확인한 뒤 정한다. 서버 ItemData에 없다는 이유로 빈 ID라 단정하지 않는다.
- 공급원이 없는 등급을 추가해 획득 가능한 아이템처럼 보여주지 않는다. 처치 포션 보상 부활·확률 드랍 테이블을 자동 계획하지 않는다.

## 다음 기획 질문과 독립 준비
- 한 번의 질문 추천: 다음 묶음을 ‘방패·갑옷의 실제 방어 효과’와 ‘등급 포션의 획득·사용 흐름’ 중 어디에 집중할지, 효과 방식·외형 범위·공급 필요 여부를 비교안으로 묶어 선택받는다. 숫자는 아직 정하지 않는다.
- 독립 준비 가능: 현재 아이템/슬롯/저장·wire 영향 표와 동일 장면의 UI·외형 비교안 작성. 기존 ID1/2 동작을 보존하는 데이터 기반 사용 판정 전환은 범위 승인 후 수행할 수 있다.
- 바로 실행할 별도 제품 수정은 이 분석에서 확정하지 않았다. 현재 PR70에 방어구 기능을 끼워 넣지 않는다.

근거: CoreLib/{ItemData.h,CharacterState.h,StateManager.cpp,ZoneState_Skills.cpp,PacketTypes.h}, CacheLib/{InventoryCodec.h,CacheStorageInventory.cpp}, ClientCore/ViewModel.cs, UnityClient/Assets/Client/Runtime/{UI/InventoryScreen.cs,Presentation/ItemPresentation.cs,World/WorldScreen.cs}.
