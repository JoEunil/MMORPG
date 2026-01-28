# MMORPG GAME PROJECT

## 1. 프로젝트 개요

## 2. 전체 아키텍처

## 3. 리팩토링 
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

## 4. 문서

### 개념
- [memory_order](memory_order.md)
  메모리 재배치 개념, memory_order 옵션 종류, SpinLock 적용 예시

### 설계
- [FloodDetect](FloodDetect.md) 
- [PingLoop](PingLoop.md) 
