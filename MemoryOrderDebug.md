# Memory order Debug

## 1. 개요
[모니터링에서 발견한 버그 수정](https://github.com/JoEunil/MMORPG/commit/16247c033861497bd2840bb25f22b093a70e5370) 커밋에서 발견한 memory order 관련 문제를 해결하는 과정을 설명한다.

## 2. 문제 상황
![이미지 로드 실패](images/flushQueue.png)
> contextPool과 flushQueue

dummy client로 테스트하던 중 클라이언트 종료 시 flushQueue가 비워지지 않는 것을 관측했다.
flush 조건을 만족한 상태이고, 새로운 클라이언트를 접속시켜 flush가 실행되도록 하였음에도 flush가 수행되지 않았다.

*[ClientContextPool](NetLibrary/ClientContextPool.h)은 클라이언트 별로 수신 버퍼를 관리하는 ClientContext의 객체 풀이다.  
*flushQueue는 ClientContext 객체 반납 과정에서 아직 작업이 남은 context를 임시 큐에 넣고 나중에 flush 되도록 만든 것이다.

## 3. 원인 분석
__Disconnect()__
```cpp
m_connected.store(false, relaxed);
if (!m_connected.load(relaxed) && m_workingCnt.load(relaxed))
```

__ReleaseBuffer()__
```cpp
m_workingCnt.fetch_sub(1, relaxed);
if (!m_connected.load(relaxed) && m_workingCnt.load(relaxed) == 0)
```
[ClientContext](ClientContext.md)에서 연결 종료를 입력받는 Disconnect 메서드와   
Core 로직에서 사용 완료한 버퍼를 반환하는 ReleaseBuffer메서드이다.  
PacketView를 통해 수신 버퍼를 복사하지 않고 버퍼 그대로 참조하여 사용하도록 만들었기 때문에,   
Disconnect시 바로 자원을 정리하는것이 위험하다고 생각해서, workingCnt를 두어 모든 작업이 끝난후 자원이 정리되도록 설계하였다.

m_connected와 m_workingCnt 둘다 atomic 변수이고, 어떤 자원을 lock하는 개념이 아니여서 memory_order_relaxed를 사용하였다.

ReleaseBuffer()의 fetch_sub()보다 m_connected.load()가 먼저 실행될 수 있고
이로 인해 조건문이 잘못 평가되는 race condition이 발생할 수 있다.

## 4. 해결
__Disconnect()__
```cpp
m_connected.store(false, release);
if (!m_connected.load(acquire) && m_workingCnt.load(acquire))
```

__ReleaseBuffer()__
```cpp
m_workingCnt.fetch_sub(1, acq_rel);
if (!m_connected.load(acquire) && m_workingCnt.load(acquire) == 0)
```
위와 같이 acquire-release 관계를 형성하도록 memory_order를 수정한 후
flush가 정상적으로 수행되었다.  
(relaxed에서는 두 atomic 간의 순서·가시성이 보장되지 않아 조건 평가가 뒤틀릴 수 있었음)

## 5. Note
- relaxed는 atomic 변수의 원자성을 보장하지만, 스레드 간 가시성은 보장하지 않는다.
  - store buffer flush가 강제되지 않아 가시성 지연이 발생할 수 있다.
  - x64 TSO + Debug 빌드 환경에서 명령 재배치가 없었음에도 버그가 발생한 원인이다.

- fetch_add 기반 ID 발급은 relaxed여도 중복이 발생하지 않는다.
  - fetch_add는 read-modify-write 연산으로 캐시 라인을 독점하기 때문에,
    발급 스레드들끼리는 항상 최신값 기반으로 연산이 수행된다.
  - 단순 load로 관측하는 스레드가 없으므로 stale read 자체가 발생하지 않는다.

- 반면 m_connected + m_workingCnt처럼 두 변수를 논리적으로 연결해서
  조건 판단에 사용하는 경우는 relaxed가 위험하다.
  - 각 변수의 가시성 시점이 독립적이라 한 변수는 최신값,
    다른 변수는 stale값을 동시에 관측할 수 있기 때문이다.
  - 플랫폼·빌드 모드와 관계없이 acquire/release로 happens-before를 형성해야 한다.

## 6. 참고
- [ClientContext 설명 문서](ClientContext.md)  
- [ClientContext.h](NetLibrary/ClientContext.h)  
- [ClientContextPool.h](NetLibrary/ClientContextPool.h)  
