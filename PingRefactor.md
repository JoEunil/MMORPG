# Ping 루프 기반 세션 종료 리팩토링

## 문제상황
초기 설계에서는 Ping Loop를 통한 세션 종료 로직이 CoreLib의 StateManager에 위치해 있었다.
이는 StateManager가 세션 매핑 정보를 통해 송신 대상을 결정할 수 있었기 때문이다.

그러나 이 구조에는 다음과 같은 문제가 존재했다.

- StateManager에서는 소켓을 직접 종료할 수 없다.
- 정상 종료 시에는 __소켓 종료 → 세션 종료__ 흐름이 보장되지만,  
FIN 패킷을 전송하지 못한 비정상 종료 상황에서는 __세션은 종료되지만 소켓은 제거되지 않는 문제__ 가 발생한다.
- TCP Keep-Alive 옵션을 통한 헬스 체크도 가능하지만,
	- 모든 연결에 대해 커널 타이머가 설정되어 비용이 크고
	- 세션 종료 원인을 애플리케이션 레벨에서 추적하기 어렵다

## 수정사항
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


## 설계 결정 및 트레이드오프
Pong 패킷 처리를 위해 네트워크 레벨에서 한 단계의 패킷 분류 로직이 추가되었고,  
Ping 패킷 생성을 위한 전용 처리 경로가 필요해져 초기 복잡성은 증가하였다.  

하지만   
- 패킷 필터링 책임이 네트워크 계층으로 명확히 이동하였고,
- Ping 패킷은 PacketDispatcher에서 전용 경로로 처리하여 복잡성 증가를 해결하였다.

## 참고
- 관련 PR: 
	- [Ping 루프 네트워크 로직으로 이동 #1](https://github.com/JoEunil/MMORPG/pull/1)
	- [timer를 통한 rtt 처리 최적화 및 Ping 전용 경로 생성 #9](https://github.com/JoEunil/MMORPG/pull/9)
- 관련 문서: 
	- [FloodDetect](FloodDetect.md) 
	- [PingLoop](PingLoop.md) 