# triple buffer

## 1. 개요
이 문서는 Triple Buffer의 개념과 구현 과정을 기술한다.  

## 2. Triple Buffer란?
riple Buffer는 공유 자원에 대해 읽기(Reader)와 쓰기(Writer) 작업이 빈번하게 발생하는 멀티스레드 환경에서,     
Lock 경합을 최소화하고 데이터의 최신 상태(Snapshot)를 안전하고 빠르게 공유하기 위한 버퍼링 기법이다..  

Triple Buffer는 3개의 버퍼를 사용하여 다음을 보장한다:  
1. Writer는 현재 쓰고 있는 버퍼를 독점적으로 수정  
1. Reader는 최신 완료된 버퍼를 읽음  
1. 버퍼 교체(swap)는 atomic/lock-free로 수행

이를 통해 reader와 writer가 동시에 충돌 없이 데이터를 읽고 쓸 수 있음을 보장한다.

> 본래 그래픽 렌더링 파이프라인(GPU)에서 프레임 생성 속도와 모니터 출력 속도(V-Sync) 차이로 인한 티어링(Tearing) 및 지연(Lag) 현상을 방지하기 위해 고안된 기법이다.

## 3. 도입 배경
### 시스템 요구사항
- 시나리오: Zone 스레드(Writer)가 브로드캐스트 패킷을 생성하여 ThreadPool(Reader)로 넘김.
- 제약 사항: 패킷에는 전송 대상 정보가 없으므로, Worker 스레드는 다시 Zone에서 '전송 대상 목록'을 받아와야 함.
- 구조: SPMC (Single Producer - Multiple Consumer)
	- Writer: 개별 Zone 스레드 (1개)
	- Reader: 브로드캐스트 ThreadPool (N개, 공유됨)

### 해결해야 할 Pain Point
1. 동기화 비용: Writer와 Reader가 동일 자원에 접근 시 Mutex를 사용하면 심각한 성능 저하 발생.
1. 데이터 일관성: 전송 도중 데이터가 변경되면 안 되며, 항상 '특정 시점에 완성된' 스냅샷이 필요.
1.  메모리 효율성 (vs Object Pool):
	- Object Pool은 수천 개의 스냅샷 객체를 관리해야 하며 Pool 고갈 시 메모리 추가 할당을 해야함.
	- Triple Buffer는 고정된 3개의 버퍼만 재사용하므로 메모리가 안정적임.

## 4. 구현 히스토리
### 1차 구현
Rust 포럼의 [SPMC Triple Buffring](https://users.rust-lang.org/t/spmc-buffer-triple-buffering-for-multiple-consumers/10118) 
개념을 참고하여 구현.  

__구현 코드__
```cpp
template <typename T>
class TripleBuffer {
	T* back;
	std::atomic<uint8_t> dirty; // 0: readable, 1: writing , 2: dirty
public:
	void Init(T* b) {
		back = b;
	}
	void Write(T* write) {
		dirty.store(1, std::memory_order_acquire); // 이전 swap 작업 완료
		std::swap(back, write);
		dirty.store(0, std::memory_order_release);
	}
	void Read(T* read)) {
		if (dirty.compare_exchange_strong(0, 2, std::memory_order_acq, std::memory_order_relaxed )) {
			std::swap(back, read);
			dirty.store(2, std::memory_order_release);
		}
	}
};
```

__문제점__  

SPMC 환경에서 다수의 Worker 스레드가 공유된 Reader 풀로 동작할 때 치명적인 단점이 발견됨.
- 오래된 데이터: 오랫동안 대기하던 Worker 스레드가 뒤늦게 깨어나 Read를 수행할 때, 이미 아주 옛날에 Swap 해둔 버퍼를 참조할 수 있다.
- 잘못된 데이터: Zone 객체는 여러 개인데 ThreadPool은 공유됩니다. Worker가 가진 로컬 버퍼가 현재 처리하려는 Zone의 것이 아닐 수 있어 논리적 오류를 유발한다.

[TripleBuffer.h](BaseLib/TripleBuffer.h)

### 2차 개선 버전

Double Back-Buffer 구조와 Ref-Counting을 도입하여 RCU(Read-Copy-Update) 스타일로 개선.  

__핵심 아이디어__

- 버퍼 역할의 재정의 (3-Buffer System)
	- Writer (Local): producer 스레드가 독점적으로 작성 중인 버퍼.
	- Back1 (Staging Area): Writer가 작성을 완료하고 대기하는 Staging Area. 
	- Back2 (Shared Area): 모든 Reader가 참조하는 Shared Area.
- Reader 주도 업데이트: Reader가 접근할 때 Back1에 최신 데이터가 있다면 Back2로 Swap 시키고 읽는다.

__사용한 기술__   

~~Bit-packing(uint8_t)~~
- ~~참조 카운팅과 back1 포인터 상태의 원자성을 보장하기 위해, 두 변수를 하나의 변수(uint8_t)_로 압축~~
- ~~최상위 비트를 back1 포인터 상태, 하위 7 비트는 ref-counter~~
- ~~두개의 상태값을 하나의 원자 조작으로 처리 가능하여 Lock-free 알고리즘 구현 가능.~~

BitPacking(최신 구조, uint16_t)
- 상위 2비트: 상태 표시
	1. 최상위 비트 (0x8000): Write와 back 포인터 swap 시 잠금
	2. 상위 2번째 비트 (0x4000): back2가 최신 상태인지 표시;
- 하위 14비트: Reader count


RAII 패턴을 통한 안전한 반납
- Reader가 사용을 완료한 후 참조 카운트 감소를 위해 ReadDone() 이라는 완료 메서드를 호출해야함.
- BufferReader라는 객체를 정의하여 스코프를 벗어날 때 반납이 이루어지도록 처리.

__동작__  
1. Write
	- CAS로 권한 획득하고 최상위 비트 마킹(0x8000)
	- back1과 write 포인터 스왑
	- 상위 2비트 0으로 하면서 unlock
		- 잠금해제, back2 포인터가 최신 상태가 아님을 표시
1. Read
	- flag ==0 
		- back1-back2 스왑
		- back2 포인터 최신상태 마킹 (0x4000)
		- read counter 1 표시
	- flag & 0x8000 - 재시도
	- 나머지 경우 - read counter 증가
	- Read 끝나면 RAII로 reader count 감소
	
	
__의사 코드__
```
flag (16bit)
┌───────┬───────┬───────────────┐
│ 8000  │ 4000  │ 3FFF (reader) │
│ write │ fresh │ read count    │
└───────┴───────┴───────────────┘

Write:
  CAS -> set 8000
  swap(back1, write)
  CAS -> unset 8000 + set 4000

Read:
  CAS loop:
    0          -> swap(back1, back2), set 4000 + count=1
    &8000 != 0 -> block
    else       -> count++
```

[TripleBufferAdvanced.h](BaseLib/TripleBufferAdvanced.h)




