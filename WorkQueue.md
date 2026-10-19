# Work Queue Policy

## 1. 개요

스레드풀의 작업 Queue는 P-C(Producer-Consumer) 구조로 동작한다. Queue 선택 시 다음 항목을 각각 결정한다.  

- Queue 동기화: Mutex / Lock-free   
- Consumer 대기: Condition Variable / Semaphore / Tick polling  
- 포화 정책: 일반 queue / BackPressure queue
- 종료 정책: Drain / HardStop

`Blocking`, `Lock-free`, `BackPressure`, `Drain`은 서로 독립적인 정책이다. Worker의 실행 방식과 작업 특성에 맞춰 조합한다.  

서버가 직접 관리하는 worker dispatch queue만 다룬다.   

## 2. P-C 구조 기본 원칙

Producer는 작업을 queue에 publish한 뒤 consumer를 깨운다.

1. Push 성공 후에만 wake-up 신호를 보낸다.
2. Push 실패 시 wake-up 신호를 보내지 않는다.
3. Consumer는 wake-up 후 처리 조건을 다시 확인한다.
4. 대기 조건에는 queue 상태와 종료 상태를 함께 포함한다.
5. Drain은 producer 입력을 먼저 중단한 뒤 시작한다.

- **Condition Variable**은 notify 횟수를 저장하지 않는다. Queue와 running 상태를 같은 mutex로 보호하고 `wait(lock, predicate)`로 조건을 다시 확인한다.  
- **Semaphore**는 작업 수를 permit으로 관리한다. Producer는 push 성공 후 `release`, consumer는 `acquire` 후 pop한다. Queue에 publish된 작업 수와 permit 수가 일치해야 한다.
- **Tick worker**는 별도 wake-up이 필요 없다. 정해진 tick마다 queue를 확인하고 처리량 제한 안에서 작업을 소비한다.

## 3. Bounded Queue

`std::queue`는 별도 제한을 두지 않으면 계속 증가한다. DB 지연으로 consumer 처리량이 producer보다 낮아지면 다음 문제가 생긴다.

- 대기 작업과 처리 지연이 제한 없이 누적된다.
- Queue 확장 과정에서 동적 할당이 계속 발생해 메모리 사용량과 파편화가 증가할 수 있다.
- Push가 계속 성공하므로 drop, durable 기록, 상태 복구 같은 포화 정책을 적용할 시점이 없다.

Bounded Queue는 시작 시 고정된 공간만 할당하고 full이면 즉시 실패를 반환한다. Queue 크기와 backlog 증가를 제한하고, 호출부에서 작업 특성에 맞는 포화 정책을 적용할 수 있다.  
`LockFreeQueue`와 `RingQueue`는 모두 bounded queue다. Core hot path처럼 producer의 대기를 피해야 하면 `LockFreeQueue`를 사용한다. CacheFlush와 DBWorker는 Mutex/CV로 동시 접근을 보호하므로 단순한 `RingQueue`를 사용한다.   

## 4. Mutex / Lock-free

Queue 동기화 방식과 consumer wake-up 방식은 독립적이다. Mutex queue에 semaphore를 사용할 수 있고, lock-free queue에 Condition Variable을 사용할 수도 있다. 다만 Condition Variable은 대기 조건을 보호할 mutex가 필요하다.  

현재 서버에서 Mutex와 Lock-free를 나누는 핵심 기준은 producer의 enqueue 경로에서 blocking을 허용할지 여부다. Mutex queue는 다른 스레드가 mutex를 보유하고 있으면 producer도 lock 획득까지 대기한다. `LockFreeQueue`는 consumer가 보유한 mutex의 해제를 기다리지 않으며, queue가 full이면 즉시 실패를 반환한다. CAS 경쟁으로 재시도할 수는 있지만 mutex 대기로 producer thread가 block되는 구조는 아니다.  

이 구분은 consumer의 대기 방식과는 별개다. Lock-free queue도 semaphore를 결합하면 consumer는 작업이 없을 때 block할 수 있다.

현재 서버에서는 다음 조합을 사용한다.

| Queue 동기화 | Wake-up | Producer | 적용 기준 |
| --- | --- | --- | --- |
| Mutex queue | Condition Variable | Mutex 획득 중 대기 가능 | Queue, running, in-flight 상태를 한 임계 구역에서 관리 |
| lock-free queue | Semaphore | Full이면 즉시 실패 | worker를 재우고 producer enqueue는 non-blocking으로 유지 |
| lock-free queue | Tick polling | Full이면 즉시 실패 | sleep loop가 consumer 실행 시점을 결정 |

- Lock-free queue와 semaphore를 조합하면 producer는 queue push와 permit 발행만 수행한다. Consumer mutex를 기다리지 않으며, consumer는 작업이 없을 때 block된다.
- `BlockingQueue`의 Blocking은 consumer 대기를 의미한다. Producer는 queue 공간을 기다리지 않는다.
- Mutex/CV 조합은 queue 외 상태를 함께 관리할 때 사용한다. DBWorker는 DB 작업 queue와 running 상태를, CacheFlush는 queue와 in-flight 상태를 같은 mutex로 관리한다.

## 5. DrainableSemaphore

일반 semaphore는 permit 수만 관리하며 close 상태는 표현하지 않는다.   
일반 Semaphore는 Drain 처리에서 permit이 0되면 blocking 된다. 
Block된 consumer를 종료하고 Drain처리를 위해 `DrainableSemaphore`를 사용한다.  

`DrainableSemaphore`는 하나의 atomic 값에 closing flag와 permit count를 저장한다. Consumer 대기는 `atomic::wait/notify`로 처리한다.

- `Drain`: close 전에 발행된 permit을 모두 소비한 뒤 종료
- `HardStop`: close 확인 후 남은 permit을 소비하지 않고 종료

Drain은 `Stop()`과 producer push를 직렬화하지 않는다. 모든 producer의 push가 끝난 뒤 `Stop()`을 호출해야 한다. 전체 종료 순서는 [Graceful Shutdown](GracefulShutdown.md)을 따른다.

## 6. BlockingQueue

`BlockingQueue`는 `LockFreeQueue`와 `DrainableSemaphore`를 결합한 래퍼 클래스다.

- Push 성공 후 permit 발행
- Permit 획득 후 queue pop
- Queue full이면 producer에 `false` 반환
- Queue가 비면 consumer block
- `Drain` template 인자로 종료 정책 선택

Producer의 lock-free enqueue 경로를 유지하면서 consumer의 polling을 제거한다. `DrainableSemaphore`를 내부에서 관리하므로 worker마다 종료 protocol을 구현할 필요가 없다.

## 7. BackPressure Queue

BackPressure 정책은 Blocking/Non-Blocking 방식으로 나눠 적용한다.

- `BlockingBackPressureQueue`: Primary/defer queue + `DrainableSemaphore`
- `NonBlockingBackPressureQueue`: Primary/defer queue, tick 또는 batch polling

[Back Pressure Queue](BackPressure.md)

## 8. Worker별 적용

Core 측 worker는 gameplay와 session 처리를 담당해 hot path에 가깝다. Core worker가 producer인 queue는 enqueue 과정에서 대기하지 않는 방식을 우선한다.

Cache 측 worker는 DB와 외부 I/O 대기가 처리 시간을 지배한다. Hot path가 아니므로 짧은 Mutex 대기는 허용하고, queue와 종료 상태를 단순하게 관리하는 방식을 사용한다. 단, Core에서 Cache로 작업을 전달하는 Cache InMemory queue는 Core worker를 block하지 않아야 한다.

| Worker | Queue / Wake-up | 포화 정책 | Stop | 적용 이유 |
| --- | --- | --- | --- | --- |
| Zone | `LockFreeQueue` + game tick | Full 시 drop | HardStop | 고정 tick이 consumer 실행 시점을 결정하므로 별도 wake-up이 필요 없다. |
| Chat | `NonBlockingBackPressureQueue` + 15ms batch loop | Chat·Lifecycle drop, Zone Join·Leave defer | HardStop | Batch window 동안 dequeue가 block되면 안 된다. |
| Broadcast | `BlockingQueue` + Semaphore | Full 시 drop | HardStop | Idle worker는 block한다. Core hot path에서 enqueue하므로 producer는 대기하지 않는다. |
| NonZone | `BlockingBackPressureQueue` + Semaphore | Work drop, Disconnect defer | Drain | Idle worker는 block한다. Disconnect를 보존하기 위해 BackPressure를 적용한다. |
| Core InMemory | `RingQueue` + Mutex/CV | Full 시 거부 | HardStop | Cache 응답 전달 경로는 producer의 non-blocking을 요구하지 않아 Mutex/CV로 queue와 종료 상태를 함께 관리한다. |
| Cache InMemory | `BlockingBackPressureQueue` + Semaphore | Disconnect State Important, 나머지 Droppable | Drain | Core hot path에서 enqueue하므로 producer는 대기하지 않는다. Disconnect 시 생성한 최종 상태를 보존하기 위해 BackPressure를 적용한다. |
| CacheFlush | `RingQueue` + Mutex/CV | Full 시 실패 반환, dirty 유지 | Drain | Cache 내부 I/O worker이므로 Mutex/CV를 사용한다. Queue와 in-flight 상태를 함께 관리한다. |
| DBWorker Game/Bazaar | `RingQueue` + Mutex/CV | Full 시 거부 | Drain | Cache 내부 I/O worker이므로 Mutex/CV를 사용한다. 수락된 작업은 종료 시 모두 처리한다. |

CacheFlush와 DBWorker는 외부 DB가 막혔을 때 작업이 무한히 쌓이지 않도록 bounded `RingQueue`를 사용한다.

- CacheFlush가 full이면 오류를 남기고 `Failed`를 반환한다. 호출부는 `flushInFlight`를 풀고 dirty를 유지한다. evict 중인 항목은 `AVAILABLE`로 돌리고 LRU 위치도 복구한다. DB 쓰기에 실패하면 rollback 후 재시도하고, 즉시 재시도 한도를 넘으면 다음 dirty flush에서 다시 처리한다.
- DBWorker는 큐가 가득 찼거나 종료된 상태면 오류를 남기고 `Rejected`를 반환한다. 내부 cache load 호출부는 `DB_READING` 예약을 풀어 다음 조회에서 재시도할 수 있게 한다. 이미 수락한 작업이 실행 중 실패하면 실패 콜백으로 예약 상태나 소유 자원을 정리한다.
- 여기서 남기는 오류 로그에는 자동 재처리가 붙어 있지 않다. WAL·Outbox/Inbox와 Cache InMemory queue의 durable fallback은 별도 경로다. CacheFlush·DBWorker가 거부한 작업이 이쪽으로 자동으로 넘어가지는 않는다.

- IOCP는 Windows completion port와 `GetQueuedCompletionStatus`를 사용한다. SessionAuth는 libevent event loop를 사용한다. 공용 Work Queue 대상이 아니다.
- FlushDispatcher, Timer, PerfCollector, PingManager, WAL fsync thread는 주기 실행 thread이며 입력 queue가 없다.
- `ClientContext` send queue와 `ClientContextPool` flush queue는 IO 직렬화와 객체 재사용을 위한 내부 buffer다.

## 9. 선택 기준

1. 실행 시점이 고정 tick이면 lock-free queue를 polling한다.
2. worker가 tick 기반이 아니고 producer가 대기를 피해야 하면 lock-free queue와 semaphore를 사용한다.
3. Queue와 여러 상태를 함께 관리해야 하면 mutex와 Condition Variable을 사용한다.
4. 외부 I/O 지연으로 backlog가 누적될 수 있으면 bounded queue와 full 처리 정책을 함께 둔다.
5. 작업 중요도를 구분해야 하면 BackPressure Queue를 사용한다.
6. 수락된 작업을 완료해야 하면 Drain, 폐기할 수 있으면 HardStop을 사용한다.
