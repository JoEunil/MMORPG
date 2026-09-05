# IOCP와 epoll 비교

## 1. 개요

이 문서는 Windows의 IOCP와 Linux의 epoll을 비교한다.  

두 기술 모두 적은 수의 스레드로 많은 연결을 처리하기 위한 이벤트 기반 모델이지만, 이벤트가 의미하는 내용이 다르다.  

- `epoll`: 소켓이 I/O 가능한 상태가 되었음을 통지하는 **readiness 기반 모델**  
- `IOCP`: 요청한 I/O 작업이 완료되었음을 통지하는 **completion 기반 모델**  

## 2. 동기·비동기와 블로킹·논블로킹

두 개념은 서로 구분해야 한다.

- 블로킹/논블로킹: 함수 호출 시 작업을 즉시 처리할 수 없을 때, 호출 스레드를 대기 상태로 전환하는지에 따라 구분한다.
- 동기/비동기: 작업 요청과 완료 결과 전달이 하나의 호출 안에서 이루어지는지, 서로 분리되어 나중에 완료 결과를 전달받는지에 따라 구분한다.

### 논블로킹 소켓

논블로킹 소켓에서는 `recv()`나 `send()`를 즉시 처리할 수 없을 때 호출 스레드를 재우지 않고 바로 반환한다.

```cpp
char buf[1024];

int n = recv(sock, buf, sizeof(buf), 0);

if (n > 0) {
    // n바이트 수신
} else if (n == 0) {
    // 상대방이 정상적으로 연결 종료
} else if (errno == EAGAIN || errno == EWOULDBLOCK) {
    // 지금 읽을 데이터가 없음
} else {
    // 오류 처리
}
```

소켓을 논블로킹 모드로 설정해도 `recv()`와 `send()` 호출 자체는 애플리케이션 스레드가 직접 수행한다.  
따라서 이는 일반적으로 **논블로킹 동기 I/O**로 분류한다.  
또한 `send()` 성공은 상대방에게 데이터가 도착했다는 뜻이 아니다. 데이터가 커널 송신 버퍼에 받아들여졌다는 뜻이다.  

## 3. Linux의 readiness 기반 네트워크 I/O

Linux에서는 소켓을 파일 디스크립터로 표현한다. `select`, `poll`, `epoll`은 여러 파일 디스크립터의 준비 상태를 감시하는 I/O 멀티플렉싱 API다.   
이들은 I/O 작업을 대신 수행하지 않는다. 애플리케이션에 어떤 파일 디스크립터가 현재 읽기 또는 쓰기 가능한지를 알려준다.  

### epoll

`epoll`은 많은 파일 디스크립터의 readiness 이벤트를 효율적으로 감시하기 위한 Linux API다.  

일반적인 처리 흐름은 다음과 같다.

```text
epoll_ctl()로 관심 있는 FD 등록
              ↓
epoll_wait()에서 이벤트 대기
              ↓
커널이 준비된 FD 반환
              ↓
애플리케이션이 recv()/send() 직접 호출
              ↓
처리할 수 없으면 EAGAIN
              ↓
다음 readiness 이벤트 대기
```

`epoll_wait()`는 이벤트가 없으면 호출 스레드를 block할 수 있다. 하지만 특정 소켓의 `recv()`에서 기다리는 것이 아니라, 등록된 여러 파일 디스크립터 중 하나에서 이벤트가 발생하기를 기다린다.

```cpp
while (true) {
    int count = epoll_wait(epfd, events, MAX_EVENTS, -1);

    for (int i = 0; i < count; ++i) {
        int fd = events[i].data.fd;

        if (events[i].events & EPOLLIN) {
            // non-blocking recv() 수행
        }

        if (events[i].events & EPOLLOUT) {
            // 보류 중인 데이터를 non-blocking send()로 전송
        }
    }
}
```

### 시간 복잡도에 대한 주의

`epoll`을 단순히 O(1) 이라고 표현하는 것은 부정확하다.  
`epoll`의 장점은 `select`나 `poll`처럼 등록된 모든 파일 디스크립터를 매번 선형 순회하지 않고, 준비된 이벤트들을 반환한다는 점이다.  
전체 처리 비용은 대체로 전체 연결 수가 아니라 **발생한 이벤트 수**에 비례한다.  

```text
select/poll: 등록된 전체 FD 집합 검사
epoll: 준비된 이벤트 목록 반환 후 해당 이벤트들을 순회
```

동시 연결 가능 수 역시 `epoll`만으로 결정되지 않는다. 파일 디스크립터 제한, 메모리, 트래픽, 이벤트 처리 비용 및 애플리케이션 구조에 따라 달라진다.  

### EPOLLIN과 EPOLLOUT 처리

수신 중 `EAGAIN`이 발생하면 현재 읽을 데이터가 모두 소진된 것이므로 다음 `EPOLLIN` 이벤트를 기다린다.  
송신할 데이터가 남았는데 부분 전송이나 `EAGAIN`이 발생하면 남은 데이터를 Send Queue에 저장하고 `EPOLLOUT`을 감시한다.  

```text
send() 시도
    ↓
부분 전송 또는 EAGAIN
    ↓
남은 데이터를 Send Queue에 보관
    ↓
EPOLLOUT 등록
    ↓
쓰기 가능 이벤트 발생
    ↓
다시 send()
    ↓
Queue가 비면 EPOLLOUT 감시 해제
```

`EPOLLOUT`은 소켓 송신 버퍼에 여유가 있는 동안 계속 발생할 수 있으므로, 보낼 데이터가 있을 때만 감시하는 것이 일반적이다.  

## 4. Windows의 completion 기반 비동기 I/O

### Overlapped I/O와 IOCP

Windows에서는 `OVERLAPPED` 구조체와 `WSARecv()` 또는 `WSASend()`를 사용해 비동기 I/O 작업을 요청할 수 있다.

```text
WSARecv()/WSASend()로 I/O 요청 제출
                  ↓
즉시 완료하거나 WSA_IO_PENDING 반환
                  ↓
OS가 I/O 작업과 pending 상태 관리
                  ↓
작업 완료 시 completion packet을 IOCP 큐에 삽입
                  ↓
GetQueuedCompletionStatus()가 완료 결과 반환
```

호출 결과가 `WSA_IO_PENDING`이면 오류가 아니라 I/O 요청이 정상적으로 접수되어 비동기로 진행 중이라는 뜻이다.  
작업 스레드는 다음과 같이 완료 이벤트를 기다린다.

```cpp
DWORD transferred;
ULONG_PTR completionKey;
OVERLAPPED* overlapped;

BOOL ok = GetQueuedCompletionStatus(
    completionPort,
    &transferred,
    &completionKey,
    &overlapped,
    INFINITE
);
```

`GetQueuedCompletionStatus()` 자체는 완료 이벤트가 없으면 block할 수 있다. 하지만 스레드가 개별 소켓의 I/O 완료를 직접 기다리는 것은 아니며, OS가 여러 pending I/O를 처리하는 동안 스레드는 사용되지 않는다.  
Overlapped I/O를 사용하는 소켓은 `epoll` 방식처럼 반드시 논블로킹 모드로 설정할 필요가 없다.  
또한 비동기 작업에 전달한 버퍼와 `OVERLAPPED` 객체는 완료 통지를 받을 때까지 유효해야 한다.

## 5. IOCP와 epoll의 차이

| 구분 | epoll | IOCP |
|---|---|---|
| 기본 모델 | Reactor | Proactor |
| 이벤트 의미 | I/O를 시도할 수 있음 | 요청한 I/O가 완료됨 |
| 통지 방식 | Readiness 통지 | Completion 통지 |
| 실제 I/O 수행 | 애플리케이션이 `recv()`/`send()` 호출 | OS에 Overlapped I/O 요청 |
| 일반적인 소켓 모드 | Non-blocking | 별도의 non-blocking 설정 불필요 |
| 대기 함수 | `epoll_wait()` | `GetQueuedCompletionStatus()` |
| 부분 송신 처리 | 남은 데이터를 저장하고 재시도 | 완료 바이트 수를 확인하고 필요하면 추가 요청 |
| 작업이 처리되지 못한 경우 | 일반적으로 `EAGAIN` 후 readiness 대기 | 요청이 pending 상태로 유지된 뒤 완료 통지 |
| 이벤트 처리 비용 | 준비된 이벤트들을 순회 | 완료 큐에서 완료 항목을 꺼내 처리 |

## 6. Send Queue가 필요한 이유

두 모델 모두 실용적인 서버 구현에서는 Send Queue를 사용하는 경우가 많지만 이유와 동작 방식에는 차이가 있다.

### epoll

Send Queue는 다음 목적으로 사용한다.

- 애플리케이션에서 발생한 송신 요청 보관
- 부분 전송 후 남은 미전송 데이터와 offset 관리
- 여러 송신 요청의 처리 순서 직렬화
- 비동기적으로 사용되는 송신 버퍼의 수명 관리
- 대기 중인 데이터 크기를 측정해 backpressure 정책 적용

Queue에 데이터가 남아 있을 때 `EPOLLOUT`을 등록하고, Queue가 비면 `EPOLLOUT`을 해제하는 것이 일반적이다.

### IOCP

IOCP에서는 OS가 pending I/O를 관리하므로 `EAGAIN` 방식의 재시도는 일반적으로 필요하지 않다.

다만 애플리케이션에서는 다음 목적으로 Send Queue를 사용할 수 있다.

- 동일 소켓에 대한 송신 요청 직렬화
- 애플리케이션 메시지 순서 관리
- Overlapped I/O 버퍼 수명 관리
- 부분 완료 처리
- 동시에 outstanding 상태인 송신 요청 수 제한
- backpressure 및 메모리 사용량 관리

동일 소켓에 여러 `WSASend()`를 동시에 제출하는 구현도 가능하지만, 여러 스레드가 무질서하게 요청하면 애플리케이션이 기대한 메시지 순서와 버퍼 관리가 복잡해질 수 있다. 따라서 단일 Send Queue를 통해 송신을 직렬화하는 설계가 흔하다.

Send Queue는 두 API가 반드시 요구하는 고정 조건이라기보다, **순서·부분 처리·버퍼 수명·backpressure를 안전하게 관리하기 위한 애플리케이션 설계 요소**다.

## 7. 정리

`epoll` 기반 서버의 전체 흐름을 넓은 의미에서 “비동기적” 또는 “이벤트 기반”이라고 표현할 수는 있다.

하지만 I/O 모델을 엄밀하게 구분할 때는 다음 표현이 정확하다.

- `epoll`: readiness 기반 I/O 멀티플렉싱 + 논블로킹 동기 I/O
- `IOCP`: completion 기반 비동기 I/O
- 공통점: 소수의 스레드로 다수의 연결을 효율적으로 처리할 수 있음
- 핵심 차이: `epoll`은 준비 상태를 알리고, IOCP는 요청한 작업의 완료를 알림