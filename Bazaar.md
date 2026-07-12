# 통합 거래소 시스템 (Bazaar)

## 1. 개요

거래소 시스템은 플레이어 간 아이템 거래를 지원하며 서버 간 거래도 가능하도록 설계되었다.
판매자가 아이템을 등록하면 구매자가 다이아(과금 재화)로 구매하고, 판매자가 정산을 청구하는 방식으로 동작한다.

### 도입 배경
MMORPG 플레이 경험상 소규모 서버에서는 거래소가 활발하지 않아 불편함을 느꼈다.
거래소 데이터를 중앙 DB에서 관리하면 여러 GameServer에 걸친 통합 거래소를 구현할 수 있어 이를 해소할 수 있다.
다만 모든 서버를 통합할 경우 서버 경제 균형이 깨질 수 있어, 거래가 활발하지 않은 소규모 서버끼리만 통합하는 방향이 운영상 바람직할 것으로 판단된다.

### 한계점
- 구매 트랜잭션(sp_bazaar_buy)이 characters_diamond와 bazaar 테이블을 단일 트랜잭션으로 처리하므로, 두 테이블이 동일 DB 인스턴스에 존재해야 한다.
  - 재화 테이블과 거래소 테이블의 독립적인 샤딩이 불가능하고, 통합 서버 수가 증가할수록 단일 DB 인스턴스에 트랜잭션이 집중되어 병목이 될 수 있다.
- 등록 수수료 gold는 best-effort 보상으로 처리한다 (수수료 특성상 유실 허용 — [DB.md](DB.md) 차등 durability 참조).
- 배송(Outbox → 인벤토리)은 exactly-once로 수렴하지만, 크래시 시점에 따라 수령이 지연될 수 있다 (재접속 시 수렴).

## 2. 핵심 설계 결정

### 2.1 데이터 특성 기반 저장 전략 분리

데이터별 갱신 빈도와 손실 허용 수준이 다르므로 저장 전략을 분리 적용했다.  

| 구분 | Gold (게임플레이 재화) | Item (인벤토리) | Diamond (과금 재화) |
|------|----------------------|----------------|-------------------|
| 갱신 빈도 | 고빈도 (몬스터 처치, 퀘스트 보상 등) | 고빈도 (획득·사용·거래) | 저빈도 (아이템 구매, 부활 등) |
| 손실 허용 | 제한적 손실 수용 가능 | 거래 구간은 불허 (유실·복제 모두 치명적) | 0 (실제 금전 환산 가능) |
| 저장 전략 | In-Process 캐시 (Write-Back) | In-Process 캐시 + 거래 이동은 Outbox/Inbox | DB 직접 관리 + 트랜잭션 |
| 이유 | DB 부하 최소화, 저지연 처리 | 저지연을 유지하면서 cross-DB 배송 구간만 exactly-once 보장 | 완전한 ACID 보장, 감사 추적 필수 |

> 일반 게임플레이로 인한 인벤토리 변경은 Write-Back 특성상 flush 주기 내 유실 가능성이 남는다.  
> 거래소 배송은 Outbox가 재시도 소스로 남아 이 한계와 무관하게 수렴한다.  
> 현재는 거래소 처리만 로그를 남기고 있지만, 다이아몬드 같은 유료 재화는 모든 변동 이력을 별도 테이블에 기록해야 한다.  

### 2.2 Diamond 트랜잭션 처리 구조

다이아 거래는 Stored Procedure 트랜잭션으로 처리하여 완전한 ACID를 보장한다.  

애플리케이션 레벨 트랜잭션은 쿼리마다 네트워크 홉을 오가며 lock 점유 시간이 홉 수만큼 늘어나는 구조라 성능상 부적절하다고 판단했다.  
트랜잭션이 필요한 로직은 Stored Procedure로 구현해 DB 내부에서 원자적으로 완결되도록 설계했다.  

### 2.3 아이템 배송: Outbox / Inbox 패턴  

구매한 아이템은 **Bazaar DB → 인벤토리(Game DB 캐시)** 로 이동해야 하는데, 두 저장소를 하나의 트랜잭션으로 묶을 수 없다.  
두 장군 문제로 인해 "한 번에 정확히 반영"을 보장하는 것은 불가능하므로, **재시도(Outbox) + 멱등(Inbox)** 조합으로 exactly-once에 수렴시킨다.  

- **Outbox (`buyer_outbox`)**: 구매 트랜잭션 안에서 원자적으로 INSERT되는 "배송해야 할 사실"의 durable한 기록. `READY` 상태인 한 재배송이 반복된다 → **유실 없음**  
- **Inbox (인벤토리 blob 내 dedup ring)**: 배송 시 event_id를 아이템과 같은 blob에 기록해 원자적으로 영속. 재배송이 와도 dedup으로 스킵 → **중복 없음**  
- Outbox의 `CLAIMED` 전환은 인벤토리 blob이 DB에 durable해진 이후에만 수행한다.  

상세 계약(복구 테이블, 배송 불변식)은 [DB.md](DB.md)에 정리했다.  

## 3. 거래 흐름
![이미지 로드 실패](images/Bazaar.png)

| 저장소 | 관리 데이터 | 전략 |
|--------|------------|------|
| Cache (In-Process) | Inventory(+dedup ring), Gold | Write-Back |   
| DB (Bazaar) | Diamond, bazaar, bazaar_log, bazaar_claim, buyer_outbox | 트랜잭션, 감사 추적, 배송 Write-ahead |  

## 4. BUY 처리 상세 (sp_bazaar_buy)

1. listing 행 잠금 + 조회 (`status = 'TRADING'`, FOR UPDATE — 롤백 낭비 방지)  
2. 구매자 diamond 차감 (`diamond >= price` 조건으로 원자적 처리)  
3. bazaar CAS: `TRADING → SOLD` (`ROW_COUNT() = 0`이면 rollback)  
4. bazaar_log INSERT (append-only 감사 로그)  
5. bazaar_claim INSERT (`READY` — seller 정산 대기)  
6. **buyer_outbox INSERT (`READY` — 배송 Write-ahead)**  
7. COMMIT  

아이템 지급은 트랜잭션에 포함되지 않는다. 클라이언트의 `CHECK_OUTBOX` 요청이 outbox의 `READY` event를 인벤토리로 배송(DeliverItem)하고,   
인벤토리 blob flush가 완료된 뒤 CacheFlush가 outbox를 `CLAIMED`로 전환한다.  

__Crash 시나리오별 수렴__ (검증: [BazaarTest.md](BazaarTest.md) Test 6)  

| 크래시 시점 | 크래시 직후 상태 | 복구 동작 |
|------|------|------|
| 배송 반영 후, flush 전 | 인벤토리 미반영 + outbox READY | 재접속 시 **재배송** → 정확히 1개 지급 (유실 없음) |  
| flush 후, CLAIM 전 | 인벤토리 반영 완료(durable) + outbox READY | dedup이 재배송을 스킵 → **수량 변화 없이 outbox만 READY → CLAIMED** (중복 없음) |

## 5. CLAIM 처리 상세 (sp_bazaar_claim)

1. listing 조회 (`status = 'SOLD'`, `seller_id` 조건)
2. bazaar_claim `claim_status` 업데이트 (`READY → CLAIMED`, CAS로 이중 정산 방지)  
3. seller diamond 지급 (`INSERT ... ON DUPLICATE KEY UPDATE` — 행 없으면 생성)  
4. COMMIT

CLAIM은 전부 단일 트랜잭션 내에서 처리되므로 데이터 유실이 발생하지 않는다.  

> bazaar_log는 append-only 감사 로그로 유지하고, mutable한 정산 상태는 bazaar_claim으로 분리하여 로그 테이블의 불변성을 보장한다.

## 6. 설계 실수 — buyer_prev_quantity 복구 방식의 오류

초기 구매 설계에서는 bazaar_log에 `buyer_prev_quantity`(구매 직전 구매자의 아이템 수량)를 기록하고, 크래시 시 이 값을 기준으로 인벤토리를 복구할 수 있다고 판단했다.   

**착각의 근원은 판매 등록의 Saga 패턴이었다.**   
등록은 먼저 인벤토리에서 아이템을 차감하고, DB 등록에 실패하면 차감한 만큼 되돌리는 구조다.   
보상이 "자신이 차감한 양을 다시 더하는" 상대 연산이라, 보상 시점까지 인벤토리가 어떻게 변하든 유효하다.  

**그러나 구매는 다르다.**  
`prev_quantity`가 기록되는 시점(BUY 트랜잭션)과 인벤토리에 반영되는 시점 사이에 시차가 있고, 그 사이 인벤토리는 다른 요청(아이템 사용·획득·추가 구매)으로 계속 변한다.  
인벤토리를 트랜잭션 동안 lock으로 묶지 않는 한 **`prev_quantity`는 복구 기준 스냅샷이 아니라 그냥 과거의 한 값**이며, 이 값으로 복구하면 그 사이에 일어난 정상 변경까지 되돌려버린다.  
lock으로 묶는 순간 캐시를 도입한 의미(저지연)가 사라지므로 그것도 답이 아니었다.  

결론은 문제를 바꾸는 것이었다: **"상태 스냅샷을 되돌리는 복구"가 아니라 "전달해야 할 사실(event)의 기록 + 멱등 재적용"**.   
Outbox는 "이 아이템을 이 구매자에게 전달해야 한다"는 사실만 기록하므로 시차와 동시 변경의 영향을 받지 않고,  
Inbox(dedup)가 재적용의 멱등성을 보장한다. 상태가 아니라 이벤트를 기록하면 경쟁 조건 자체가 사라진다.  

이에 따라 `buyer_prev_quantity`는 현재 스키마에서 제거했고, bazaar_log는 복구 용도가 아닌 순수 감사 로그로만 유지한다.  

## 7. 향후 개선

- 현재는 모든 등급 거래 가능.  
- 전설급 이상은 `unique_item_id`를 부여해 `unique_items` 테이블로 소유권을 추적하고, 거래 이력을 별도 기록하면 개별 아이템 단위의 정확한 추적이 가능해진다.

## 8. 참고

- [DB.md](DB.md)
- [BazaarTest.md](BazaarTest.md) 
- [procedure.sql](Resources/DB/procedure.sql)
