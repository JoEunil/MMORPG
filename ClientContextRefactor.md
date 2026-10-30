# ClientContext 및 SessionManager 리팩토링

## 1. 개요
이 문서는 ClientContext에 집중되던 책임을 분리하고, 세션 관리 구조를 재정비한 리팩토링 과정과 그 결과를 설명한다.

## 2. 문제상황
[ClientContext 설명 문서](ClientContext.md)  

이전에 [Ping 루프 기반 세션 종료 리팩토링](PingRefactor.md)에서 ClientContext의 복잡성을 인지하였고  
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
    struct SessionEntry {
        SessionState state;
        ClientContext* context;
    };
    struct SessionShard {
        std::mutex mutex;
        std::unordered_map<SOCKET, SessionEntry> sessions; // socket -> 상태와 Context
    };
    struct ReverseShard {
        std::mutex mutex;
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
- ClientContext는 Pool의 flush 시점에 `workingCnt == 0`, `pendingIOCnt == 0`, 종료 통지 완료를 모두 확인한 뒤 재사용한다. 종료 통지 전의 마지막 PacketView 반환과 취소된 RECV completion도 수명 조건에 포함한다.


NetHandler 
- IOCP 수신 상태(ACCEPT, RECV, SEND)에 대한 처리.
ClientContext
- TCP 수신버퍼 패킷 만들기전 누적 버퍼 관리
PacketView
- 패킷에 해당하는 RingBuffer 포인터와 길이 등의 정보 담는 구조체

## 4. 설계 결정 및 트레이드오프
- socket별 SessionState와 ClientContext를 한 항목으로 관리해 중복 조회와 두 맵의 일치 조건을 제거
- session → socket 역방향 맵은 송신과 PONG 처리의 조회에 사용
	- 등록·해제 시 정방향·역방향 샤드의 mutex를 `std::scoped_lock`으로 함께 잠가 두 맵을 일관되게 갱신
	- 중복 등록과 역방향 매핑 불일치는 assertion으로 검사하고, 연결 종료·오래된 세션 ID·Context 풀 고갈은 정상 실패로 처리
- Zero-copy 전략 유지
	- ClientContext의 책임 분리가 명확해지고, Zero-copy로 인한 로직의 복잡성은 컨트롤 가능한 수준
    
## 5. 참고
- 관련 PR: [Context, Session 리팩토링 #13](https://github.com/JoEunil/MMORPG/pull/13)
- 관련 PR: [데드락 해결 및 SessionManager 수정#14](https://github.com/JoEunil/MMORPG/pull/14)
