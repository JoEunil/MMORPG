# Ping Loop

## 1. 개요
본 문서는 네트워크 계층에서 Ping/Pong을 이용한
세션 생존 여부 판단과 강제 종료(Kill) 처리의 설계 의도를 설명한다.

특히 IOCP 워커 스레드, NetHandler, ClientContext 간의
호출 스택 관계로 인해 발생할 수 있는 구조적 위험과,
이를 회피하기 위한 Ping loop 기반 세션 종료 전략을 다룬다.

## 2. Ping / Pong 처리 개요

- Ping은 주기적으로 전송되어 세션의 생존 여부를 확인한다.
- Pong 수신 시 세션은 정상 상태로 간주되며,
  Ping 누적 카운트는 초기화된다.
- 일정 횟수 이상 Pong 미수신 시 세션은 종료 대상이 된다.
- Pong 수신 시 RTT를 측정하여 다음 Ping 패킷에 담아 클라이언트에서 나타낼 수 있도록 한다.

## 3. ClientContext의 호출 스택 특성

IOCP 워커 스레드에서 패킷을 수신한 이후,
NetHandler와 ClientContext를 반드시 거치며,
Core Dispatcher에 Enqueue 되기 전까지
ClientContext는 호출 스택 상에 존재한다.

즉, IOCP 워커 스레드의 실행 흐름 내에서는
ClientContext가 여전히 활성 상태로 사용 중일 수 있다.

## 4. 동일 실행 흐름에서의 세션 종료 위험성

IOCP 워커 스레드와 동일한 실행 흐름에서
AbortSocket()을 호출하는 것은 바람직하지 않다.

다음과 같은 호출 경로가
동일 호출 스택 또는 인접한 실행 흐름에서 발생할 경우,
치명적인 문제가 발생할 수 있다.

ClientContext
  -> AbortSocket (IOCP)
       -> NetHandler
            -> ClientContext

이와 같은 구조에서는 다음과 같은 문제가 발생할 수 있다.

- lock 순서 꼬임
- 이미 점유 중인 버퍼에 대한 해제 시도
- ClientContext 수명 관리 실패

내부적으로 즉시 kill 되지 않도록 방어 로직은 존재하지만,
구조적으로 이러한 경로를 허용하는 것 자체가 바람직하지 않다.

## 5. Ping Loop 기반 세션 종료 전략

위와 같은 문제를 회피하기 위해,
ForEachShard()에서는 ClientContext의 상태만 조회한다.

실제 AbortSocket / Disconnect 처리는
NetHandler 및 IO 스택과 완전히 분리된
Ping loop 전용 스레드에서 수행한다.

Ping loop는 IOCP 워커 스레드와 독립적으로 동작하며,
상위 호출 스택의 자원을 점유한 상태에서
세션 종료가 발생하지 않도록 하기 위한
의도적인 설계 선택이다.

## 6. 요약

- ClientContext는 IOCP 워커 스레드의 호출 스택에 포함될 수 있다.
- 동일 실행 흐름에서의 세션 종료는 구조적으로 위험하다.
- Ping loop는 이를 위한 전용 Killer 스레드 역할을 수행한다.

## 7. 참고
- [PingManager.h](NetLibrary/PingManager.h)
- [PingManager.cpp](NetLibrary/PingManager.cpp)
