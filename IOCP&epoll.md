# IOCP와 epoll 비교

## 1. 개요
이 문서는 Windows의 IOCP와 Linux의 epoll 개념을 정리하고, 각각의 장단점을 비교한다.  

## 2. 논블로킹 소켓
논블로킹 소켓이란, I/O 함수 호출 시 블로킹하지 않고 즉시 반환되는 소켓을 의미한다.  
- Send: 소켓이 준비되지 않은 상태인 경우, Send는 블로킹하지 않고 실패를 반환
- Recv: 소켓에 읽기 가능(TCP 수신)이 발생하면 select, poll, epoll을 사용하여 처리

```cpp
char buf[1024];
int n = recv(sock, buf, sizeof(buf), 0);
if (n == -1 && errno == EWOULDBLOCK) {
    // 데이터 없음, 나중에 다시 시도
}
```
- 소켓을 논블로킹 모드로 설정하면 recv/send 호출 자체는 동기적이다.
- 호출 시점에 데이터가 준비되지 않으면 바로 반환 → -1 + EAGAIN/EWOULDBLOCK
- 즉, 커널이 pending 상태를 관리하지 않고, 애플리케이션에서 재시도나 이벤트 기반 처리 필요

## 3. 리눅스의 비동기 IO
리눅스는 모든 시스템 자원을 파일 디스크립터(FD)로 관리한다.  
소켓도 FD로 관리되며, 비동기 I/O 처리는 __논블로킹 소켓__을 기반으로 한다.  

### epoll
- select/poll의 진화 버전
- __이벤트 기반__ 처리
  - 준비된 FD만 커널이 반환 → 순회 필요 없음
- O(1) 처리 가능 → 수만 명 동시 연결 처리 가능
- 일반적으로 최대 수만 명 이상 동시 연결 가능

## 4. windows의 비동기 IO
### IOCP와 OverlappedIO
- OVERLAPPED 구조체로 비동기 I/O 요청  
- I/O 완료 시 커널이 **completion queue**에 이벤트 삽입  
- 스레드는 `GetQueuedCompletionStatus()`로 큐에서 이벤트를 꺼냄 → O(1) 처리
- I/O 처리 함수자체가 비동기적으로 동작하여 소켓 자체는 동기 소켓을 사용.
  - WsaSend, WsaRecv
 
## 5. IOCP와 epoll의 차이
epoll: 논블로킹 소켓 특성상 Send 호출 시 송신 버퍼가 가득 찬 경우 EAGAIN/EWOULDBLOCK이 발생한다..
→ 애플리케이션이 재시도 로직을 구현해야 함.

IOCP: OVERLAPPED 구조체를 통한 비동기 I/O 관리가 필요하다.
→ Send 호출 자체가 비동기적이므로 재시도 로직이 불필요하며, 커널이 내부 송신 큐를 관리하여 거의 실패하지 않는다.
Overlapped 구조체 관리가 복잡하고, 플랫폼 종속성 있음.

*게임 서버의 플랫폼 종속성은 무시 가능하고, Overlapped 구조체 관리 가능하다면 IOCP가 epoll에 비해 장점을 가진다.
