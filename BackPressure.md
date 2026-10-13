# Back Pressure

## 1. 개요

서버는 병목이 발생하면 일부 작업을 버려서 핵심 처리를 유지한다. 다만 세션 종료나 메모리 상태 저장처럼 유실할 수 없는 작업까지 같은 방식으로 처리할 수는 없다.
처음에는 일반 작업을 drop하고 중요한 작업만 호출부에서 재시도하였다. 이후 작업을 `Droppable`과 `Important`로 나누고, 중요한 작업의 처리 경로를 queue 내부 정책으로 옮겼다.
이 문서는 이 정책을 공통 queue로 구현한 과정과 Blocking, Non-Blocking 방식에서 달라지는 요구사항을 정리한다. Worker별 queue 선택 기준은 [Work Queue Policy](WorkQueue.md)를 따른다.

## 2. 도입 배경

기존에는 병목 처리 방식이 경로마다 달랐다.

- 일반 작업은 queue가 가득 차면 drop하였다.
- Disconnect는 `yield + 최대 재시도 횟수 + 실패 로그`로 처리하였다.
- WAL은 별도의 degraded mode를 사용하였다.

중요 작업마다 재시도와 실패 처리를 추가하면 같은 코드가 반복되고, 새로운 경로를 추가할 때 유실 정책을 빠뜨리기 쉽다. 이를 줄이기 위해 입력의 중요도를 나누고 primary queue와 defer queue를 사용하는 정책으로 일반화하였다.

`Important`는 먼저 처리한다는 뜻이 아니다. 병목 상황에서도 바로 버리지 않고 defer queue로 넘긴다는 의미다.

## 3. 공통 정책

두 queue는 다음 규칙을 공유한다.

1. 정상 상태에서는 모든 작업을 primary queue에 넣는다.
2. 병목 상태의 `Droppable` 작업은 버린다.
3. 같은 상태의 `Important` 작업은 defer queue에 넣는다.
4. Defer queue까지 가득 차면 durable log에 기록한다.

Primary와 defer는 모두 bounded queue다. Important 작업도 무한히 쌓지 않고, 병목 시 처리 방식을 queue 정책으로 통일한다.

두 queue 사이의 전역 FIFO는 보장하지 않는다. 순서가 중요한 작업은 sequence 검증이나 최신 상태 우선 처리처럼 별도의 수렴 규칙이 필요하다.

## 4. Non-Blocking 방식

`NonBlockingBackPressureQueue`는 primary와 defer에 `LockFreeQueue`를 사용한다. Producer와 consumer 모두 대기하지 않으며, queue가 비거나 가득 차면 즉시 결과를 반환한다.

### Normal

- 모든 입력을 primary queue에 넣는다.
- Primary queue가 가득 차면 degraded mode로 전환한다.

### Degraded

- Droppable 입력은 버린다.
- Important 입력은 defer queue에 넣는다.
- Consumer는 primary queue를 먼저 비운 뒤 defer queue를 처리한다.
- 두 queue가 모두 비면 normal 상태로 복구한다.

Non-Blocking consumer는 두 queue를 계속 확인하므로 비어 있는 시점을 기준으로 degraded 상태를 복구할 수 있다. 복구 전환과 늦은 defer publish가 겹칠 수 있어 normal 상태에서도 defer queue를 먼저 확인한다.

## 5. Blocking 방식

`BlockingBackPressureQueue`는 `LockFreeQueue`와 `DrainableSemaphore`를 결합한 BlockingQueue 구성을 사용한다. Producer의 enqueue는 대기하지 않고, consumer는 처리할 작업이 없으면 semaphore에서 block된다.

Primary와 defer가 하나의 semaphore를 공유하며, consumer는 permit을 획득한 뒤 defer queue를 먼저 확인한다. 종료 시에는 설정에 따라 남은 작업을 처리하는 Drain과 즉시 중단하는 HardStop을 선택한다.

Blocking 방식은 degraded 상태를 유지하지 않는다. Consumer가 block되는 구조에서 degraded 복구까지 관리하려면 queue publish, permit, 상태 전환을 함께 동기화해야 한다. 대신 primary queue가 가득 찬 순간에만 Droppable을 버리고 Important를 defer queue로 보낸다.

`Stop()`은 모든 producer의 입력이 끝난 뒤 호출한다. Stop과 Enqueue의 동시 실행 및 Stop 이후 Enqueue는 지원하지 않는다.

## 6. 서버 적용

Chat은 Non-Blocking 방식을 사용한다. 일반 Chat과 session lifecycle은 Droppable, Zone Join과 Leave는 Important로 분류한다. Primary와 defer 사이의 순서 역전은 session별 Zone sequence로 걸러낸다.

Cache InMemory와 NonZone은 Blocking 방식을 사용한다.

- Cache InMemory는 Disconnect에서 생성한 Character State 저장만 Important로 분류한다.  일반 변경 요청은 Droppable로 처리한다.
- NonZone은 Disconnect를 Important로 분류하고, defer까지 가득 차면 호출 스레드에서 직접 처리한다.
- 실제 캐릭터 상태 저장은 `StateManager -> Cache MQ` 경로에서 처리한다.

DBWorker와 CacheFlush는 BackPressure queue를 사용하지 않는다. Bounded `RingQueue`가 가득 차면 오류를 기록하고 호출부 상태를 복구한다. 자세한 worker 구성은 [Work Queue Policy](WorkQueue.md)에 정리한다.

## 7. 검증

[NonBlockingBackPressureQueue 테스트](UnitTests/BaseLib/NonBlockingBackPressureQueue.cpp)

- degraded 전환과 복구
- Droppable drop과 Important defer

[BlockingBackPressureQueue 테스트](UnitTests/BaseLib/BlockingBackPressureQueue.cpp)

- Droppable drop과 Important defer 우선 처리
- Drain과 HardStop

구현:

- [NonBlockingBackPressureQueue](BaseLib/NonBlockingBackPressureQueue.h)
- [BlockingBackPressureQueue](BaseLib/BlockingBackPressureQueue.h)
