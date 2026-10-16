
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

**접근 차단은 실패하는 순간 즉시 풀린다.** flush가 실패하면 그 즉시 AVAILABLE로 되돌리고, `Insert()`에서 뺐던 LRU 리스트 등록도 복원한다(다시 넣지 않으면 이 entry가 LRU 추적에서 이탈해 `MAX_CACHE_SIZE` 상한을 우회하는 좀비 entry가 된다). 
eviction이었다는 사실 자체는 여기서 취소되고, 이후로는 일반 dirty entry와 동일한 재시도 경로(즉시 2회 → 30초 주기 백오프)를 탄다. 데이터는 write가 성공할 때까지 버리지 않는다.

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

__도입 후__: 인벤토리 변경을 같은 shard 임계구역 안에서 WAL에 append하여, DB write-back 이전 장애의 복구 범위를 넓혔다. 현재 보호 대상은 인벤토리뿐이며 Gold는 WAL에 기록하지 않는다. 기본 fflush/fsync 대기 값은 20ms이고, 응답 전에 완료를 기다리지 않으므로 최근 tail과 I/O 장애 구간의 유실 가능성은 남는다.

서버 장애 발생 시 WAL Replay를 통해 Flush되지 않은 데이터를 캐시에 복원한 후 백그라운드 Write-Back으로 DB와 다시 동기화한다.  

WAL의 LSN 관리, Segment Rotation, CRC 검증, Replay 및 Truncate 정책 등 자세한 구현은 [WAL.md](WAL.md)에서 설명한다.  


## 4. ACID 보장

아래는 단일 캐시 연산의 원자성·격리성과, 조건부 장애 복구 범위를 ACID 관점에서 구분한 것이다. 전체 캐시 또는 성공 응답 전체에 대한 완전한 ACID 보장을 의미하지 않는다.
캐시와 DB 사이는 write-back 주기만큼 벌어지는 최종 일관성이며, 아래 네 항목과는 다른 축이다.  

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
- 같은 키의 flush는 하나만 in-flight로 허용하며, 완료·롤백 콜백은 발행 generation이 일치할 때만 적용한다. DB I/O 중의 새 변경은 dirty로 남겨 다음 snapshot에 포함한다.
- DB write 성공 시 `WriteDone`이 재시도 카운터를 초기화한다. eviction 대상이었던 키만 캐시에서 최종 제거되고, 그 외에는 AVAILABLE 상태로 캐시에 남아 계속 접근 가능하다 (write-back 캐시 본연의 동작 — flush됐다고 캐시에서 쫓아내지 않는다).
- DB write 실패 시 `Rollback`이 처리한다. eviction 시도였다면 즉시 AVAILABLE로 되돌리고 LRU에 재등록해 접근 차단을 풀고 eviction 자체를 취소한다. 이후 재flush는 처음 2회 즉시, 3회째부터는 `dirty_list` 재등록으로 백오프하며 재시도한다.
- 다만 이 재시도에는 상한이 없다. DB 장애가 길어지면 dirty entry가 계속 쌓이는데, evict하려면 DB write가 먼저 성공해야 하므로 그동안은 `MAX_CACHE_SIZE` 상한도 실질적으로 강제되지 않는다. WAL이 기록 실패에 degraded mode를 두는 것처럼 flush 경로에도 같은 장치가 필요하다.

__Durability (지속성)__
- 인벤토리 변경은 같은 shard 임계구역 안에서 WAL에 append한다. 재기동 시 파일에 남아 CRC 검증을 통과한 레코드까지 복구하며, 아직 fflush/fsync되지 않은 tail은 유실될 수 있다.
- Gold는 WAL 보호 대상이 아니며, 마지막 변경 후 30초 write-back 또는 정상 종료 시 DB 반영에 의존한다. 계속 변경되는 키에 대해 30초는 최대 유실 시간의 상한이 아니다.
- 서버 크래시 시 재기동 과정에서 WAL Replay로 flush되지 않은 Dirty 데이터를 캐시에 복원한다. 복원 판정은 `레코드 LSN > blob의 lastLsn` 비교이며, 몇 번을 재실행해도 같은 결과가 나오는 멱등한 절차다.
- 복원된 데이터는 이후 백그라운드 Write-Back을 통해 다시 DB와 동기화된다.
- 정상 종료는 입력 큐와 DB 작업 워커를 먼저 drain하여 마지막 rollback까지 반영한 뒤, dirty 데이터를 여러 batch로 DB에 저장하고 WAL을 최종 fsync한다. DB 저장이 성공한 경우 다음 부팅에서 해당 WAL 이미지는 `lastLsn` 비교로 skip한다. 종료 중에도 DB 저장이 실패하면 인벤토리는 남은 WAL 복구에 의존하고, WAL이 없는 Gold는 복구를 보장하지 않는다.
- WAL 기록(`fwrite`) 자체가 실패하면 즉시 서비스를 막지 않고 WAL 보호만 일시적으로 내려놓는다(degraded mode). 이 구간의 변경은 기존 30초 write-back 창으로만 보호되며, 일정 시간 후 자동으로 WAL 기록을 재개한다. 재개 후 첫 기록이 그 시점의 전체 상태를 다시 실어 보내므로 별도 보정 없이 자동으로 수렴한다. 상세는 [WAL.md](WAL.md) 참고.

## 5. 참고
- [CacheLib](CacheLib.md)
- [WAL 설계 문서](WAL.md)
- [Cache Durability Test](CacheDurabilityTest.md)
