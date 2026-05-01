
# CacheLib ACID 설계

## 1. 개요
본 문서는 CacheLib의 ACID 설계에 대해 설명한다.

## 2. 목적

캐시 데이터의 상태 변경을 제한 없이 허용하면 캐시-DB 간 데이터 정합성이 깨질 수 있다.  
캐시 데이터에 상태값을 도입하여 접근을 제어하고 ACID를 보장한다.

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
EVICTING 상태를 도입하여 DB write 완료 전까지 해당 데이터에 대한 접근을 차단한다.

### 처리 흐름

```
LRU overflow 발생
    → evict 대상 선정
    → EVICTING 상태로 전환 (접근 차단)
    → DB write-back 시도
        → 성공 (WriteDone) → 데이터 삭제
        → 실패 (Rollback)  → 재시도 (최대 3회)
            → 3회 초과 → 로그 남기고 삭제 처리
```

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

## 4. ACID 보장

__Atomicity (원자성)__
- DB write 작업이 완전히 완료되거나 복구됨
- DB write 성공 시 WriteDone, 실패 시 Rollback으로 처리

__Consistency (일관성)__
- 현재 단일 연산 수준에서 Atomicity와 Isolation에 의해 뒷바침 됨.(애플리케이션 로직이 올바른 경우)
- 다중 연산 간 일관성 보장을 위해 추후 캐시 레이어에서 DB Transaction과 유사한 처리가 필요하다

__Isolation (격리성)__
- 중간 상태의 데이터에 대한 외부 접근 차단
- EVICTING 상태인 키에 대해 Getter/Update 접근 차단
- DB_READING 상태인 키에 대해 중복 조회 차단

__Durability (지속성)__
- dirty 데이터는 DB flush를 통해 영구 저장 보장
- 서버 종료 시에도 잔여 flush 수행
- DB write 실패 시 최대 3회 재시도, 초과 시 로그 후 삭제   
-> 재시도 실패는 DB 장애상황이라 판단했고, 재시동 후 로그를 통해 수동복구하는 방식이 바람직하다고 판단.
