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

C++ 메모리 모델은 캐시나 버퍼를 말하지 않는다.   
어떤 쓰기가 다른 스레드에 보이는지는 두 연산 사이에 happens-before 간선이 있는지로만 결정된다.  

- **memory_order_release (store)**: 이 스토어보다 program order상 앞선 모든 read/write가 스토어 뒤로 넘어가지 못하게 막는 단방향 배리어.   
  다른 스레드가 **같은 변수**를 acquire로 읽어 이 값을 관측하면, release 이전의 모든 쓰기가 그 스레드에 보이도록 보장된다(happens-before 성립).    
- **memory_order_acquire (load)**: 이 로드보다 program order상 뒤에 오는 모든 read/write가 로드 앞으로 당겨지지 못하게 막는 단방향 배리어    

즉 release/acquire는 "버퍼를 비운다"가 아니라, **재배치를 한 방향으로만 막고 같은 변수에 대한 release→acquire가 맞물릴 때만 happens-before 간선을 만든다**로 이해해야 정확하다.

### Debug 빌드에서도 재현되는 이유

Debug 빌드는 컴파일러가 재배치를 하지 않는데도 이 문제가 재현된다. 원인은 store buffer다.  

코어는 store를 캐시에 바로 쓰지 않고 **store buffer**에 넣은 뒤 다음 명령으로 넘어간다.   
그래서 그 store가 다른 코어에 보이기 전에, 뒤따르는 load가 먼저 실행될 수 있다.   
x86-TSO는 다른 재배치는 막지만 이 store→load 순서만은 허용한다. 아래 SB 리트머스 테스트가 그 최소 사례다.  

> x86에서 `seq_cst` store는 `XCHG`(또는 `MOV` + `MFENCE`)로 컴파일되어, 이후 load가 그 store의 전역 가시화 전에 실행되는 것을 막는다.   
> `release` store는 평범한 `MOV`라 그 제약이 없다. 그래서 SB 패턴은 acquire/release로는 막히지 않는다.  
>
> - `MOV` — 값을 옮기는 일반 명령. 순서에 대한 제약이 없다.  
> - `XCHG` — 레지스터와 메모리를 교환하는 명령. x86에서 암묵적으로 `lock` 접두사가 붙어 full barrier로 동작한다.  
> - `MFENCE` — 앞선 메모리 연산이 전부 전역에 보이기 전까지 뒤따르는 메모리 연산을 실행하지 않게 하는 배리어 명령.  


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

### SB 리트머스 테스트 
 x, y는 공유 변수, 초기값 둘 다 0
```
Thread 1:              Thread 2:
store(x, 1)             store(y, 1)
r1 = load(y)            r2 = load(x)
```
실행이 끝난 뒤 **r1 == 0 이면서 r2 == 0**인 상태가 나올 수 있는가?
-> Thread 1의 store(x,1)이 자기 store buffer에 잠깐 머무는 사이(다른 코어엔 아직 안 보임) load(y)를 먼저 실행해버리고,  
Thread 2도 동시에 똑같은 상황이라면, 두 load 모두 상대방의 store가 아직 전역적으로 보이기 전의 "옛날 값(0)"을 읽어버릴 수 있다.  

전부 relaxed: r1==0 && r2==0 관측 가능
전부 acquire/release: r1==0 && r2==0 관측 가능  
-> "같은 변수"에 대한 release-store를 acquire-load가 읽었을 때만 happens-before가 형성됨.
전부 seq_cst: r1==0 && r2==0이 절대 불가능하다고 표준이 보장

> SB 리트머스 테스트는 store buffer가 만들어내는 재배치 효과를 드러내는 최소 테스트  
> x86-TSO 모델에서도 store-load 재배치는 허용하기 때문에 이 문제가 발생한다. 

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

