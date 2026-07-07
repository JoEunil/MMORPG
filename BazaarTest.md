# 통합 거래소 테스트 (BazaarTest)

## 1. 개요

이 문서는 [통합 거래소 시스템](Bazaar.md)의 기능 테스트, 경합 상황 테스트, Crash 시나리오 테스트, 부하 테스트를 기록한다.


## 2. 목적

- 기본 거래 흐름 (REGISTER → BUY → CLAIM) 정상 동작 검증
- Crash 시 DB 정합성 및 복구 가능성 확인
- 동시 구매 경합 상황에서 정합성 검증
- lock contention 발생 수준 관측

## 3. 테스트 환경

- CacheLib를 인스턴스화하여 각 인스턴스를 GameServer 1개로 설정하여 테스트를 진행한다.   
- MockMessageQueue를 통해 CacheLib에 메시지를 직접 주입하고 응답을 수신하는 방식으로 동작한다. 
- 커넥션 풀 크기 - Game DB: 3, Billing DB: 3

## 4. 테스트

### 4.1 기본 기능 테스트 (Test 1 ~ 5)

| 테스트 | 설명 |
|--------|------|
| Test1 | 인벤토리 아이템 추가/제거 |
| Test2 | Currency (Gold) 추가/조회 |
| Test3 | Diamond 추가/조회 |
| Test4 | Bazaar 기본 동작 (REGISTER → BUY → CLAIM) |
| Test5 | Seller 오프라인 상태에서 BUY + 재접속 후 CLAIM |

__결과__   
![이미지 로드 실패](images/TestBazaar.png)

- Test1: 아이템 추가/제거 정상 동작 확인 (인벤토리 상태 변화 확인)
- Test2: Gold 추가 및 조회 정상 동작 확인 (Gold 상태 변화 확인)
- Test3: Diamond 추가 및 조회 정상 동작 확인 (Diamond 상태 변화 확인)
- Test4: 기본 거래 흐름 정상 동작 확인 (bazaar 상태 변화, 구매자 diamond 차감, 판매자 다이아몬드 지급)
- Test5: 판매자가 오프라인 상태에서도 BUY가 정상 처리되고, 판매자 재접속 시 CLAIM이 정상 처리 확인. 

### 4.2 Crash 상황 테스트 (Test 6)

__시나리오__: `CRASH_POINT_BUY` 매크로 활성화 시, sp_bazaar_buy() COMMIT 이후 ~ 인벤토리 PartialUpdate 전 `abort()` 발생

__목적__: Crash 이후에도 bazaar_log 기반으로 복구 가능한 상태임을 확인

__흐름__:
1. Seller가 아이템 등록
2. Buyer가 구매 요청 → DB 트랜잭션 성공 후 abort()
3. DB 상태 및 인벤토리 상태 확인

__기대 결과__:

| 항목 | 기대 상태 |
|------|----------|
| bazaar status | SOLD |
| diamond | 차감 완료 |
| bazaar_log | 기록 있음 (buyer_prev_quantity 포함) |
| 인벤토리 (Cache) | 아이템 미반영 |
| 복구 가능 여부 | bazaar_log 기반 수동 복구 가능 |

__결과__:  

![이미지 로드 실패](images/TestBazaar6-1.png)
- Crash 없이 테스트: 정상 결과  

![이미지 로드 실패](images/TestBazaar6-2.png)
- Crash 상황: inventory에 아이템이 반영되지 않은 상태  

![이미지 로드 실패](images/TestBazaar6-3.png)
- Crash 후 DB 상태 확인: transaction 정상 처리 확인 및 다이아몬드 차감 확인. (900 -> 800)
- buyer_prev_quantity가 0으로 기록되어 있어, 수동 복구 시 아이템 수량을 buyer_prev_quantity 기준으로 1개 추가하여 복구할 수 있음을 확인하였다.

### 4.3 경합 상황 테스트 (Test 7)

__시나리오__: 10개 buyer가 5개 listing에 동시 구매 요청

__목적__: 동시 구매 경합 상황에서 CAS로 중복 구매가 발생하지 않음을 검증

| 항목 | 값 |
|------|-----|
| Buyer 수 | 10 |
| Listing 수 | 5 |
| 총 요청 수 | 50 |
| 기대 성공 | 5 |
| 기대 실패 | 45 |

__결과__

![이미지 로드 실패](images/TestBazaar7.png)

| 항목 | 결과 |
|------|------|
| 성공 | 5 |
| 실패 | 45 |
| commit | +5 |
| rollback | +45 |
| lock_waits | +1 |

### 4.4 부하 테스트 - lock contention 관측 (Test 8)

__목적__: 서버 성능 검증보다는 단일 listing에 대한 최대 lock contention이 얼마나 발생하는지 관측하는 것이 목적

__시나리오__: 40개 buyer가 단일 listing에 동시 구매 요청

| 항목 | 값 |
|------|-----|
| Buyer 수 | 40 (Server 20 × 2) |
| Listing 수 | 1 |
| 총 요청 수 | 40 |
| 기대 성공 | 1 |
| 기대 실패 | 39 |

__결과__ 

![이미지 로드 실패](images/TestBazaar8-1.png)

| 항목 | 결과 |
|------|------|
| 성공 | 1 |
| 실패 | 39 |
| commit | +1 |
| rollback | +39 |
| lock_waits | +2 |
| lock_time | +3 ms |
| lock_time_avg | 2 ms |

__slow query 관측 결과__
![이미지 로드 실패](images/TestBazaar8-2.png)

전부 2ms 내외로 측정되었으며, stored procedure의 max_rows_examined도 4~5로 측정되어 풀스캔 없이 필요한 row만 접근하는 것을 확인할 수 있다.  
stored procedure 40회 호출 중 3회만 slow query로 기록되었으며, 이는 lock contention으로 인해 query_time이 1ms 임계값을 초과한 것으로 추정된다.   
나머지 37회는 1ms 미만에 처리되어 기록되지 않았다.   
임계값을 1ms로 설정했기 때문에 관측된 수치인 것일 뿐 실제로는 전부 정상범위 내에서 처리된 것이다. 

## 5. 테스트 결과 정리

| 테스트 | 결과 | 비고 |
|--------|------|------|
| 기본 기능 (1~5) | 통과 | - |
| Crash 시나리오 (6) | 통과 | bazaar_log 기반 복구 가능 확인 |
| 경합 상황 (7) | 통과 | 정합성 검증 — 중복 구매 없음 |
| lock contention (8) | 통과 | 단일 listing 40 동시 요청 — lock_waits: 2회, lock_time_avg: 2ms |

__lock contention이 미미한 이유__
테스트 환경에서 요청 타이밍을 정확히 일치시킬 수 없어 실제 동시 경합이 제한적이었다.  
또한 stored procedure로 트랜잭션이 DB 내부에서 완결되어 lock holding time이 최소화된다.  
프로시저는 SELECT FOR UPDATE 없이 CAS 패턴(UPDATE WHERE status = 'TRADING')으로 경합을 처리하므로, 경합 지점이 bazaar 테이블의 단일 UPDATE로 한정되고 실패한 트랜잭션은 즉시 rollback된다.  

## 6. 참고

- [Bazaar.md](Bazaar.md)