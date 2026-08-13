# Graceful Shutdown & 자원 정리 순서

## 1. 개요

graceful shutdown은 서버 종료 시 진행 중인 작업을 유실 없이 마무리하고, 자원을 안전한 순서로 정리하는 것을 말한다.  
이 문서는 종료 파이프라인, ASan 부하 테스트 중 검출·수정한 종료 버그, 그리고 그 과정에서 확립한 자원 정리 규칙을 정리한다.  

> 메모리 안전성·누수 검증 수치는 [ASAN.md](ASAN.md)에 별도로 정리했다.

## 2. 배경

원래 종료 시 캐릭터 상태를 DB에 안전하게 저장하는 graceful shutdown 경로를 설계해 두었으나, 이후

- 로거를 전역(global) `unique_ptr`로 전환
- DB 워커(`DBWorker`)를 나중에 추가

하는 과정에서 종료 경로가 흐트러졌고, graceful shutdown이 잠시 방치된 상태가 되었다.
이후 ASan([ASAN.md](ASAN.md))으로 더미 클라이언트 2000명 부하 테스트를 돌리면서 종료 시 발생하던 크래시/데이터 유실을 하나씩 잡아 자원 정리 순서를 다시 정립했다.

## 3. 종료 파이프라인 (`MainServer/main.cpp`)

종료 처리는 **입력 중단 → 남은 입력 처리 → 내부 상태 저장 및 정리 (DB 쓰기 작업 생성) → DB 반영** 순서를 따른다.  
뒤 단계가 앞 단계의 결과에 의존하므로, 이 순서가 지켜져야 유실이 없다.  

이 순서의 근거는 파이프라인이 producer-consumer 체인이라는 데 있다 — net(입력 생산) → core(입력 소비·DB write 생산) → cache(DB write 소비).   
각 P-C 경계에서 **producer(상류)를 먼저 종료**해야, consumer(하류)가 "더 이상 새 입력이 없는" 상태에서 이미 받은 것만 drain하고 안전하게 끝낼 수 있다.  
순서가 뒤집히면 멈춘 큐로 producer가 계속 밀어넣어 유실되거나, 새 입력이 끊이지 않아 drain이 끝나지 않는다.  

> P-C 역전이 두 곳 있는데, 둘 다 유실 없이 처리했다.
> - core → net (send): net은 core에 대해 recv의 producer이자 send의 consumer라 cleanup을 둘로 쪼갠다. recv는 `net.CleanUp1()`로 먼저 끊고, send를 소비하는 IOCP는 `net.CleanUp2()`로 core 정리 뒤에 내린다 → 분리로 역전을 애초에 피함.
> - cache → core (응답): consumer(core)가 producer(cache)보다 먼저 정리되지만, core 큐의 `m_running` 체크로 방어. 종료 중 도착하는 응답은 버려도 되므로(나가는 클라이언트 대상) 무해.

```cpp
net.CleanUp1();   // 1) 입력 중단     : iocp 수신 중단, ping 정지
core.CleanUp1();  // 2) 시뮬레이션 정지: zoneThreadSet(틱), broadcast, chat 정지
core.CleanUp2();  // 2)+3) 남은 입력 처리(nonZone/recvMQ drain) → 내부 상태 저장(stateManager.CleanUp)
net.CleanUp2();   //    네트워크 인프라 종료: iocp, NetTimer 정지 
cache.CleanUp();  // 4) DB 반영       : cache recvMQ drain → dispatcher/flush → dbWorker drain → WAL

// 로거는 스레드풀이 살아있을 때 먼저 파괴하고, 그 다음 spdlog 정지
Core::sysLogger.reset();
Core::gameLogger.reset();
Core::errorLogger.reset();
Core::perfLogger.reset();
spdlog::shutdown();
```

### 단계별 목적

| 단계 | 호출 | 목적 |
|---|---|---|
| 1. 입력 중단 | `net.CleanUp1()` | `iocp.StopReceive()`로 수신 재게시를 멈춰 클라이언트 패킷 유입 차단, ping 정지. 이후 처리 대상이 고정된다 |
| 2. 남은 입력 처리 | `core.CleanUp1()` → `core.CleanUp2()` 앞부분 | 시뮬레이션 루프(zone tick·broadcast·chat) 정지 후, 이미 수신되어 큐에 쌓인 요청을 `recvMQ.Stop()`이 drain하며 처리 → 내부 상태를 최신으로 만든다 |
| 3. 내부 상태 저장 | `core.CleanUp2()`의 `stateManager.CleanUp()` | 메모리에 올라가 있는 캐릭터 상태를 순회해 영속 경로(cache 큐)로 넘긴다. |
| 4. DB 반영 | `cache.CleanUp()` | cache 큐 drain → dirty 캐시 flush → `dbWorker` drain(실제 DB write) |

### 캐릭터 상태 저장 프로세스 

캐릭터 상태는 메모리에만 존재하는 휘발성 상태라 종료 시 반드시 DB로 내려야 한다.  
3단계에서 큐에 넣고 4단계에서 실제로 기록되며, 각 단계의 큐/워커가 drain하므로 유실이 없다.   

```
[3. 내부 상태 저장]  stateManager.CleanUp()            (core.CleanUp2)
                       → cache recvMQ.EnqueueMessage()  (이 시점 cache 살아있음)
[4. DB 반영]         cache recvMQ.Stop() → drain        (cache.CleanUp)
                       → handler->Process()
                         → dbWorkerGame.Enqueue()       (DB write 작업)
                     dbWorkerGame.Stop() → drain         → 실제 DB write 후 종료
```

- 순서 보장: 저장 메시지를 넣는 시점(3단계)엔 cache가 살아있고, 소비(4단계)는 그 뒤에 온다.  
- 유실 방지: recvMQ·dbWorker 모두 Stop 시 큐를 drain하고, `Stop()`의 `join()`이 drain 완료까지 블록한다.  


## 4. 종료 시 발견·수정한 문제들

ASan 부하 테스트를 돌리며 발견한 종료 관련 문제들. 검출 수단은 ASan(메모리 오류)과 종료 로그 관찰이 섞여 있어 각 항목에 표시했다.

### 1. `Stop()` 이중 호출 + 로거 파괴 후 접근  — *ASan: access-violation*

`~Initializer()`가 CleanUp1/CleanUp2를 호출하는데, main에서 이미 명시적으로 CleanUp을 부른 뒤라 **Stop이 두 번 실행**됐다. 
두 번째 호출은 `spdlog::shutdown()`·`sysLogger.reset()` 이후(스코프 종료 시 소멸자)라 이미 파괴된 전역 로거를 역참조 → access-violation.  

- 증상: `ZoneThreadSet::Stop`/`CorePerfCollector::Stop`에서 `ILogger::LogInfo` 널 역참조  
- 조치: 각 Stop을 idempotent + 로거 null-safe로 변경  

```cpp
void Stop() {
    if (!m_running.exchange(false, std::memory_order_relaxed))
        return;                 // 이미 정지됨 → 재호출 무시
    // ... join ...
    if (sysLogger)              // 로거 파괴 이후 소멸자 경로 방어
        sysLogger->LogInfo(...);
}
```

### 2. 소멸 순서로 인한 UAF (`Net::Initializer`) — *ASan: heap-use-after-free*

멤버는 **선언 역순으로 소멸**한다. `overlappedExPool`이 `packetPool`/`bigPacketPool`보다 먼저 선언돼 있어, 풀이 먼저 파괴된 뒤 overlappedExPool이 소멸하며 물고 있던 in-flight 패킷을 이미 죽은 풀에 반납하려다 Use-After-Free 가 발생했다.

- 의존 관계: `X`가 `Y`의 자원을 보유하면 `Y`가 `X`보다 나중에 소멸해야 함 (= 먼저 선언)
  - `overlappedExPool`(STOverlappedEx) → 패킷 보유(스마트 포인터)
  - `clientContextPool`(ClientContext) → overlappedExPool의 overlapped 보유 (Send Queue)
- 필요한 선언 순서: packetPool, bigPacketPool → overlappedExPool → clientContextPool
- 조치: 두 패킷 풀을 `overlappedExPool` 앞으로 이동

### 3. 로거 파괴 시 `spdlog` async flush 에러 — *spdlog 에러 로그*

"async flush: thread pool doesn't exist anymore" 에러의 근본 원인은 **`External::Logger::~Logger()`가 인스턴스마다 `spdlog::shutdown()`을 호출**한 것.  
로거 4개(sys/game/error/perf)가 하나의 전역 스레드풀을 공유하는데, 첫 로거가 파괴되며 풀을 내려버리면 나머지 로거의 flush가 죽은 풀을 참조한다.  
(에러가 [system]↔[game]으로 옮겨다닌 것도 "두 번째로 파괴되는 로거"가 바뀌었기 때문.)

- 조치 1: `~Logger()`에서 `spdlog::shutdown()` 제거 — 소멸자는 자기 로거만 flush.  
- 조치 2: 전역 풀 종료는 main에서 모든 로거 reset 후 한 번만 호출. 또한 로거 reset을 `spdlog::shutdown()`보다 먼저 두어 async 로거가 살아있는 풀로 flush하도록 함.  

### 4. DB 워커가 종료 시 큐를 버려 캐릭터 저장 유실 — *에러 로그*

`DBWorker::ThreadFunc`가 `m_running`을 false로 보면 큐에 작업이 남아도 즉시 break → 종료 시점에 enqueue된 캐릭터 상태 DB write가 **처리 전에 폐기**됐다. (ASan이 아니라 종료 시 남은 error log로 발견)  
- 조치: 큐가 비고 running=false일 때만 종료하도록 변경 → 남은 작업 전부 처리 후 exit

```cpp
while (true) {
    std::function<void(T*)> work;
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this]{ return !m_queue.empty() || !m_running.load(); });
        if (m_queue.empty()) break;   // running=false로 깨어났고 큐도 비었음 → drain 완료
        work = std::move(m_queue.front());
        m_queue.pop();
    }
    // process(work)
}
```

`Stop()`이 `join()`으로 블록하므로 main 스레드는 워커의 drain 완료를 기다린다.
running=false 이후엔 producer(recvMQ, dispatcher)가 먼저 멈춰 새 작업이 안 들어오므로
큐는 단조 감소 → 반드시 비게 되어 종료가 보장된다.

> 참고: `DBWorker::Enqueue`는 `m_running`을 검사하지 않는다. 따라서 dbWorker.Stop() 이후 enqueue되는 작업이 있는지 주의해야 한다.

## 5. 종료 처리에서 지킨 것

- Stop/CleanUp은 idempotent하게 만들었다. `running.exchange(false)`로 재호출을 무시해서 명시적 호출 경로와 소멸자가 겹쳐도 안전하다.
- 소멸자 경로에서는 전역 로거가 이미 reset됐을 수 있다. `if (logger)` 가드를 둔다.
- 로거는 spdlog 스레드풀보다 먼저 파괴한다. async 로거가 살아있는 풀로 flush해야 하기 때문이다.
- 자원을 보유하는 쪽(소비자)을 먼저 선언해 먼저 소멸시키고, 자원을 제공하는 풀을 나중에 선언한다. 멤버 선언 순서로 강제되므로 순서를 바꿀 때는 주석을 남긴다.
- 종료 순서는 producer-consumer 관계로 결정된다. 파이프라인(net → core → cache, 그리고 내부의 recvMQ → dbWorker)의 각 경계에서 상류를 먼저 멈춰 새 입력을 끊고, 하류가 남은 것을 drain한 뒤 종료한다. 뒤집히면 유실이나 무한 대기가 된다.
- 큐·워커마다 hard stop(남은 것 버림)인지 graceful drain(비울 때까지 처리)인지 명시한다. DB write처럼 유실이 곧 데이터 손실인 경로는 반드시 drain이다.
