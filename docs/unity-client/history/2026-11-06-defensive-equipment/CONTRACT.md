# 방어 장비 구현 계약 — DEFGEAR01/r1

사용자 선택: 방패2% + 갑옷3% 피해 감소. 외형·패링·몬스터 AI는 유지. 자동 지급 없음.

- 슬롯0=무기(0/3),1=방패(0/4),2=몸통(0/5). 0은 해당 슬롯 해제. 세 슬롯은 가방 권위·WAL 선확정·동일ID 모든 스택 차감 차단을 공유한다.
- 내부 API: EquipmentOperationStatus, EquipmentStateResult(status,weaponID,shieldID,bodyID), GetEquipmentState/EquipItem(charID,slot,itemID)/LoadEquipmentState. 무기 전용 임시 wrapper를 남기지 않는다.
- inventory376: 기존360 prefix + IEQ2 tag(0x32514549)@360 + weapon@364/shield@368/body@372. WAL type3(charID8+376=384). 지원 legacy84/352/360·전체zero2404·368 IEQ1/type2를 명시 해석한다. 구형 크기에 새 tag를 허용하지 않는다.
- opcode53 GET24 불변. opcode54 SET29: requestID8/epoch8/slot1/itemID4. opcode55 RES46: requestID8/epoch8/operation1/status1/slot1/weaponID4/shieldID4/bodyID4/base2/bonus2/effective2/defensePercent1. flags0·status0..8 유지, GETslot0/SET슬롯echo. 실패시 장비·수치0. SET dedup에는slot도 포함한다.
- ItemData 끝 defensePercent:u8, 방패4=2/몸통5=3/나머지0. 합산0..99 검증, 치명타 후 패링으로 막히지 않은 양수 피해에 넓은 정수곱/나눗셈 내림, 최소1. 원피해0·패링성공0 유지. 각 타격 시점의 확정 상태를 사용한다.
- 서버→Unity JSON format5 items.defensePercent. 이전format4는 명시 거부하고 마지막 유효 에셋/GUID를 보존한다. 맵 exporter 방향·AI 데이터는 바꾸지 않는다.
- ClientCore EquipmentStateView 값 타입과 기존 Network/Handler/ViewModel/dispatcher 재사용. GET/SET·slot·epoch·늦은 응답·취소·unknown mutation 검증 유지. 가방이 표시 권위이며 거래소는 이를 확인한다.
- 본인 무기 표시만 기존 기능 유지. 방패 파츠·갑옷 재질·패링 모션은 바꾸지 않는다. 다른 플레이어 장비 외형 패킷을 추가하지 않는다.

소유권: storage=IInventoryOperations/Cache 저장·codec·WAL·CacheTests, core=서버 계약/피해·hydration/서버2fixture, client=ClientCore/가방·거래소/Unity2fixture와FakeNetwork, 총괄=exporter·Unity content/importer/import2fixture·통합·검증·기록. Editor는 총괄 단독. 모두 완료 후 후보를 동결하고 직접 영향만 한 배치로 검증한다.

실제 자료 읽기: 지정3캐릭터 방패/갑옷0개. 거래소 방패 매물4건(최저200다이아), 갑옷 매물 없음. 직접 DB 지급은 하지 않으며 갑옷 실전 확인은 현재 전제 미충족이다. 근거 ROOT/UnityClient/Builds/ClientGrowthHUD/verification/defensive-equipment/available-items-before.json.
