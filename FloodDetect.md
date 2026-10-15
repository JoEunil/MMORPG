# Flood 탐지  
## 1. 개요
현재 `TrafficFloodDetector`는 정상 클라이언트의 요청 패턴을 기준으로 **20회 수신의 합계 바이트에 허용 범주를 두고, 이를 벗어난 세션을 차단하는 정책**이다. 시간 측정 없이 극단적으로 작은 수신과 수신 버퍼가 계속 채워지는 패턴을 검사해, 해당 세션의 추가 수신·파싱 비용을 제한한다.

## 2. 목적
큰 수신 조각 (m_bytes > BYTE_THRESHOLD): 20회 수신 합계가 2,048바이트를 넘는지 검사한다. 평균 102.4바이트 초과라는 뜻이며 전송 속도를 의미하지 않는다.
작은 수신 조각 (m_bytes < MIN_BYTE_PER_WINDOW): 20회 수신 합계가 128바이트 미만인지 검사한다. 평균 6.4바이트 미만이라는 뜻이며 초당 이벤트 수를 의미하지 않는다.

단발적인 TCP 분할·병합에 바로 반응하지 않도록 20회 수신을 묶어 판단한다. 이는 정상 트래픽의 수신 크기 분포가 허용 범주 안에 머문다는 설계 가정이며, TCP 자체가 보장하는 성질은 아니다. 정상 버스트나 수신 처리 지연으로 누적 데이터가 많아져도 상한을 넘을 수 있으므로, 실제 환경의 수신 크기 분포와 오탐률은 별도 측정이 필요하다. 시간 정보가 없어 중간 크기 데이터의 고속 송신을 구분하지 못하며, bytes/s·수신 완료 횟수/s 제한을 대신하지 않는다.

외부 방어 장비·서비스 (Outer Layer): 대규모 DDoS와 대역폭 고갈은 앞단 인프라에서 대응한다는 역할 분담을 전제로 한다. 해당 인프라는 이 리포지토리의 구현 범위에 포함하지 않는다.
FloodDetector (Inner Layer): 서버에 도달한 개별 세션의 수신 패턴에 저비용 허용 범주를 적용한다. IP 공유 환경에서도 세션 단위로 제한할 수 있으며, 위반한 세션의 후속 수신 작업을 중단한다.

## 3. 요구사항
1. 검사 실패 시 NetHandler에서 연결을 종료하여 다음 수신 작업의 등록을 막는다. 판정은 20회 수신마다 이루어진다.
2. 저비용 연산 (Hot Path): 매 수신마다 호출되는 핵심 경로이므로, steady_clock::now() 등 무거운 연산을 지양한다.

![이미지 로드 실패](images/FloodCheckFlowChart.png)
> iocp 수신완료 처리 work flow 

![이미지 로드 실패](images/IOCPWorkerThreadRecv.png)
> IOCP 워커스레드 내부 동작 구조

### 왜 NetHandler에서 Flood를 처리해야하는가?  
*ClientContext는 TCP 스트림을 패킷 단위로 잘라내는 로직을 담당
1. 호출 스택 오염  
AbortSocket() 호출 시 ClientContext의 종료 처리 메서드를 수행하게 됨.
따라서 ClientContext 이후 로직에서 AbortSocket()을 호출하게 된다면,   
호출스택 상위에 ClientContext를 남겨둔 채 해당 인스턴스를 종료 상태로 만들게 된다.  
이는 코드의 실행 흐름을 예측하기 어렵게 만드는 주요 원인이 된다.   
-> 객체 지향 설계 관점에서도 제어권을 하위 레이어에 부여하는건 위험  
-> 상태값을 가지지 않고 전달자 역할만 하는 NetHandler에서 제어하는 것은 안전한 설계
2. DoS 방어의 효율성  
가장 앞단인 NetHandler에서 즉시 차단하는 것은 시스템 자원을 보호하는 가장 효과적인 전략

## 4. 구현
### sliding window 방식  
* 개념 
	* 단위 시간당 수신 바이트의 정밀한 측정을 위해 Ring Queue와 누적합을 활용함.  
* 특징 
	* 누적합을 통해 구간합 연산 단순화.
	* Q_SIZE를 2의 거듭제곱으로 설정하고 비트 연산을 사용하여 인덱스 관리 효율을 높임.
* 한계
	* 매 수신마다 steady_clock::now()를 호출
	* Ring Queue 및 타임스탬프 관리를 위한 로직 복잡도 증가
	* 평균화 연산 특성상 순간적인 트래픽 변화 대응이 늦음
	* tiny packet 공격을 판단할 수 없다. -> 수신 간격이 큰 경우 매우 작은값으로 측정됨

```cpp  
constexpr uint8_t Q_SIZE = 16; // 2의 거듭제곱
static_assert((Q_SIZE != 0) && ((Q_SIZE& (Q_SIZE - 1)) == 0), "Q_SIZE must be a power of 2!");
constexpr double THRESHOLD = 4096; // bps 제한
class TrafficFloodDetector {
	std::vector<uint64_t> m_byte; // 누적합, ring queue
	std::vector<std::chrono::steady_clock::time_point> m_timeStamp; // ring queue
	uint8_t m_idx;
	uint8_t mask;
	public:
		TrafficFloodDetector()
			: m_byte(Q_SIZE, 0)
			, m_timeStamp(Q_SIZE, std::chrono::steady_clock::now())
			, mask(Q_SIZE - 1)
		{ }
		bool ByteReceived(uint32_t byte) {
			auto before = m_idx;
			m_idx = (m_idx + 1) & mask;
			auto next = (m_idx + 1) & mask; // 가장 오래된 데이터, 측정 기준
			m_timeStamp[m_idx] = std::chrono::steady_clock::now();
			m_byte[m_idx] = m_byte[before] + byte;
			auto byteSum = m_byte[m_idx] - m_byte[next];
			auto duration = m_timeStamp[m_idx] - m_timeStamp[next];
			auto sec = duration_cast<std::chrono::seconds>(duration).count();
			auto bps = byteSum / (double)sec;
			return bps < THRESHOLD;
		}
};
```  

### Fixed window 방식
* 개념 
	* 특정 시간 구간(Window)을 설정하고, 해당 구간 내 수신 합계만 체크하여 로직을 단순화함
* 특징 
	* 변수 두개만 사용하여 메모리 사용량이 적음
* 한계
	* 여전히 steady_clock::now() 연산이 필요함
	* tiny packet 공격을 판단할 수 없다. -> 새 윈도우에서 첫 패킷은 매우 작은 값으로 측정됨

```cpp
	class TrafficFloodDetector {
		std::chrono::steady_clock::time_point m_windowStart;
		uint32_t m_bytesInWindow;

	public:
		TrafficFloodDetector()
			: m_windowStart(std::chrono::steady_clock::now())
			, m_bytesInWindow(0)
		{ }
		bool ByteReceived(uint32_t byte) {
			auto now = std::chrono::steady_clock::now();

			if (now - m_windowStart > WINDOW) {
				m_windowStart = now;
				m_bytesInWindow = byte;
			}
			else {
				m_bytesInWindow += byte;
			}
			return m_bytesInWindow > BYTE_THRESHOLD;
		}
	}; 
```

### Count 기반 수신 크기 검사(현재 구현)
* 개념 
	* 시간 기준을 제거하고, 수신 횟수(RECV_WINDOW)별 합계 바이트를 검사함.
* 특징 
	* 변수 두개만 사용하여 메모리 사용량이 적음
	* steady_clock::now() 같은 무거운 연산이 없어서 Hot path에 적용 가능
	* 임계 횟수 도달 시점에 즉시 판정하여 신속한 차단 가능.
	* 작은/큰 수신 조각이 연속되는 패턴을 찾을 수 있음. 전송 속도나 공격 여부는 판단하지 못함.
	* 세션에서 한 번 검사에 실패하면 종료까지 실패 상태를 유지하고, 새 세션 생성 시 누적값을 초기화함.
```cpp
	class TrafficFloodDetector {
		uint32_t m_bytes = 0;
		uint32_t m_recvCount = 0;

	public:
		bool ByteReceived(uint32_t byte) {
			m_recvCount++;
			m_bytes += byte;

			if (m_recvCount >= RECV_WINDOW) {
			// 1. 큰 수신 조각 
				if (m_bytes > BYTE_THRESHOLD) return true;
			// 2. 작은 수신 조각 
				if (m_bytes < MIN_BYTE_PER_WINDOW) return true;
				m_recvCount = 0;
				m_bytes = 0;
			}
			return false;
		}
	};
```

[TrafficFloodDetector.h](NetLibrary/TrafficFloodDetector.h)

### EWMA 
* 개념
	* 지수 가중 이동 평균을 이용하여 최근 데이터에 높은 가중치를 부여하여 트래픽 변화를 유연하게 대응하는 방식.
* 미도입 사유
	* 현재는 수신 조각 크기의 편향만 관찰하며 EWMA는 도입하지 않음. 속도 제한이 필요하다면 우선 시간 구간을 명확히 정의해야 함.
	* EWMA는 소수점 연산이 포함되어 Count 기반 방식보다 CPU 자원을 더 소모함.
	* 실시간 방어 로직에서는 수치가 명확히 떨어지는 Count 방식이 유지보수 및 임계치 설정에 유리함.
	* 가중치 때문에 Tiny 패킷 공격 방어 설계가 어려움.
