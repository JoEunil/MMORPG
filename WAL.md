# WAL (Write-Ahead Log)

## 1. 개요
본 문서는 CacheLib에 도입한 WAL의 구조와 설계 근거를 설명한다.  
목적은 하나다 — **Write-Back 캐시의 flush 이전 유실 창을 좁히는 것.**  

## 2. 도입 배경

기존 구조(Write-Back)에서 durability는 오직 주기적 DB flush에 의존했다.  
`FlushDispatcher`는 마지막 변경 후 30초가 지난 dirty 데이터를 DB에 write-back하는데, 이 30초 사이에 서버가 죽으면 그 구간의 변경은 그대로 유실된다.  

이 유실 창을 없애는 가장 단순한 방법은 매 변경마다 즉시 DB에 쓰는 것이지만, 그러면 애초에 캐시를 둔 이유(DB 부하 절감, 저지연 처리)가 사라진다.  
대신 **캐시 자체가 자기 durability를 보장하도록** WAL을 도입했다.  
변경이 일어날 때 그 변경 내용을 별도 로그 파일에 순서대로 기록해두면, 서버가 죽어도 재기동 시 그 로그로 캐시 상태를 재구성할 수 있다.  

WAL을 얹은 뒤에도 **30초 write-back 주기는 그대로 유지**한다.  
다만 이 정책의 역할이 명확해진다 — 원래는 "DB 부하 절감"과 "durability 확보"라는 두 역할을 동시에 지고 있었는데, durability를 WAL이 넘겨받으면서 write-back은 순수하게 *DB 반영 시점을 늦춰 부하를 줄이는* 최적화 정책으로만 남는다.  
각 메커니즘이 자기 목적에 맞는 일만 하게 된 셈이다.

## 3. "Write-Ahead"의 정확한 위치

표준적인 WAL은 "로그를 먼저 쓰고, 그 다음 데이터 페이지를 바꾼다"는 순서를 강제해 redo 가능성을 보장한다.  

이 구현은 **캐시 변경(메모리)과 WAL 기록이 같은 shard critical section 안에서 연속으로 일어난다**   
— 정확히는 캐시를 먼저 바꾸고, 그 직후 같은 락 안에서 WAL에 기록하는 순서다. 즉 로그가 캐시 변경보다 시간상 "먼저" 쓰이는 것은 아니다.  

그럼에도 Write-Ahead라 부르는 이유는 비교 대상이 캐시 변경이 아니라 **DB 반영**이기 때문이다.  
로그는 DB에 반영되기 훨씬 전(최대 30초 전)에 먼저 기록된다.  
이 로그가 있기 때문에, DB 반영 전에 크래시가 나도 캐시 상태를 재구성할 수 있다.  

> 캐시 변경과 로그 기록은 같은 임계구역 안에서 원자적으로 묶인 하나의 사건에 가깝다.

## 4. 레코드 포맷

```cpp
#pragma pack(push, 1)
struct WALHeader {
    uint32_t magic;
    uint16_t type;   // payload 타입 태그 — WAL은 내용을 모름, 라우팅용
    uint16_t length; // payload 길이
    uint64_t lsn;    // LSN = (segment << 32) | offset
    uint32_t crc;
};
#pragma pack(pop)
// [WALHeader][Payload]
```

- `pack(1)`로 패딩을 제거해 `sizeof(WALHeader) == 20`을 보장한다.  
- CRC는 **header + payload 전체**를 커버한다. 계산 시 `crc` 필드는 0으로 채운 상태로 구하고, 검증 시에도 동일하게 0으로 되돌린 뒤 재계산한다 — payload뿐 아니라 헤더(특히 length, lsn) 손상도 탐지하기 위함이다.   
- WAL은 payload 내부 레이아웃을 모른다. 타입 dispatch와 역직렬화는 상위 레이어(`WALManager`)의 책임이며, WAL은 순수하게 "타입 태그가 달린 바이트 운반자"로 남는다.  

## 5. LSN 설계

```
LSN = (segment << 32) | offset
```

세그먼트 번호와 파일 내 오프셋을 하나의 64bit 값에 담는다. 이렇게 하면:
- 별도의 전역 카운터 없이 `Write()` 내부에서 자연스럽게 LSN이 발급된다 (락 안에서 발급되므로 전역 순서가 보장된다).
- 세그먼트 번호가 LSN에서 산술(`lsn >> 32`)로 그대로 유도되어, truncate 판정에 파일명을 다시 파싱할 필요가 없다.

**LSN은 인벤토리 blob 안에(`lastLsn` 필드) 저장한다.** 별도 컬럼이 아니라 도메인 데이터와 같은 blob에 두는 이유는, flush가 blob을 통째로 UPDATE하므로 **스키마 변경 없이 데이터와 원자적으로 영속**되기 때문이다.  

복구 규칙은 한 줄로 요약된다:  

```
레코드.lsn > blob.lastLsn  ->  아직 DB에 반영 안 됨 -> 재적용
레코드.lsn ≤ blob.lastLsn  ->  이미 반영됨          -> skip
```

**주의 — 레코드는 자기 자신의 LSN을 담을 수 없다.**   
payload가 조립되는 시점엔 아직 LSN이 발급되기 전이라, payload 안의 `lastLsn`은 항상 *직전* 레코드의 LSN(한 세대 이전 값)이다.   
그 레코드의 진짜 위치는 `WALHeader.lsn`이며, 복구 시 이 값으로 다시 채운 뒤에 캐시에 적용해야 이후 flush 정산과 재부팅 비교가 정확히 수렴한다.  

## 6. 동기 Write / 비동기 fsync

데이터가 디스크에 닿기까지 세 계층을 거친다.  

```
fwrite()          ->  1. CRT 버퍼 (C runtime의 표준 입출력 함수가 내부에 갖고있는 메모리 버퍼)
fflush()          ->  2. OS 페이지 캐시 (커널)   — 프로세스가 죽어도 생존, 프로세스 소유가 아닌 커널 소유 영역이기 때문.
FlushFileBuffers() ->  3. 디스크                 — 정전에도 생존
```

`WAL::Write()`는 shard 락 안에서 호출되므로 **1 단계까지만** 하고 빠진다(레코드 조립 + memcpy + fwrite, 디스크 I/O 대기 없음).   
2,3 단계(`Fsync()`)는 별도 스레드가 **50ms 주기**로 일괄 처리한다.   
순서 보장(동기 Write)과 내구성 보장(비동기 fsync)을 분리해, 쓰기 경로의 레이턴시는 memcpy 수준으로 유지하면서 손실 창만 fsync 주기로 제한하는 구조다.  

매 레코드마다 `fsync()`를 하지 않은 이유는 처리량 때문이다 — 레코드 단위로 디스크 I/O를 기다리면 처리량이 급격히 떨어진다.   
`fflush()`만으로 충분한 이유는, 실제로 방어하려는 대상(프로세스 크래시)에는 OS 페이지 캐시가 프로세스와 무관하게 생존하기 때문이다.  

| 장애 종류 | 손실 범위 |
|---|---|
| 프로세스 크래시 (abort, 예외, 강제 종료) | **없음** — fwrite된 레코드는 OS 페이지 캐시에 남아 프로세스 생명주기와 무관하게 생존 |
| OS 크래시 / 정전 | 최대 fsync 주기(50ms)분 — 그마저도 파일 전체가 아니라 아직 fsync 안 된 **tail 레코드만** |

필요 시 fsync 주기를 줄이면 정전 손실 창까지 좁힐 수 있다 (durability ↔ throughput 트레이드오프).


## 7. Segment / Rotation

WAL은 `LIMIT`(현재 1MB) 바이트마다 새 세그먼트 파일로 회전한다 (`base.1`, `base.2`, ...). 활성 세그먼트 파일이 곧 현재 append 대상이며, `Write()`가 `offset + recordSize > LIMIT`을 감지하면 이전 세그먼트를 fsync, close하고 다음 번호로 새 파일을 연다.  

## 8. 장애 복구 — Replay

부팅 시 세그먼트를 번호순으로 스캔하며 레코드를 검증한다.

```
레코드마다: magic 확인 → length로 경계 계산 → CRC 검증
  실패 시 → 그 지점에서 해당 세그먼트 스캔 중단 (재동기화하지 않음)
```

**Torn tail 절단**: 절단된 지점을 찾은 뒤, 다음 append를 시작하기 전에 파일을 그 유효 offset으로 truncate(`resize_file`)한다. 이 절단이 없으면 append 모드(`ab+`)가 깨진 꼬리 뒤에 그대로 이어 써서, 로그 한가운데 쓰레기가 영구히 남고 이후 재기동에서 그 지점 뒤의 멀쩡한 레코드까지 replay되지 않는다.  
**Full image 로깅**: 레코드는 delta가 아니라 *변경 후 blob 전체*다. 그 덕에 replay는 key별로 map에 덮어쓰기만 하면 자동으로 "가장 마지막 이미지"만 남는다. delta 로깅이었다면 순서대로 재적용하는 로직이 필요했을 뿐 아니라, 뒤에서 다룰 truncate에서 옛 이미지까지 보존해야 하는 문제가 생겼을 것이다.  

## 9. Truncate — 세그먼트 보존/삭제

로그가 무한히 자라지 않도록, DB에 이미 반영(flush)된 레코드만 있는 세그먼트는 주기적으로 삭제한다.  
Full image 로깅 덕분에 보존해야 할 최소 정보는 **key별로 "아직 flush 안 된 가장 최신 레코드의 LSN"** 뿐이다 (그 이전 이미지는 어차피 replay에서 버려지므로 가치가 없다). `WALManager`가 두 개의 장부로 이를 추적한다.  

```cpp
std::unordered_map<uint64_t, uint64_t> m_unflushedInventory; // key → 미flush 최신 lsn
std::unordered_map<uint32_t, uint32_t> m_segRefCnt;           // seg → 그런 레코드 보유 개수
```

segment 참조 카운트는 공통으로 사용 가능하다.  
key는 레코드 마다 다르기 때문에 key-unflushedLSN 매핑만 별도로 유지하면 된다.   

**전이 규칙**
- `Write` 성공: 이전 lsn이 있으면 그 세그먼트 refcnt−-, 새 lsn의 세그먼트 refcnt++
- `OnInventoryFlushed(key, flushedLsn)`: 장부의 lsn이 flushedLsn보다 크면 skip(flush 도중 재수정되어 최신 이미지가 아직 미반영 상태, 레코드 lock 한 상태에서 수행되기 떄문에 도달하지 않는 부분), 아니면 refcnt−- 장부에서 제거  

**삭제 경계**: `refcnt > 0`인 세그먼트 중 최솟값. **활성 세그먼트는 경계 계산과 무관하게 항상 보존**한다 — 세그먼트 번호는 절대 재사용하지 않는데, 재사용하면 새로 발급되는 LSN이 DB에 이미 기록된 이전 LSN보다 작아질 수 있어 복구 판정(`lsn > dbLSN`)이 조용히 무력화되기 때문이다.  
truncate는 5초 주기로 fsync 스레드에서 실행한다 — hot-path(shard lock 보유 중)에서 디렉토리 스캔과 파일 삭제를 하면 락 경합이 생기므로 분리했다.  

**Restore와의 상호작용**: 재기동 복구로 캐시에 되살린 key는 정의상 미flush 상태이므로, 복구 성공 시 이 두 장부에도 등록해야 한다.  
등록하지 않으면 truncate가 "DB에 아직 없는 유일한 사본(WAL)"을 지워버릴 수 있다 — 그래서 `Initialize()`는 반드시 `Restore()`를 마친 뒤에 fsync/truncate 스레드를 시작한다.

## 10. 장애 대응 — Degraded Mode

WAL 기록(`fwrite`) 자체가 실패하면(디스크 가득 참 등) 곧바로 캐시를 막지 않는다. 대신 WAL 보호만 내려놓고(`blocked`) 서비스는 계속한다.  

- `blocked` 동안 캐시의 `lastLsn`은 마지막으로 성공한 LSN에 동결된다. 이 구간의 변경은 WAL 없이 기존 30초 flush 창으로만 보호된다 (write-back 캐시 원래 수준으로 일시 하락).
- 10초(`RESUME_TRIGGER`) 후 자동으로 재시도한다.
- 재개 후 첫 `Write` 성공은 **그 시점의 전체 이미지**를 싣고 가므로, blocked 구간에 누적된 변경까지 자동으로 메꿔진다.

즉 "WAL 장애 = 서비스 중단"이 아니라 "durability 보장 수준의 일시적 하락"으로 대응한다. 

## 11. Sharded Cache와 WAL — 병렬과 직렬이 맞물리는 지점

샤딩은 캐시 접근을 shard mutex 단위로 병렬화하기 위한 구조다. 반면 WAL은 전역 LSN 순서를 지켜야 하므로 append가 구조적으로 직렬화된다.  
— 모든 shard의 변경이 결국 같은 로그 파일, 같은 락을 거쳐야 한다. 얼핏 병렬 캐시 구조와 상충하는 것처럼 보인다.

그런데 두 구조가 직렬화하는 대상이 다르다.
| | 성격 | 병렬성 |
|---|---|---|
| Cache 접근 (해시 테이블 조회, 뮤테이션) | 상대적으로 무겁고, shard별로 독립적이어야 함 | shard mutex로 병렬 |
| WAL append (레코드 조립 → 버퍼 memcpy → `fwrite`) | 디스크 I/O를 기다리지 않는 버퍼드 연산, 짧음 | 직렬 |

**무거운 부분(캐시 조작)은 병렬로 남기고, 가벼운 부분(로그 버퍼 append)만 직렬화**되는 구조라 자연스럽게 맞물린다.   
진짜 느린 작업인 디스크 I/O(fsync)는 둘 중 어디에도 걸리지 않고 별도 스레드로 완전히 분리돼 있어, WAL의 직렬성이 캐시의 병렬성을 갉아먹지 않는다.

## 12. 참고
- [CacheLib](CacheLib.md) — Write-Back/Read-Through 구조, WAL이 얹히는 지점
- [CacheLib ACID](CacheLib_ACID.md) — 캐시 상태값과 ACID 관점의 WAL 위치
- [Cache Durability Test](CacheDurabilityTest.md) — Crash 복구 검증
- [WAL.h](BaseLib/WAL.h) 
- [WALManager.h](CacheLib/WALManager.h)
