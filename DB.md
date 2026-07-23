# DB

## 1. 개요
본 문서는 이 프로젝트에서 사용된 DB 구성과 설계 결정에 대해 설명한다.

## 2. 구성
| 데이터베이스 | 테이블 | 설명 |
|---|---|---|
| Login | users | 사용자 계정 정보 |
| Game | characters | 캐릭터 정보 |
| Game | characters_inventory | 캐릭터 인벤토리 (수직 파티셔닝) |
| Game | characters_currency | 캐릭터 재화 (수직 파티셔닝) |
| Game | v_user_characters | 캐릭터 목록 조회용 View |
| Bazaar | bazaar | 거래소 등록 정보 |
| Bazaar | bazaar_log | 거래 이력 |
| Bazaar | bazaar_claim | 판매자 정산 처리 |
| Bazaar | buyer_outbox | 구매자가 수령해야할 아이템 기록 (Write-ahead 역할) |
| Bazaar | characters_diamond | 캐릭터 유료 재화 |

>거래소 관련 내용은 [거래소 설계 문서](Bazaar.md) 참고

## 3. 사용 기술
### DB 분리 (Login / Game / Bazaar)

도메인별로 데이터베이스를 분리하였다.  

- **Login** — 계정·인증 (게임 로직과 생명주기·보안 요구가 다름)  
- **Game** — 캐릭터·인벤토리·재화 (게임 시뮬레이션의 상태)  
- **Bazaar** — 거래소·유료 재화 (독립적으로 확장/분리 가능해야 하는 도메인)  

현재는 동일 MySQL 인스턴스의 별도 스키마이므로 물리적으로는 cross-schema 원자 트랜잭션도 가능하다.    
그러나 **커넥션 풀을 DB별로 분리**하여, 트랜잭션이 DB 경계를 넘지 못하도록 의도적으로 설계했다.  

 **원자성을 포기하는 대신 확장성(분리 가능성)을 얻고**, 그로 인한 cross-DB정합성은 Saga(등록)와 Outbox·Inbox(배송)로 보상한다.    

### Outbox / Inbox 패턴 (cross-DB 배송 정합성)

거래 완료 시 아이템은 **Bazaar DB → Game DB(인벤토리)** 로 이동해야 한다.  
두 DB가 하나의 트랜잭션으로 묶이지 않으므로, 배송의 exactly-once를 Outbox·Inbox 패턴으로 보장한다.  

**Outbox (`buyer_outbox`, Bazaar DB)**
- 구매 트랜잭션(`sp_bazaar_buy`) 안에서 **원자적으로** INSERT   
- "배송해야 할 아이템"의 durable한 기록 = **Write-ahead 역할**  
- 배송 완료 전까지 `delivery_status = 'READY'`  

**Inbox (인벤토리 blob 내 dedup ring, Game DB)**
- 배송(DeliverItem) 시 event_id를 인벤토리 blob 안의 ring(`recentEventIds[32]`, [head, tail))에 기록  
- 아이템과 **같은 blob이므로 flush 한 번으로 원자적으로 영속** — 별도 inbox 테이블·트랜잭션 불필요  
- 중복 검사는 ring **전체(32칸) 선형 탐색**: outbox read → deliver가 원자적이지 않아, CLAIMED 직후 truncate된 event의 스테일 재배송도 방어  

**복구 (크래시/유실)**
| 상태 | 처리 |
|---|---|
| Outbox READY | 아직 미배송 → **재배송** |
| Inbox에 event_id 존재 | 이미 배송됨 → **스킵** (중복 방지) |
| Inbox 있음 + Outbox READY | roll-forward → Outbox CLAIMED |

즉 **Outbox = 재시도 소스, Inbox = 멱등** 으로, 크래시·재시도에도 손실·중복 없이 수렴한다

**배송 불변식 (코드로 강제되는 순서)**
1. **CLAIM은 인벤토리 blob flush 성공 이후에만** — CacheFlush(DBWrite case 6) 단일 지점에서 수행.  
   dedup 히트 시에도 직접 CLAIM하지 않고 dirty 재마킹으로 flush→CLAIM 경로에 위임  
   (flush 전 CLAIM은 크래시 시 "Inbox 없음 + Outbox CLAIMED" = 아이템 유실을 만든다)  
2. **head(truncate)는 CLAIM에 성공한 연속 prefix만큼만 전진** — 중간 실패(예외) 시 거기서 중단.   
   미CLAIM event가 보장 구간([head, tail)) 밖으로 밀려나면 재배송 방어가 시한부가 된다  
3. **CLAIM UPDATE의 0 rows = 이미 CLAIMED = 성공** — `WHERE delivery_status='READY'` 덕분에  
   응답 유실 후 재시도가 멱등. READY인 행은 반드시 1 row를 반환하므로 둘을 혼동할 수 없다  
4. **ring full 시 배송 BLOCKED (backpressure)** — 미CLAIM event를 덮어쓰는 대신 배송을 멈춰,  
   중복 지급이 아니라 배송 지연으로 안전하게 실패한다  
5. head 전진은 스냅샷 head 기준 CAS(불일치 시 no-op) — claim pass 중복 실행에 의한 이중 전진 방지  

> 배송의 Write-ahead 기록은 별도 로그 파일이 아니라 **Outbox 행 자체**이다.  
> DB의 자체 redo log가 각 INSERT/UPDATE의 durability를 보장한다.  
>
> 이 문서의 "Write-ahead"는 **MySQL 엔진 자체의 redo log**를 가리킨다. Game DB 인벤토리 캐시가 자체적으로 갖는 앱 레벨 WAL(별도 로그 파일 + LSN + Replay)은 이와 별개 메커니즘이며 [WAL.md](WAL.md)에서 다룬다.

### 동시성 제어 — CAS + 비관적 잠금

구매 경쟁(동일 매물 동시 구매)은 `sp_bazaar_buy`에서 처리한다.  
- `SELECT ... FOR UPDATE` 로 리스팅 행을 잠가 **직렬화**(롤백 낭비 감소)  
- `UPDATE ... WHERE status='TRADING'` + `ROW_COUNT()` 로 **CAS 판정**  
  → 한 매물은 정확히 1회만 SOLD, 패자는 트랜잭션 롤백  

### 멱등성

- **Inbox dedup**: `characters_inbox.event_id` PK (배송 중복 방지)  
- **판매자 정산**: `INSERT ... ON DUPLICATE KEY UPDATE` (행 없으면 생성 → 정산금 손실 방지)  

### 차등 durability

정합성 보장 수준을 비즈니스 임팩트에 맞춰 차등한다.  
- **아이템** (복제/유실 치명적) → Saga·Outbox·Inbox로 강하게 보장  
- **등록 수수료 gold** → best-effort 보상 (수수료라 유실 허용)  

### 수직 파티셔닝

`characters` 테이블은 캐릭터 정보를 담는 테이블로, 기존에는 `inventory(blob)` 컬럼도 같은 테이블에서 관리하였다.

하지만 인벤토리 데이터는 다음과 같은 특성을 가진다.

- Zone 내부 시뮬레이션 로직에서 접근할 필요가 없음
- 접근 빈도와 응답 속도를 고려하여 Cache Server에서 유지 및 관리

따라서 애플리케이션 레벨에서 별도로 조회/저장되는 데이터를 같은 테이블에 두어 lock 경합 가능성을 높이는 것보다,   
`characters_inventory` 테이블로 수직 파티셔닝하는 것이 적합하다고 판단하였다.

### Composite Index

게임 접속 흐름은 다음과 같다.

```
로그인 서버 토큰 발급 → 게임 서버 진입 요청 → 토큰 검증(user_id 인가) → 캐릭터 선택 → 게임 진입
```
![이미지 로드 실패](images/SessionManagerDeadLock1.png)  
> 캐릭터 선택 화면

캐릭터 선택 단계에서 `characters` 테이블을 `user_id`, `channel_id` 기준으로 필터링하여 캐릭터 목록을 조회해야 한다.   

둘다 등가(=) 쿼리 이기 때문에 두 칼럼의 순서는 성능 차이가 없다. 

```sql
CREATE INDEX idx_user_channel ON characters(user_id, channel_id);
```


### View Table

```sql
CREATE OR REPLACE VIEW v_user_characters AS
SELECT 
    c.user_id,
    c.channel_id,
    c.char_id,
    c.name,
    c.level
FROM characters c
WHERE c.deleted_at IS NULL;
```

MySQL은 쿼리를 수신할 때마다 다음 단계를 거친다.

```
1. 파싱  →  2. 실행 계획 수립  →  3. 실행
```

초기 설계 의도는 반복 쿼리의 파싱 및 실행 계획 수립 비용을 줄이기 위함이었으나, 해당 캐싱은 Prepared Statement에 의해 처리된다.

View Table의 실질적인 이점은 다음과 같다.

- __쿼리 추상화__ — 애플리케이션 코드 단순화
- __유지보수__ — 조건 변경 시 View 한 곳만 수정하면 됨
- __민감 데이터 노출 제한__ — 필요한 컬럼만 노출하여 불필요한 데이터 접근 차단

### Stored Procedure
거래소의 구매(sp_bazaar_buy), 정산(sp_bazaar_claim) 등 원자성이 필수인 작업은 Stored Procedure로 처리한다. 
트랜잭션이 DB 내부에서 완결되어 lock holding time을 최소화하고, 애플리케이션 크래시가 트랜잭션 안정성에 영향을 주지 않는다.

프로시저 상세는 [거래소 설계 문서](Bazaar.md) 참고

## 4. 추후 업데이트

- Sharding - 튜플 증가로 탐색 속도 저하 시 적용. `channel_id` 기준 수평 샤딩 검토 
- 로그 DB - 로그인 기록 등 이력 관리용 별도 테이블 구성 

## 5. 참고
- [DB 초기화 스크립트](Resources/DB/init.sql)
- [DB 트랜잭션 정의 스크립트](Resources/DB/procedure.sql)
