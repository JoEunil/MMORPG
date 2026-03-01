# ClientContext 및 SessionManager 리팩토링

## 1. 개요
이 문서는 ClientContext에 집중되던 책임을 분리하고, 세션 관리 구조를 재정비한 리팩토링 과정과 그 결과를 설명한다.

## 2. 문제상황
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

## 3. 수정사항
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

## 4. 설계 결정 및 트레이드오프
- SessionManager가 두 개의 맵을 관리하므로 구조가 복잡해짐
	- 단, 삽입/삭제를 SessionManager에 집중시켜 race condition과 데드락 가능성 최소화
- Zero-copy 전략 유지
	- ClientContext의 책임 분리가 명확해지고, Zero-copy로 인한 로직의 복잡성은 컨트롤 가능한 수준
- SessionManager에 SpinLock 적용
	- 조회가 매우 빈번한 hot-path에서 단순 Getter/Setter 접근이 대부분이므로, mutex 대신 SpinLock으로 ContextSwitching 비용 절감
	- PingLoop에서 호출되는 GetSessionSnapshot 내부 루프는 샤드를 적용해 한 샤드당 수백 단위이고,   
	map 접근도 캐시되어 SpinLock 비용에 큰 영향 없음
    
## 5. 참고
- 관련 PR: [Context, Session 리팩토링 #13](https://github.com/JoEunil/MMORPG/pull/13)
- 관련 PR: [데드락 해결 및 SessionManager 수정#14](https://github.com/JoEunil/MMORPG/pull/14)
