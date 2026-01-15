## memory_order

### 메모리 재배치

메모리 재배치란? CPU에서 효율적으로 처리하기 위해 순서를 바꿔서 처리하는 것.

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

### memory_order
memory_order는 **멀티스레드 환경에서 메모리 재배치로 인한 예상치 못한 동작(race condition)을 방지**하기 위해 사용됩니다.  
즉, atomic 연산의 순서와 가시성을 제어할 수 있는 기능입니다.

#### 종류와 의미

| memory_order | 설명 |
|--------------|------|
| `relaxed` | 다른 메모리 접근과의 순서나 가시성을 보장하지 않음. 단일 atomic 변수 접근에만 사용 가능 |
| `acquire` | 이 스레드에서 acquire 이후 수행하는 모든 read/write는, 다른 스레드의 release 이전에 수행된 write를 모두 관측 |
| `release` | 이 스레드에서 release 이전의 모든 write는, 다른 스레드가 acquire할 때 반드시 보임 |
| `acq_rel` | 읽기와 쓰기 모두에 대해 acquire와 release 효과를 동시에 적용, 보통 read-modify-write(RMW) 연산, 예를 들어 fetch_add, compare_exchange 같은 atomic 연산에서 사용 |
| `seq_cst` | 가장 강력한 순서 보장. 모든 스레드에서 단일 순서 유지. 성능 저하가 커서 사용하지 않는것이 좋음 |


#### 설계 주의점
- 여러 공유 변수가 있는 경우, 각각 atomic으로 쓰면 데이터 관측 순서가 어긋날 수 있음  
	- 해결책: **atomic 변수를 플래그로 사용**해 lock/unlock하고, 사이에 데이터 조작 → 논리적 원자성 보장  
- `acquire/release` 또는 `acq_rel` 사용 시:  
  - release(unlock) 시점에 이전 쓰기가 모두 반영되고  
  - acquire(lock) 시점에서 다른 스레드가 이전 상태를 관측 가능  
  → 메모리 가시성과 순서를 **싱글 스레드처럼 보이게** 하는 목적  
- **주의:** 모든 구간을 싱글 스레드처럼 완벽히 보장하면 성능 저하가 크므로, 필요한 구간에만 `acquire/release`를 적절히 사용하는 것이 중요

### Example

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

