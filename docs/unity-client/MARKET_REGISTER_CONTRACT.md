# 매물 등록 안전 계약 분석

2026-10-20 / 조사 기준은 Coordinator 제공 `codex/market-cancel` `f2cfb34`, 후속 구현 브랜치는 `codex/market-register-safety`. 실제 candidate 소스를 읽었으며 Git·빌드·시험·서버·DB·Slack 조작은 하지 않았다. 이번 소유 파일은 이 문서 하나다.

## 결론과 이번 후보

외부 등록을 안전하게 여는 작은 TCP/UI 연결안은 아직 없다. **이번 완결 후보는 기존 내부 등록의 불명 결과 자동보상 방지와 단일 종결 보완**이다. 등록 화면·외부 opcode·ClientCore API는 추가하지 않는다. 이 후보가 등록 전체의 원자성·재시도 안전·crash 복구를 완성했다고 보고하면 안 된다.

## 실제 경로와 영속 순서

- `CacheLib/Handler_Bazaar.cpp:72`: 품목/양수 수량 확인 → inventory `PartialUpdate(op2, -quantity)` → gold `TryWithdrawCurrency(max(10, price/10))` → Bazaar worker `ExecuteUpdate(13, ...)`. 수수료 규칙은 보존한다.
- `CacheLib/Config.h:87`: QUERY13은 즉시 `TRADING` INSERT다. 요청 식별자·등록 상태 조회·예약 행은 없다. 내부 요청/응답에도 requestID/listingID가 없다(`CoreLib/MessageTypes.h:228`).
- `CacheStorageInventory.cpp:157,267`: inventory shard lock 안에서 수량을 먼저 변경하고 전체 blob WAL append 뒤 dirty 마킹. WAL 반환0이어도 변경을 취소하지 않고 AVAILABLE을 반환한다.
- `WALManager.h:17,24,109,206`: WAL은 inventory만 기록한다. 기본 fsync는 별도 thread의 20ms 주기이며 append 완료는 fsync 완료가 아니다. write/fsync 저하 때 추가 append를 생략해도 cache 변경은 진행한다. 재시작은 header LSN이 DB blob LSN보다 큰 마지막 inventory 이미지를 복구한다.
- `CacheStorageCurrency.cpp:105,146`: gold는 별도 shard lock의 메모리 차감과 dirty 마킹만 있다. currency WAL/등록 intent는 없다.
- `FlushDispatcher.h:27,35,50`: 일반 dirty는 마지막 변경 후 30초가 지나면 inventory/currency를 각각 snapshot enqueue한다. eviction/종료 flush는 더 일찍 가능하다. `CacheFlush.cpp:36,120`은 각각 별도 Game connection/UPDATE6·8로 영속화한다. Bazaar INSERT와 한 transaction이 아니다.
- `CacheStorage.inl:130,158,192`: `flushInFlight`/generation은 같은 cache entry의 snapshot 중복/완료 순서를 막는다. AVAILABLE entry의 후속 변경은 가능하고 새 dirty가 남는다. 여기의 `Rollback`은 메모리 자원 환급이 아니라 최신 snapshot 재flush다. inventory/gold/listing 사이 원자성을 제공하지 않는다.
- `CacheFlush.cpp:116`의 inventory flush 완료만 WAL pin을 정산한다. 배송용 blob event ring → outbox CLAIM 순서는 존재하지만 등록 차감/gold를 묶는 증빙으로 재사용할 수 없다.

## 재현 가능한 위험 순서

1. INSERT가 DB에서 commit된 뒤 연결/응답이 끊김 → SQL 예외 → `affected=0` 유지 → item/gold 자동환급 → TRADING 매물과 환급 자원이 동시에 존재할 수 있다(`Handler_Bazaar.cpp:125–143`). 예외만으로 rollback 여부를 판단할 수 없다.
2. inventory 차감 WAL이 fsync되고 gold는 아직 메모리뿐인 때 crash → inventory 차감만 복구될 수 있다. INSERT가 성공했어도 수수료만 복원될 수 있고, INSERT 전 crash면 매물 없이 item만 사라질 수 있다.
3. 두 자원 flush 뒤 INSERT 전 crash → 자원은 차감됐지만 매물 없음. 반대로 INSERT 뒤 inventory WAL/flush와 gold flush 전에 crash → 매물은 있으나 일부 자원 차감이 남지 않을 수 있다.
4. inventory 차감이 슬롯을 비운 뒤 다른 ADD/배송이 슬롯을 채움 → fee 실패/INSERT 실패 환급이 capacity 때문에 BLOCKED. gold 환급도 중간 입금으로 overflow할 수 있다. 환급은 두 lock의 별도 연산이며 실패 시 현재 로그만 남는다.
5. worker Acquire 실패/비SQL 예외 → `DBWorker.h:93–119` onFailure는 현재 `Handler.h:34`의 메시지 Return만 실행 → 이미 차감된 자원, 응답 없음. 성공 응답/Return 이후 예외가 전파되면 동일 메시지의 후속 실패 처리에도 주의해야 한다.

## 멱등키·예약·새 table 없는 대안 평가

- `(seller, durableRequestKey)` UNIQUE와 동일 payload 검사/상태 조회는 **매물 INSERT 재시도 식별**에 필요하다. 동일 item/수량/가격 조회로 대체할 수 없다. 하지만 cache 선차감이 key INSERT 전에 반복되거나 crash로 gold만 복원되는 문제는 UNIQUE로 막지 못한다.
- bazaar 기존 행에 request key와 `REGISTERING` 상태를 추가해 먼저 예약하면 재조회 지점은 생긴다. 그러나 예약 → cache 차감 → TRADING 사이의 각 자원 차감 완료를 원자적·영속적으로 식별하지 못한다. 단순 단계 flag 역시 flag 기록 전후 crash와 자원 변경 사이 틈을 남긴다. 현 enum에는 예약 상태도 없고 MyList/검색/구매 경로의 숨김·거부도 수정해야 한다.
- 새 table 없이 기존 `game.characters_inventory`, `game.characters_currency`, `bazaar.bazaar`를 **같은 MySQL connection/transaction**으로 변경하는 후보는 이론적으로 가능하다. 기존 bazaar 행에 durable key/상태를 보관하고 blob은 C++에서 검증/수량 변경한 뒤 CAS 또는 잠긴 최신 행 기준으로 반영해야 한다. SQL에서 고정 blob offset을 임의 조작하는 방식은 제외한다.
- 그러나 현재 flush는 snapshot 전체를 무조건 UPDATE하므로 transaction row lock만 추가하면 늦은 stale snapshot이 등록 차감을 덮어쓴다. cache도 DB와 달라지고, 더 높은 LSN의 기존 WAL replay가 DB 변경을 되살릴 수 있다. 안전하려면 해당 캐릭터의 **모든** inventory/gold writer·배송/head전진·flush/eviction을 잠그거나 version 검사로 조정하고, 진행 중 flush drain·cache 반영/무효화·WAL 복구 fence까지 함께 정의해야 한다. 다중 서버 writer도 확인해야 한다.
- 실제 Game/Bazaar가 같은 MySQL instance인지, cross-schema transaction 권한이 있는지, 운영 writer 수는 이번 소스 읽기만으로 확정하지 않았다. `init.sql`의 InnoDB 선언은 실DB topology/권한 증거가 아니다. 분리 instance라면 위 단일 transaction 대안은 성립하지 않는다.
- 기존 buyer_outbox는 물품 **지급**과 inventory ring dedup용이다. 등록 차감·gold 예약을 그대로 표현하지 못하므로 반대 방향 용도로 끼워 넣지 않는다. 임의 서버 경제 규칙이나 범용 transaction framework도 추가하지 않는다.

## 좁은 구현 인계

1. `Handler_Bazaar.cpp::BazaarRegister`에서 request를 값 복사하고 메시지 소유권을 단일 finish에 모은다. 등록 전용 Enqueue/onFailure를 사용해 queued/rejected/worker failure 모두 종결 처리한다. 다른 handler/worker 전체를 리팩터링하지 않는다.
2. 상태 1은 ExecuteUpdate가 1을 반환한 경우만. SQL 실행을 시작한 뒤 SQL/일반 예외 및 예상 밖 반환은 **UNKNOWN=5**, 자동 item/gold 환급·SQL 재시도 없음. 예외 없이 명시적으로 0행을 반환한 경우는 삽입되지 않은 확정 실패이므로 기존 환급을 유지한다. 실행 전 확실한 enqueue/connection 거절도 환급 가능하며 두 환급 결과를 확인한다. 하나라도 실패하면 UNKNOWN으로 남기고 사실 로그를 남긴다. fee 실패 뒤 item 복원 실패도 확정 자원부족으로 숨기지 않는다.
3. 성공/실패/UNKNOWN 모두 응답 enqueue 시도와 message Return을 정확히 한 번만 수행한다. finish 이후 실패 callback 재진입이 이중 Return/환급을 만들지 않도록 등록 작업의 작은 종결 guard를 둔다. 응답 큐 거절은 기록하며 **전송 도달 보장**으로 표현하지 않는다. durable key가 없으므로 UNKNOWN 뒤 자동 재등록/자동 복구를 제공하지 않는다.
4. 예상 제품 파일: `CacheLib/Handler_Bazaar.cpp`, `CoreLib/MessageTypes.h`의 등록 status 주석, 필요 시 등록 전용 작은 결과/작업 helper, `CacheLib/Handler.cpp`의 정확 내부 요청 길이 검사. 검증은 `UnitTests/CacheLib/` 등록 전용 파일 하나. DB SQL·패키지·Unity 에셋·외부 wire 변경은 이 좁은 후보에 필요 없다.
5. 메모리 부족/프로세스 중단/기존 cache WAL 실패와 교차 자원 crash 원자성은 계속 미해결이다. 이 보완은 **불명 commit을 실패로 오판해 자원을 복제하는 기존 경로와 worker 무응답**을 줄이며 외부 등록 공개 조건을 충족하지 않는다.

## 최소 검증 두 경계와 복구

- 경계 A, 결과/환급: 주입 가능한 QUERY13 실행 경계에서 success1, commit 후 예외/비SQL 예외/예상 밖 반환은 UNKNOWN·추가 환급0·재실행0을 확인한다. 0행 확정 실패・확실한 실행 전 reject와 fee 실패에는 보유 자원만 정확히 환급하고 환급 실패를 UNKNOWN으로 보낸다. DB live/임의 seed가 아닌 fixture로 묶는다.
- 경계 B, 소유권/worker: queued 정상·enqueue 거절·connection 없음·worker exception·응답 큐 거절/finish 재진입을 한 표 기반 시나리오로 확인해 응답 **시도**1회와 Return1회, double refund0을 관찰한다. 이미 통과한 Unity/거래소 회귀와 실제 등록 입력은 반복하지 않는다. fixture를 실DB commit/crash 안전성 검증으로 보고하지 않는다.
- 좁은 후보는 실제 DB DDL/seed/데이터 변경이 필요 없다. 로컬 검증/독립 리뷰 뒤 PR 직전 Slack 승인 범위에 포함한다. 실행 서버 교체 여부는 Coordinator가 선택하며 이 분석에서 재시작하지 않았다.
- 코드 복구는 해당 후보 커밋 revert/기존 빌드로 정상 교체. UNKNOWN 기존 요청의 자원을 자동환급하거나 동일 매물 검색만으로 확정하지 않는다. 사실 로그와 DB/WAL 읽기 증거로 사람이 조정해야 하며 실제 데이터 보정은 별도 구체 승인 범위다.
- 외부 등록 후속은 durable 식별/상태조회 **및 자원·매물 원자성/재시작 수렴**을 한 후보로 검증한 뒤에만 착수한다. DDL이 필요한 후속은 로컬 SQL 후보까지 준비할 수 있으나 실제 공용DB 적용은 이번 작업에 포함하지 않는다.
