# WAL (Write-Ahead Log)

## 1. 개요
본 문서는 CacheLib에 도입한 WAL의 구조와 설계 근거를 설명한다.  
WAL을 넣은 목적은 Write-Back 캐시가 flush하기 전에 죽었을 때 생기는 유실 창을 좁히는 것이다.  

## 2. 도입 배경

기존 구조(Write-Back)에서 durability는 오직 주기적 DB flush에 의존했다.  
`FlushDispatcher`는 마지막 변경 후 30초가 지난 dirty 데이터를 DB에 write-back하는데, 이 30초 사이에 서버가 죽으면 그 구간의 변경은 그대로 유실된다.  

이 유실 창을 없애는 가장 단순한 방법은 매 변경마다 즉시 DB에 쓰는 것이지만, 그러면 애초에 캐시를 둔 이유(DB 부하 절감, 저지연 처리)가 사라진다.  
대신 **인벤토리 캐시의 장애 시 유실 창을 줄이기 위해** WAL을 도입했다.
변경 내용을 별도 로그 파일에 순서대로 append하고, 재기동 시 유효하게 남은 로그로 캐시 상태를 재구성한다. 응답 전에 fsync 완료를 기다리지는 않으므로, 성공 응답을 받은 모든 변경의 영속성을 보장하는 구조는 아니다.

### 거래소 정합성과의 경계

거래 유실 방지는 WAL의 몫이 아니다. 캐시와 DB를 오가는 배송의 exactly-once는 [Bazaar.md](Bazaar.md)의 Outbox/Inbox가 맡는다.  
WAL이 담당하는 것은 캐시가 flush 전에 죽었을 때의 유실이다. 드랍·사용·장착처럼 거래 밖에서 일어나는 인벤토리 변경은 Outbox/Inbox가 다루지 않으므로 이쪽 보호가 따로 필요하다.  

### 적용 범위 — 인벤토리

`WalType`은 `INVENTORY` 하나다. 골드(`CacheStorageCurrency`)는 일부 유실을 허용하고 DB Write-Back에 의존하는 정책으로, WAL을 적용하지 않는다. 장애 시 DB에 반영되지 않은 골드 변경은 유실될 수 있다.

데이터별 저장 전략은 갱신 빈도와 손실 허용 수준에 따라 구분한다. [데이터 특성 기반 저장 전략](Bazaar.md#21-데이터-특성-기반-저장-전략-분리)을 따른다.

WAL을 적용한 인벤토리도 마지막 변경 후 30초를 기준으로 DB에 write-back한다.
WAL은 DB 저장 전 장애의 유실 범위를 줄이고, write-back은 변경을 모아 DB 부하를 줄인다.

## 3. "Write-Ahead"의 정확한 위치

표준적인 WAL은 "로그를 먼저 쓰고, 그 다음 데이터 페이지를 바꾼다"는 순서를 강제해 redo 가능성을 보장한다.  

캐시 변경과 WAL 기록은 같은 shard 임계구역에서 처리하지만, 경로에 따라 순서가 다르다.

- **소비·보상 추가·장착**: 복사한 이미지에 변경 → WAL append → 성공한 이미지를 캐시에 반영.
- **일반 변경·거래소 배송·inbox 전진**: 캐시 변경 → WAL append 시도 → dirty 등록.

그럼에도 Write-Ahead라 부르는 이유는 비교 대상이 캐시 변경이 아니라 DB 반영이기 때문이다.  
로그 append는 해당 캐시 snapshot의 DB write-back보다 먼저 수행된다. write-back의 30초 조건은 마지막 변경 이후의 대기 정책이며, 최대 지연이나 최대 유실 시간의 상한은 아니다.
DB 반영 전 크래시가 나면 파일에 남아 CRC 검증을 통과한 레코드까지 캐시 상태를 재구성할 수 있다. 현재 `Write()`는 `fflush()` 성공 후에만 LSN을 반환하므로 성공한 append의 CRT 버퍼 잔류로 인한 프로세스 크래시 유실 창은 제거한다. 실패한 append와 메모리 변경 후 append 전의 크래시 구간은 여전히 별개다.

> 같은 shard 임계구역은 다른 스레드의 중간 상태 관측을 막는다. 프로세스 크래시나 I/O 실패에 대한 원자적 커밋을 의미하지는 않는다. 캐시 변경 뒤 append 전에 죽거나, append/fsync가 실패한 구간은 복구되지 않을 수 있다.

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
- CRC는 header + payload 전체를 커버한다. 계산 시 `crc` 필드는 0으로 채운 상태로 구하고, 검증 시에도 동일하게 0으로 되돌린 뒤 재계산한다 — payload뿐 아니라 헤더(특히 length, lsn) 손상도 탐지하기 위함이다.   
- WAL은 payload 내부 레이아웃을 모른다. 타입 dispatch와 역직렬화는 상위 레이어(`WALManager`)의 책임이며, WAL은 순수하게 "타입 태그가 달린 바이트 운반자"로 남는다.  

## 5. LSN 설계

```
LSN = (segment << 32) | offset
```

세그먼트 번호와 파일 내 오프셋을 하나의 64bit 값에 담는다. 이렇게 하면:
- 별도의 전역 카운터 없이 `Write()` 내부에서 자연스럽게 LSN이 발급된다 (락 안에서 발급되므로 전역 순서가 보장된다).
- 세그먼트 번호가 LSN에서 산술(`lsn >> 32`)로 그대로 유도되어, truncate 판정에 파일명을 다시 파싱할 필요가 없다.

LSN은 인벤토리 blob 안에(`lastLsn` 필드) 저장한다. 별도 컬럼이 아니라 도메인 데이터와 같은 blob에 두는 이유는, flush가 blob을 통째로 UPDATE하므로 스키마 변경 없이 데이터와 원자적으로 영속되기 때문이다.  

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

- **Write**: `fwrite()`와 `fflush()` 성공 후 LSN을 반환한다. OS 페이지 캐시까지 전달하며, 디스크 영속화 완료를 뜻하지 않는다.
- **Fsync**: 별도 스레드가 목표 20ms 간격으로 `FlushFileBuffers()`를 수행한다. 스케줄링·I/O 지연이 더해지므로 최대 유실 시간이 20ms로 제한되는 것은 아니다.

처리량 비교와 주기 선택 근거는 [WAL 벤치마크 결과](Benchmark_WAL.md#3-fsync-주기별-벤치마크)를 참고한다.

| 장애 종류 | 손실 범위 |
|---|---|
| 프로세스 크래시 (abort, 예외, 강제 종료) | 성공한 append는 OS 페이지 캐시에 전달되어 CRT 잔류 창 없음. 메모리 변경 후 append 전 크래시, append 실패·차단 경로의 변경 유실 가능성은 남음 |
| OS 크래시 / 정전 | 목표 fsync 간격 20ms + 스케줄링/경합 지연 — 아직 `FlushFileBuffers()` 안 된 tail 레코드만 (파일 전체 아님) |

필요 시 fsync 간격을 줄이면 정전 손실 창까지 좁힐 수 있지만, 현재의 `sleep_for()` 기반 구현은 Windows 타이머 해상도보다 짧은 간격을 정확히 재현하지 못한다 (durability ↔ throughput 트레이드오프).


## 7. Segment / Rotation

WAL은 `LIMIT`(현재 1MB) 바이트마다 새 세그먼트 파일로 회전한다 (`base.1`, `base.2`, ...). 활성 세그먼트 파일이 곧 현재 append 대상이며, `Write()`가 `offset + recordSize > LIMIT`을 감지하면 이전 세그먼트를 fsync, close하고 다음 번호로 새 파일을 연다.  

## 8. 장애 복구 — Replay

부팅 시 세그먼트를 번호순으로 스캔하며 레코드를 검증한다.

```
레코드마다: magic 확인 → length로 경계 계산 → CRC 검증
  실패 시 → 그 지점에서 해당 세그먼트 스캔 중단 (재동기화하지 않음)
```

Torn tail 절단: 절단된 지점을 찾은 뒤, 다음 append를 시작하기 전에 파일을 그 유효 offset으로 truncate(`resize_file`)한다. 이 절단이 없으면 append 모드(`ab+`)가 깨진 꼬리 뒤에 그대로 이어 써서, 로그 한가운데 쓰레기가 영구히 남고 이후 재기동에서 그 지점 뒤의 멀쩡한 레코드까지 replay되지 않는다.  
Full image 로깅: 레코드는 delta가 아니라 *변경 후 blob 전체*다. 그 덕에 replay는 key별로 map에 덮어쓰기만 하면 자동으로 "가장 마지막 이미지"만 남는다. delta 로깅이었다면 순서대로 재적용하는 로직이 필요했을 뿐 아니라, 뒤에서 다룰 truncate에서 옛 이미지까지 보존해야 하는 문제가 생겼을 것이다.  

## 9. Truncate — 세그먼트 보존/삭제

로그가 무한히 자라지 않도록, DB에 이미 반영(flush)된 레코드만 있는 세그먼트는 주기적으로 삭제한다.  
Full image 로깅 덕분에 보존해야 할 최소 정보는 key별로 "아직 flush 안 된 가장 최신 레코드의 LSN" 뿐이다 (그 이전 이미지는 어차피 replay에서 버려지므로 가치가 없다). `WALManager`가 두 개의 장부로 이를 추적한다.  

```cpp
std::unordered_map<uint64_t, uint64_t> m_unflushedInventory; // key → 미flush 최신 lsn
std::unordered_map<uint32_t, uint32_t> m_segRefCnt;           // seg → 그런 레코드 보유 개수
```

segment 참조 카운트는 공통으로 사용 가능하다.  
key는 레코드 마다 다르기 때문에 key-unflushedLSN 매핑만 별도로 유지하면 된다.   

#### 전이 규칙
- `Write` 성공: 이전 lsn이 있으면 그 세그먼트 refcnt−-, 새 lsn의 세그먼트 refcnt++
- `OnInventoryFlushed(key, flushedLsn)`: 장부의 lsn이 flushedLsn보다 크면 skip한다. DB I/O 동안 shard lock을 놓으므로 정상적으로 재수정될 수 있다. 그렇지 않으면 refcnt를 낮추고 장부에서 제거한다.

삭제 경계: `refcnt > 0`인 세그먼트 중 최솟값. 활성 세그먼트는 경계 계산과 무관하게 항상 보존한다 — 세그먼트 번호는 절대 재사용하지 않는데, 재사용하면 새로 발급되는 LSN이 DB에 이미 기록된 이전 LSN보다 작아질 수 있어 복구 판정(`lsn > dbLSN`)이 조용히 무력화되기 때문이다.  
truncate는 5초 주기로 fsync 스레드에서 실행한다. append와 최신 LSN 참조 등록을 같은 WALManager mutex로 묶어, 새 레코드가 참조 장부에 오르기 전에 삭제 경계가 계산되는 틈을 막았다. 삭제 경계를 캡처할 때 활성 세그먼트로 상한을 고정하므로 이후 append/rotation이 일어나도 안전하며, 디렉토리 스캔과 파일 삭제는 그 mutex 밖에서 실행한다.

Restore와의 상호작용: 재기동 복구로 캐시에 되살린 key는 정의상 미flush 상태이므로, 복구 성공 시 이 두 장부에도 등록해야 한다.  
등록하지 않으면 truncate가 "DB에 아직 없는 유일한 사본(WAL)"을 지워버릴 수 있다 — 그래서 `Initialize()`는 반드시 `Restore()`를 마친 뒤에 fsync/truncate 스레드를 시작한다.

### 노드별 파일 소유권과 LSN

기본 prefix는 기존 `redo`를 유지한다. **WAL 경로(prefix)마다 한 시점에 하나의 인스턴스만 소유한다는 전제**다. 여러 서버나 테스트 노드를 같은 작업 디렉토리에서 실행할 때는 `Initializer(interval, prefix)`로 `wal/node-a/redo`, `wal/node-b/redo`처럼 서로 다른 경로를 지정해야 한다. 부모 디렉토리는 자동 생성하며, 같은 prefix의 중복 실행을 파일 잠금으로 검사하지 않는다. 크래시 복구는 이전 실행과 같은 prefix를 사용해야 하며, DB에 `lastLsn`이 남은 채 WAL만 삭제하면 안 된다.

다른 노드에서 저장된 캐릭터를 로드하면 해당 blob의 `lastLsn`보다 큰 LSN을 발급하도록 로컬 세그먼트를 전진시킨다. 캐릭터 하나의 캐시 소유자는 한 시점에 한 노드라는 전제이며, 여러 노드가 같은 캐릭터를 동시에 수정하는 분산 소유권 제어는 구현 범위 밖이다.

## 10. 장애 대응 — Degraded Mode

WAL 기록에 실패하거나 일시 차단된 동안에도 인벤토리 작업은 계속 처리한다. 소비·보상 추가·장착·일반 변경·배송·inbox 전진 모두 메모리 변경과 dirty 등록을 유지하고, 기존 DB flush 경로로 저장한다. WAL 기록 실패만으로 `StorageFailure`를 반환하지 않는다.

- WAL 기록에 실패하면 기존 `lastLsn`을 유지한다. DB flush 전에 서버가 종료되면 WAL에 남지 않은 변경은 유실될 수 있다.
- append 성공은 `fwrite()` + `fflush()` 성공을 뜻한다. 디스크 fsync는 기본 20ms 주기로 별도 수행한다.
- 일시 차단은 10초 후 해제해 기록을 재시도한다. 이후 변경에서 append가 성공하면 그 시점의 전체 이미지가 기록된다.

## 11. Sharded Cache와 WAL — 병렬과 직렬이 맞물리는 지점

샤딩은 캐시 접근을 shard mutex 단위로 병렬화하기 위한 구조다. 반면 WAL은 전역 LSN 순서를 지켜야 하므로 append가 구조적으로 직렬화된다.  
— 모든 shard의 변경이 결국 같은 로그 파일, 같은 락을 거쳐야 한다. 얼핏 병렬 캐시 구조와 상충하는 것처럼 보인다.

그런데 두 구조가 직렬화하는 대상이 다르다.
| | 성격 | 병렬성 |
|---|---|---|
| Cache 접근 (해시 테이블 조회, 뮤테이션) | 상대적으로 무겁고, shard별로 독립적이어야 함 | shard mutex로 병렬 |
| WAL append (레코드 조립 → 버퍼 memcpy → `fwrite` → `fflush`) | 호출마다 OS 페이지 캐시로 전달하며 I/O 대기 가능 | 직렬 |

캐시 조작은 shard별 병렬로 남고, WAL의 `fwrite()`와 `fflush()`는 같은 append mutex 안에서 직렬화된다. 따라서 writer 수를 늘려도 WAL 처리량이 함께 증가한다고 볼 수 없다.
주기적 fsync는 별도 스레드로 분리했고, dirty 상태를 확인·조정하는 구간만 append mutex를 사용한다. `FlushFileBuffers()`는 그 mutex를 풀어놓은 뒤 실행하고 sync mutex로 rotation·종료 시 handle 수명을 보호한다. 주기적 fsync의 `fflush()`와 writer 사이의 경합은 제거됐지만 writer 간 경합과 저장장치 I/O 영향은 남는다. 주기별 처리량은 [WAL 성능 벤치마크](Benchmark_WAL.md)에서 비교한다.

## 12. 참고
- [CacheLib](CacheLib.md) — Write-Back/Read-Through 구조, WAL이 얹히는 지점
- [CacheLib ACID](CacheLib_ACID.md) — 캐시 상태값과 ACID 관점의 WAL 위치
- [Cache Durability Test](CacheDurabilityTest.md) — Crash 복구 검증
- [WAL 성능 벤치마크](Benchmark_WAL.md) — fsync 주기 및 writer 경합에 따른 처리량 측정, 20ms 선택 근거
- [WAL.h](BaseLib/WAL.h) 
- [WALManager.h](CacheLib/Persistence/WALManager.h)
