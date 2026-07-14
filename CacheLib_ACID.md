
# CacheLib ACID 설계

## 1. 개요
본 문서는 CacheLib의 ACID 설계에 대해 설명한다.

## 2. 목적

캐시 데이터의 상태값과 WAL 기반 장애 복구를 통해
캐시-DB 간 정합성을 유지하고 ACID 특성을 만족하도록 설계하였다.

## 3. 구현

### 캐시 상태값

```cpp
enum class CACHE_STATUS : uint8_t {
    AVAILABLE,  // 0 — 정상 접근 가능
    EVICTING,   // 1 — LRU evict 대상, 접근 차단 중
    DB_READING, // 2 — DB 조회 중, 접근 차단 중
    EMPTY,      // 3 — 빈 슬롯
    BLOCKED,    // 4 — 상태값 리턴용 임시 필드 (캐시 내부에서 사용 안 함)
};

template<typename T>
struct CacheItem {
    uint64_t lastModified;
    CACHE_STATUS status;
    T data;
};
```

### EVICTING 상태 도입

__도입 배경__

LRU overflow 발생 시 evict 대상 데이터를 즉시 삭제하지 않고 EVICTING 상태로 전환한다.  
DB write 완료 이후 실제 삭제 처리하며, 실패 시 Rollback 처리한다.

__목적__

LRU overflow로 evict되는 데이터가 DB에 최신 상태로 write되지 않은 상태에서 read가 발생하면 데이터 일관성이 깨질 수 있다.  
EVICTING 상태를 도입하여, **이번 flush 시도가 진행되는 동안만** 해당 데이터에 대한 접근을 차단한다 — "곧 삭제될 데이터를 읽는 것"을 막는 게 목적이므로, 삭제가 임박하지 않은 순간(=flush 실패가 확인된 순간)까지 막을 필요는 없다.

### 처리 흐름

```
LRU overflow 발생
    → evict 대상 선정
    → EVICTING 상태로 전환 (접근 차단)
    → DB write-back 시도
        → 성공 (WriteDone) → 데이터 삭제
        → 실패 (Rollback)  → 즉시 AVAILABLE로 복귀 + LRU 리스트 재등록 (eviction 시도 취소)
            → 이후 일반 dirty entry와 동일하게 재시도 (처음 2회는 즉시, 3회째부터는 dirty 재마킹으로 30초 주기 백오프)
```

**접근 차단은 실패하는 순간 즉시 풀린다.** flush가 실패하면 그 즉시 AVAILABLE로 되돌리고, `Insert()`에서 뺐던 LRU 리스트 등록도 복원한다(다시 넣지 않으면 이 entry가 LRU 추적에서 이탈해 `MAX_CACHE_SIZE` 상한을 우회하는 좀비 entry가 된다). eviction이었다는 사실 자체는 여기서 취소되고, 이후로는 일반 dirty entry와 동일한 재시도 경로(즉시 2회 → 30초 주기 백오프)를 탄다. 데이터는 성공할 때까지 절대 버리지 않는다.

### DB_READING

__도입 배경__

캐시에 데이터가 로딩되지 않은 상태에서 여러 read 요청이 한꺼번에 발생하면 불필요하게 DB read가 중복으로 발생한다. 

__목적__

DB read 작업의 중복 처리를 줄이기 위함. 

### 처리 흐름

```
캐시 조회 요청
→ 캐시 miss
→ 해당 키에 DB_READING 상태 마킹
→ DB 조회 요청 (비동기)
→ 이후 동일 키 요청 들어오면
    → DB_READING 상태 감지
    → 중복 DB 조회 차단
    → 호출자가 재시도 처리. (클라이언트 로직으로)
→ DB 조회 완료
→ 캐시에 기록
→ AVAILABLE 상태 마킹
```

### WAL(Write-Ahead Log)

__도입 전__: durability는 전적으로 주기적 DB flush(`FlushDispatcher`, 마지막 변경 후 30초)에 의존했다. 이 30초 창 안에 서버가 죽으면 그 구간의 변경은 그대로 유실됐다.

__도입 후__: 캐시 데이터가 변경될 때 그 내용을 같은 임계구역 안에서 WAL에도 함께 기록해, 캐시가 flush 주기와 무관하게 자기 durability를 보장하도록 바꿨다. 30초 write-back 주기 자체는 그대로 유지되지만, 이제 이 정책은 순수하게 *DB 반영 시점을 늦춰 부하를 줄이는* 목적만 남고, durability는 WAL이 전담한다.

서버 장애 발생 시 WAL Replay를 통해 Flush되지 않은 데이터를 캐시에 복원한 후 백그라운드 Write-Back으로 DB와 다시 동기화한다.  

WAL의 LSN 관리, Segment Rotation, CRC 검증, Replay 및 Truncate 정책 등 자세한 구현은 [WAL.md](WAL.md)에서 설명한다.  


## 4. ACID 보장

__Atomicity (원자성)__
- 캐시 변경은 하나의 Critical Section에서 수행된다.
- 캐시 변경 과정이 다른 스레드에 중간 상태로 노출되지 않도록 설계하였다.

__Consistency (일관성)__
- WAL Replay 이후 Dirty 데이터는 다시 Write-Back되어 DB와 최종 일관성을 유지한다.
- 현재 단일 연산 수준에서 Atomicity와 Isolation에 의해 뒷받침됨. (애플리케이션 로직이 올바른 경우)
- 다중 연산 간 일관성 보장을 위해 추후 캐시 레이어에서 DB Transaction과 유사한 처리가 필요하다

__Isolation (격리성)__
- 중간 상태의 데이터에 대한 외부 접근 차단
- EVICTING 상태인 키에 대해 Getter/Update 접근 차단 
- DB_READING 상태인 키에 대해 중복 조회 차단
- DB write 성공 시 `WriteDone`이 재시도 카운터를 초기화한다. eviction 대상이었던 키만 캐시에서 최종 제거되고, 그 외에는 AVAILABLE 상태로 캐시에 남아 계속 접근 가능하다 (write-back 캐시 본연의 동작 — flush됐다고 캐시에서 쫓아내지 않는다).
- DB write 실패 시 `Rollback`이 처리한다. eviction 시도였다면 즉시 AVAILABLE로 되돌리고 LRU에 재등록해 접근 차단을 풀고 eviction 자체를 취소한다. 이후 재flush는 처음 2회 즉시, 3회째부터는 `dirty_list` 재등록으로 백오프하며 무한정 재시도한다(데이터는 절대 버리지 않는다).

__Durability (지속성)__
- 캐시 변경 사항은 같은 임계구역 안에서 WAL에도 함께 기록되어, DB flush 이전 장애에도 복구 가능하다.
- 서버 크래시 시 재기동 과정에서 WAL Replay로 flush되지 않은 Dirty 데이터를 캐시에 복원한다. 복원 판정은 `레코드 LSN > blob의 lastLsn` 비교이며, 몇 번을 재실행해도 같은 결과가 나오는 멱등한 절차다.
- 복원된 데이터는 이후 백그라운드 Write-Back을 통해 다시 DB와 동기화된다.
- 정상 종료 시에는 WAL Replay가 필요 없다 — `CleanUp()`이 dirty 데이터를 먼저 동기적으로 DB에 반영한 뒤, WAL은 마지막 fsync만 수행한다(하우스키핑).
- WAL 기록(`fwrite`) 자체가 실패하면 즉시 서비스를 막지 않고 WAL 보호만 일시적으로 내려놓는다(degraded mode). 이 구간의 변경은 기존 30초 write-back 창으로만 보호되며, 일정 시간 후 자동으로 WAL 기록을 재개한다. 재개 후 첫 기록이 그 시점의 전체 상태를 다시 실어 보내므로 별도 보정 없이 자동으로 수렴한다. 상세는 [WAL.md](WAL.md) 참고.

## 5. 참고
- [CacheLib](CacheLib.md)
- [WAL 설계 문서](WAL.md)
- [Cache Durability Test](CacheDurabilityTest.md)
