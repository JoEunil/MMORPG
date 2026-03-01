# 로그 구조화

## 1. 개요
이 문서는 로그 구조화의 목적과 구현 방법에 대한 문서이다. 

## 2. 로그 구조화란?
기존 로그는 문자열 기반이라 검색/분석이 어렵다.
로그 구조화는 JSON (key-value) 등의 포맷을 이용하여 대규모 시스템에서 로그 검색, 모니터링 등을 수행할 수 있게 하는것이다.

## 3. 목적
1. 디버깅 난이도 감소  
게임 서버는 다음과 같은 특성으로 인해 버그를 추적하기 어렵다:
	- 멀티스레드 동작
	- Cell/Zone 단위 공간 분리
	- 몬스터 AI 로직 
2. 실시간 모니터링 및 트래픽 분석
	- TPS
	- 접속자 수
	- Zone 별 로직 실행 간격
	- 특정 이벤트 발생 횟수
## 3. 구현 방법
__사용 툴 및 라이브러리__  
- Grafana : 로그 시각화 대시보드
- Loki : 로그 저장소
- spdlog : 고성능 C++ 로깅 라이브러리
- nlohmann::json : JSON 직렬화/역직렬화

__Json 로그 포맷 생성 코드__  
```cpp
size_t thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
	std::chrono::system_clock::now().time_since_epoch()
).count();
return nlohmann::json{
	// 로그에서 now()는 부담 안됨.
	{"timestamp", ts},
	{"event", event},
	{"thread_id", thread_id},
	{"msg", msg},
	{"data", data}
}.dump();
```
__로그 필드 설명__  
timestamp - 시간   
event - 발생한 이벤트 종류 (클래스 구분)  
thread-id - 로그가 호출된 스레드  
msg - 로그 발생지점 구분 (메서드 구분)  
data: 부가 데이터. key-value 형식의 JSON 객체로 필요한 정보를 구조적으로 기록

## 4. 로그 종류
- Core Sysmtem
	- 프로세스 시작/종료
	- 스레드 생성/종료
	- 구성 요소 초기화
- Game play 
	- Zone 이동 요청
	- (※ 현재 단계에서는 게임 플레이 기록은 최소화하고 성능 중심으로 유지)
	- ~~(플레이 로그는 전체를 기록하면 안되고, 일단 메모리에 기록 후 특정 조건을 만족할 경우만 로그를 기록해야한다.)~~
- Error / Exception Logs
	- function_name
	- thread_id
	- exception
- Performance Logs
	- Zone 로직 TPS
	- 객체 풀 크기
	- lock-free 작업큐 enqueue, pop 횟수

## 5. 로그 처리 원칙
책임 원칙
- 로그는 반드시 '주체가 명확한 레이어'에서만 남긴다.
- 하위 레이어는 가능한 한 로직 수행만 담당하고,
- 상위 레이어에서 상태를 조합해 로그를 기록한다.
- 예시
	- RingBuffer → 하위 레이어, 로그 없음
	- ClientContext → 상위 레이어, 세션 ID 등 주체가 명확하므로 로그 기록 가능

과도한 로깅 금지
- 초당 수백~수천 번 호출되는 핵심 루프에서 무분별한 로그는 성능을 해칠 수 있다.
- 성능 민감 구간은 “집계 후 1초 단위 perf log 출력” 형태로 제한한다.
