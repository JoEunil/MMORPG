 # MMORPG GAME PROJECT

## 1. 프로젝트 개요
__목적__
- MMORPG 서버 구현을 통해 서버 관점에서 요구되는 기술과 개념을 이해한다.  
- 서버 운영과 관련된 핵심 기술을 직접 경험하고 습득한다.  

__목표__
- 클라이언트 로그인부터 사용자 접속 종료 및 재접속 로직까지, __End-to-End 데이터 파이프라인__ 을 설계하고 구현한다.  
- 프로젝트 전반의 서버 구조와 흐름을 직접 설계한다.

## 2. 전체 아키텍처
![이미지 로드 실패](images/architecture.png)
프로젝트는 클라이언트·서버·공용 라이브러리로 명확히 분리된 구조를 가진다.

클라이언트는 .NET 기반의 ClientCore 라이브러리에서 네트워크 로직을 처리하고, UI는 WinForms 테스트 후 Unity View로 대체할 수 있도록 MVVM 패턴을 적용했다.

서버는 기능별로 모듈화되어 있으며,

- __NetLibrary__는 네트워크 IO 및 네트워크 세션, 핑 처리,
- __CoreLib__는 게임 로직,
- __CacheLib__는 DB I/O와 메모리 캐시,
- __ExternalLib__는 로그 처리 및 Redis 기반 세션 인증을 담당한다.

외부 모듈로는  spdlog, hiredis, libevent, nlohmann(json), Mysql Connector C++를 사용하며, DB는 로그인 DB와 게임 DB로 분리하여 운영한다.
또한 인증 서버는 Redis에 임시 세션을 저장해 게임 서버 진입을 검증하고, 로그인 서버는 로그인 토큰을 발급해 인증 서버에 세션을 등록하는 역할을 수행한다.

## 3. 리팩토링 
README에서는 refactoring을 위주로 서술한다. 
주요 기능은 별도로 문서를 남겨놓았고 [문서](#5-문서)에서 확인할 수 있다.

### 3.1 Ping 루프 기반 세션 종료 리팩토링
#### 문제상황
초기 설계에서는 Ping Loop를 통한 세션 종료 로직이 CoreLib의 StateManager에 위치해 있었다.
이는 StateManager가 세션 매핑 정보를 통해 송신 대상을 결정할 수 있었기 때문이다.

그러나 이 구조에는 다음과 같은 문제가 존재했다.

- StateManager에서는 소켓을 직접 종료할 수 없다.
- 정상 종료 시에는 __소켓 종료 → 세션 종료__ 흐름이 보장되지만,  
FIN 패킷을 전송하지 못한 비정상 종료 상황에서는 __세션은 종료되지만 소켓은 제거되지 않는 문제__ 가 발생한다.
- TCP Keep-Alive 옵션을 통한 헬스 체크도 가능하지만,
	- 모든 연결에 대해 커널 타이머가 설정되어 비용이 크고
	- 세션 종료 원인을 애플리케이션 레벨에서 추적하기 어렵다

#### 수정사항
__구조 변경__  
NetLib에 PingManager를 추가하여 Ping/Pong 처리 및 세션 종료 판단을 담당  
송신 대상 참조는 NetLib의 SessionManager로 이동  
IOCP에 소켓 종료 요청을 전달하기 위한 전용 인터페이스 IAbortSocket 도입

__처리 방식__  
호출 스택 상에 상위 로직이 남아 있는 상태에서
소켓 종료가 발생하지 않도록 종료 지점을 명확히 분리하였다.

- PingManager
	- 전용 스레드에서 동작
	- IOCP 워커 스레드와 독립된 안전한 세션 종료 지점

- NetHandler
	- 패킷 파싱 이전 단계
	- ClientContext가 호출 스택에 포함되기 전 단계에서 종료 가능


#### 설계 결정 및 트레이드오프
Pong 패킷 처리를 위해 네트워크 레벨에서 한 단계의 패킷 분류 로직이 추가되었고,  
Ping 패킷 생성을 위한 전용 처리 경로가 필요해져 초기 복잡성은 증가하였다.  

하지만   
- 패킷 필터링 책임이 네트워크 계층으로 명확히 이동하였고,
- Ping 패킷은 PacketDispatcher에서 전용 경로로 처리하여 복잡성 증가를 해결하였다.

#### 참고
- 관련 PR: 
	- [Ping 루프 네트워크 로직으로 이동 #1](https://github.com/JoEunil/MMORPG/pull/1)
	- [timer를 통한 rtt 처리 최적화 및 Ping 전용 경로 생성 #9](https://github.com/JoEunil/MMORPG/pull/9)
- 관련 문서: 
	- [FloodDetect](FloodDetect.md) 
	- [PingLoop](PingLoop.md) 

### 3.2 채팅 기능 리팩토링
#### 문제상황
초기 설계에서는 Zone 단위 채팅만을 고려하여, Zone 내부 ID 관리와 채팅 처리를 하나로 묶어 Zone Thread에서 처리하였다.    
이 구조는 구현이 단순하다는 장점은 있었으나, 다음과 같은 문제가 있었다.  
- Zone Thread에 채팅 로직으로 인한 불필요한 부하가 추가됨
- Global 채팅, Whisper 채팅 등 채팅 기능 확장이 어려움  
#### 수정사항
__구조 변경__  
채팅 처리를 Zone Thread에서 분리하고, 전용 Chat Thread를 두는 구조로 리팩토링하였다.
- 모든 채팅 대상은 ChatID를 기준으로 식별하며, Session과 매핑하여 관리
- Session 관리가 일부 중복되지만, Session의 생명주기와 ChatID의 생명주기를 일치시키는 방향으로 설계
	- SessionManager의 처리 결과(Zone Change, Disconnect)를 이벤트로 전달받아 반영  
	
__처리 방식__  
채팅 전송, Session 추가/제거, Zone 이동과 같은 처리를 하나의 이벤트 구조체로 통합 
- 외부 호출자에 의한 lock이 발생하지 않도록 하였다.
#### 설계 결정 및 트레이드오프

- Lock-free 큐에 재시도(back-off) 정책은 적용하지 않음
	- 큐가 가득 찬 경우 채팅 이벤트는 드롭
	- Back-pressure 상황에서 재시도로 인한 CPU 소모가 채팅 유실보다 더 치명적이라고 판단
- World / Zone 채팅은 Batch 처리를 적용
	- 한 틱에서 여러 채팅을 묶어 패킷 전송
	- 채팅 지연은 증가하지만, 실시간성 요구가 상대적으로 낮은 채팅 특성상 허용 가능한 수준이라고 판단  

#### 시연

 ![GIF 로드 실패](images/ChatWhisper.gif)
> Whisper 채팅  
UI 복잡도를 줄이기 위해 Whisper 창을 하나로 통합하였다.  
채팅 메시지를 클릭하면 해당 대상을 Whisper 타겟으로 설정하여 메시지를 전송할 수 있다.

#### 참고
- 관련 PR: [채팅 기능 리팩토링 #11](https://github.com/JoEunil/MMORPG/pull/11)

### 3.3 ClientContext 및 SessionManager 리팩토링
#### 문제상황
[ClientContext 설명 문서](ClientContext.md)  

이전에 [3.1 Ping 루프 기반 세션 종료 리팩토링](#3.1-ping-루프-기반-세션-종료-리팩토링)에서 ClientContext의 복잡성을 인지하였고  
다음과 같이 이슈를 정리했었다. [Refactor God Object: ClientContext](https://github.com/JoEunil/MMORPG/issues/5)  

- 기존 구조에서 ClientContext가 세션 상태까지 관리하며 책임이 과도하게 집중됨
- NetHandler와 SessionManager에서 중복된 매핑이 존재
```cpp
    class SessionManager{
		...
        std::unordered_map<uint64_t, ClientContext*> m_contextMap;
		...
	}
```

그 원인은 SessionManager에서 session - context 매핑으로 세션 상태를 관리했기 때문이다.  
기능을 추가하면서 자연스럽게 Context에 세션 상태들을 추가하게 되었고, Context가 너무 복잡해졌다.  

ClientContext의 목적은 버퍼를 패킷으로 파싱하기 위해 클라이언트 개별로 TCP 수신 버퍼를 모아두는 역할이다.  
하지만 여기서 Session에 대한 책임까지 추가되었고, SessionManager에서는 실질적으로 Session 상태 관리 주체가 아닌 Context 매핑 자료구조 역할만 수행했다.  

#### 수정사항
__구조 변경__  
```cpp
    struct SessionShard {
        std::atomic_flag flag;
        std::unordered_map<SOCKET, SessionState> stateMap; // socket -> sessionState
        std::unordered_map<uint64_t, ClientContext*> contextMap; // session -> context
    };
    struct ReverseShard {
        std::atomic_flag flag;
        std::unordered_map<uint64_t, SOCKET> socketMap; // session -> socket 조회용도
    };

    class SessionManager{
        std::array<SessionShard, SESSION_SHARD_SIZE> m_sessionShard;
        std::array<ReverseShard, SESSION_SHARD_SIZE> m_reverseShard;
```
- session 상태에 대한 정보를 담는 SessionState 클래스 추가
- NetHandler에서 socket - ClientContext 매핑 제거
- ClientContext와 SessionState는 SessionManager 중심으로 관리
- NetHandler는 전달자 역할만 수행
- ClientContext는 버퍼 관리 책임만 수행, Zero-copy(least-copy) 전략 유지


__주의 사항__
- ClientContext는 게임 로직 큐로 들어가기 전까지 패킷 파싱 등을 수행하므로 의도적으로 포인터로 사용
	- [ClientContext로 인한 데드락 디버깅](SessionManagerDeadLock.md)
- ClientContext의 수명 관리는 Pool 내부 flush 정책과 workingCnt 기반으로 안전하게 처리됨


NetHandler 
- IOCP 수신 상태(ACCEPT, RECV, SEND)에 대한 처리.
ClientContext
- TCP 수신버퍼 패킷 만들기전 누적 버퍼 관리
PacketView
- 패킷에 해당하는 RingBuffer 포인터와 길이 등의 정보 담는 구조체

#### 설계 결정 및 트레이드오프
- SessionManager가 두 개의 맵을 관리하므로 구조가 복잡해짐
	- 단, 삽입/삭제를 SessionManager에 집중시켜 race condition과 데드락 가능성 최소화
- Zero-copy 전략 유지
	- ClientContext의 책임 분리가 명확해지고, Zero-copy로 인한 로직의 복잡성은 컨트롤 가능한 수준
- SessionManager에 SpinLock 적용
	- 조회가 매우 빈번한 hot-path에서 단순 Getter/Setter 접근이 대부분이므로, mutex 대신 SpinLock으로 ContextSwitching 비용 절감
	- PingLoop에서 호출되는 GetSessionSnapshot 내부 루프는 샤드를 적용해 한 샤드당 수백 단위이고,   
	map 접근도 캐시되어 SpinLock 비용에 큰 영향 없음
#### 참고
- 관련 PR: [Context, Session 리팩토링 #13](https://github.com/JoEunil/MMORPG/pull/13)
- 관련 PR: [데드락 해결 및 SessionManager 수정#14](https://github.com/JoEunil/MMORPG/pull/13)

### 3.4 AOI 적용
#### 문제상황
- 스킬 기능 및 상태 동기화 처리 기능을 추가하면서, Zone 내 유저 수가 많아지면 AOI 탐색 Depth가 깊어지고,
이는 Zone 스레드의 TPS에 영향을 미칠 위험성을 인지하였다.
- Zone을 Cell 단위로 나누어서 처리했고, Cell에 AOI를 적용하기 위해서는 기존의 패킷 처리 로직을 그대로 사용할 수 없었다.
#### 수정사항
__구조 변경__  
- Zone 내부를 Cell 단위로 분리하여 패킷 전파 단위 축소
	- 기존: zone 유저수 x zone 유저수 x 패킷 필드
	- Cell 적용 후: Cell 유저수 x Cell 유저수 x 패킷 필드
- AOI 처리 시 개별 패킷을 보내는 대신 Chunk 단위로 묶어 전송
- overlapped 구조체를 일부 수정하고, 별도의 Send 메서드를 추가하여 Chunk 전송 처리

__처리 방식__  
Chunk 단위 전송 처리 방식  
- 기존 Packet 클래스를 그대로 사용
- 패킷 헤더는 기존 경로로 처리
- 패킷 바디는 각 Cell을 Chunk 단위로 분리 후 합쳐서 전송
- overlapped 구조체에서 wsaBuf를 vector에 담아 WSA Send call 한 번으로 패킷 전송 가능.
- 현재 Grid 기반 AOI를 사용하며, 각 Cell의 AOI 단위는 미리 정의되어 있음

#### 설계 결정 및 트레이드오프
- 기존 3.3 리팩토링에서 Chunk 단위 전송을 고려했으나, 당시에는 필요성 낮음 판단 → 보류
- Cell, AOI 적용
	- 패킷 크기 감소, Skill 처리 탐색 범위 감소
	- 대역폭 계산  
	```패킷 크기 x 전송 대상 수 = (패킷 필드 1개 x 전송 대상) x 전송 대상 ```
	- 패킷 전송량은 전송 대상 수의 제곱에 비례
	- 따라서 Cell 단위 분리는 대역폭 절감 효과가 매우 큼
	- AOI 적용을 통해 예측 가능한 시야 범위 정보를 미리 로드하고, 불필요한 전송을 줄일 수 있음
- Chunk 단위 전송 처리 로직 추가 
	- 코드 복잡도 상승, 
	- 패킷 절감 효과 발생

#### 시연

 ![GIF 로드 실패](images/BeforeAOI.gif)
>  AOI 적용 전 임시로 Cell 단위로 처리한 상태

 ![GIF 로드 실패](images/AfterAOI.gif)
>  AOI 적용 후  
*분홍색 선은 Cell 경계를 나타낸 것이다.
#### 참고
- 관련 PR: [grid 기반 AOI 적용 #20](https://github.com/JoEunil/MMORPG/pull/20)

## 4. 느낀점 및 추후 개선이 필요한 점
__게임 서버에서 성능 우선순위__
1. 네트워크
	- AOI
	- 패킷 최소화
	- Full 패킷 줄이기
	- Delta 패킷 사용
	- Batch 처리
2. CPU
	- AOI
	- Zone 전용 스레드 코어 고정 -> 캐시 효율 증가
	- SpinLock 적용 -> contextSwitching 비용 감소
	- Lock free 자료구조 적용 -> mutex 잠금 비용 감소
3. 메모리
	- MMO 서버에서 메모리는 문제가 되지 않는다.
		- Network IO나 CPU 스케줄링이 먼저 병목 발생
		- 메모리는 대부분 예측 가능하게 소비됨
	- 문제가 되는 경우: 메모리 누수, 객체 풀 설계 오류

__프로젝트를 통해 얻은 인사이트__
- Lock-free 자료구조를 직접 구현하고 디버깅하며 atomic 변수, mutex, memory_order에 대한 이해 향상
- 멀티스레드 환경에서 스레드 동기화와 TPS 처리 구조 이해

__추후 개선 사항__
- 현재 Zone 전환 로직에서 모든 작업에 mutex 잠금을 사용 → Zone별 lock-free 작업 큐 도입으로 mutex 제거
- 서버 성능 모니터링을 위해 메트릭 수집기 추가 필요
- 스킬, 몬스터 데이터는 데이터 드리븐으로 변경. 지금은 하드코딩으로 처리하는 상태.

## 5. 문서

### 개념
- [memory_order](memory_order.md)
- [LockFreeQueue](LockFreeQueue.md)
- [IOCP와 epoll](IOCP&epoll.md)
- [로그 구조화](StructuredLogging.md)
- [TripleBuffer](TripleBuffer.md)

### 설계
- [네트워크 Flood 탐지](FloodDetect.md) 
- [Ping 처리](PingLoop.md) 
- [ClientContext](ClientContext.md) 
- [Skill](Skill.md) 
- [Monster](Monster.md) 
- [모니터링](Monitoring.md) 
- [더미 클라이언트 테스트](DummyTest.md) 
- [서버 클라이언트 동기화 처리 전략(Snapshot)](Snapshot.md) 
- [서버, 클라이언트 틱 처리](Tick.md) 

### 디버그
- [SessionManager 데드락](SessionManagerDeadLock.md)
- [LockFreeQueue 디버그](LockFreeQueueDebug.md)
- [ContextPool memory_order 디버그](MemoryOrderDebug.md)
