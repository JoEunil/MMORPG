# memory_order
## 1. 개요 
이 문서는 atomic 변수의 메모리 가시성과 가시성을 보장하는 memory_order semantic에 대해 설명한다.

## 2. 메모리 재배치

메모리 재배치란? CPU에서 효율적으로 처리하기 위해 순서를 바꿔서 처리하는 것이다. 
컴파일 단계에서 컴파일러가 최적화를 위해 코드의 명령어 순서를 변경할 수 있고, CPU에서도 명령어 파이프라인과 캐시 최적화를 위해 명령어 실행 순서를 변경할 수 있다.  

 <br>

```cpp
x = 1;
y = 2;
if (y == 2)
    print(x);
```

__싱글 스레드에서는 절대 관측 가능한 재배치가 허용되지 않음.__ (싱글 스레드일 경우 위 코드에서 출력되는 값은 무조건 1)

하지만 멀티스레드 환경에서는 이러한 보호가 적용되지 않는다. 

 <br>

```cpp
// Thread A
x = 1;
y = 2;

// Thread B
if (y == 2)
    print(x);
```

위의 예시에서 x가 1이 출력되는것이 보장되지 않는다.


```
Thread A: y = 2;

Thread B:  if(y == 2)  //true

Thread B: print(x)   

Thread A: x = 1;
```

메모리 재배치에 의해 위의 실행 흐름 처럼 x의 대입 연산이 이루어지기 전에 print가 발생할 수 있다.  
x, y가 atomic 변수로 원자적으로 처리된다고 하더라도 메모리 재배치에 의해 예상치 못한 결과가 나올 수 있다.  
간단한 해결책은 mutex를 사용하는 것이며, 성능을 위해 atomic 변수를 사용하는 경우 메모리 재배치에 의한 race condition이 발생하지 않도록 설계해야한다.  
*mutex도 내부에서 atomic을 사용

## 3. 메모리 가시성 
멀티스레드 환경의 문제는 메모리 재배치만이 아니다. 한 코어가 값을 수정했더라도 다른 코어에서 즉시 관측되지 않는 가시성 문제가 존재한다.    
멀티코어 CPU에서 각 코어는 자체 캐시(L1, L2)를 보유한다. 캐시 간 데이터 일관성은 __MESI 프로토콜__ 로 관리된다.  

MESI 프로토콜 자체는 캐시라인 단위 코히어런스를 보장하지만, 성능을 위해 코어와 캐시 사이에 Store Buffer와 Invalidation Queue가 존재한다.   
 -> 코어가 값을 쓸 떄마다 다른 코어의 캐시라인을 무효화하고 응답을 기다리는 것은 성능 저하가 크기 때문에, 버퍼를 활용하여 무효화 응답을 기다리지 않고 다음 명령어를 실행할 수 있도록 한다.  
 -> 하지만 이런 버퍼들로 인해 상태 불일치가 발생한다.  
- Store Buffer: 코어가 값을 쓸 때 캐시에 바로 쓰지 않고 버퍼에 먼저 기록. 다른 코어의 Invalidate 응답을 기다리지 않고 다음 명령어를 실행하기 위함.  
- Invalidation Queue: Invalidate 요청을 받은 코어가 즉시 처리하지 않고 큐에 넣어두고 나중에 처리.  

>코히런스: 같은 메모리 주소에 대해 모든 코어가 동일한 값을 보는 것.  

이 버퍼들로 인해 코어 A가 값을 썼더라도 코어 B가 이전 값을 읽는 구간이 발생한다. 이것이 가시성 문제이며, memory barrier가 이를 해결한다.  
memory_order_release → Store Buffer를 비워 이전 쓰기를 모두 캐시에 반영, 자신의 캐시에 변경이 발생하면 다른 코어들의 invalidation큐를 통해 전파.  
memory_order_acquire → Invalidation Queue를 처리한 후 읽기 수행, cache miss 발생 시 MESI 프로토콜을 통해 최신 값을 보유한 코어(Modified 상태)에서 직접 cache-to-cache transfer로 가져옴.  

## 4. memory_order  
memory_order는 **멀티스레드 환경에서 메모리 재배치와 가시성으로 인한 예상치 못한 동작(race condition)을 방지**하기 위해 사용된다.  
즉, atomic 연산의 순서와 가시성을 제어할 수 있는 기능이다.

### 종류와 의미

| memory_order | 설명 |
|--------------|------|
| `relaxed` | 다른 메모리 접근과의 순서나 가시성을 보장하지 않음. 단일 atomic 변수 접근에만 사용 가능 |
| `acquire` | 이 스레드에서 acquire 이후 수행하는 모든 read/write는, 다른 스레드의 release 이전에 수행된 write를 모두 관측 |
| `release` | 이 스레드에서 release 이전의 모든 write는, 다른 스레드가 acquire할 때 반드시 보임 |
| `acq_rel` | 읽기와 쓰기 모두에 대해 acquire와 release 효과를 동시에 적용, 보통 read-modify-write(RMW) 연산, 예를 들어 fetch_add, compare_exchange 같은 atomic 연산에서 사용 |
| `seq_cst` | 가장 강력한 순서 보장. 모든 스레드에서 단일 순서 유지. 꼭 필요한 곳이 아니라면 acquire/release로 충분한 경우가 많다 |


### 설계 주의점
- 여러 공유 변수가 있는 경우, 각각 atomic으로 쓰면 데이터 관측 순서가 어긋날 수 있음  
	- 해결책: **atomic 변수를 플래그로 사용**해 lock/unlock하고, 사이에 데이터 조작 → 논리적 원자성 보장  
- `acquire/release` 또는 `acq_rel` 사용 시:  
  - release(unlock) 시점에 이전 쓰기가 모두 반영되고  
  - acquire(lock) 시점에서 다른 스레드가 이전 상태를 관측 가능  
  → 메모리 가시성과 순서를 **싱글 스레드처럼 보이게** 하는 목적  
- **주의:** 모든 구간을 싱글 스레드처럼 완벽히 보장하면 성능 저하가 크므로, 필요한 구간에만 `acquire/release`를 적절히 사용하는 것이 중요

## 5. 예시

```cpp
	class SpinLockGuard {
		std::atomic_flag& lock; 
		SpinLockGuard(std::atomic_flag& lo) : lock(lo)
		{
			while (lock.test_and_set(std::memory_order_acquire)) {}
		}
		~SpinLockGuard()
		{
			lock.clear(std::memory_order_release);
		}

	};
```
[SpinLockGuard.h](BaseLib/SpinLockGuard.h)

SpinLock에서 memory_order가 적용된것을 직관적으로 확인할 수 있다.  
lock 획득 시 공유 데이터 조작을 하고 block을 빠져나가면서 소멸자를 호출하여 unlock을 하게 된다.  
`Lock -> 데이터 조작 -> Unlock`   
이 과정에서 메모리 재배치에 의해 race condition(이전 스레드에서 unlock 이후에 공유 데이터 write)을 방지해야된다.

`Lock(memory_order_acquire) -> 데이터 조작 -> Unlock(memory_order_release)`  
이렇게 적용 하면, Unlock(release) 시점에 완료된 데이터 조작이 다른 스레드가 Lock(acquire)할 때 반드시 관측 가능하게 된다.

*Spin lock이란 스레드가 락을 얻을 때까지 루프를 돌며 확인하는 메커니즘이다.  
critical section이 작은 경우  lock 실패 시 wait하지 않고 loop를 돌아 Context switching을 줄일 수 있는 장점이 있다.

