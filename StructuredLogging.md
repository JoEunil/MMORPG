## 로그 구조화

## 로그 구조화란?
기존 로그는 문자열 기반이라 검색/분석이 어렵다.
로그 구조화는 JSON (key-value) 등의 포맷을 이용하여 대규모 시스템에서 로그 검색, 모니터링 등을 수행할 수 있게 하는것이다.

## 목적
1. 게임 서버에서 추적하기 어려운 버그를 찾아낼 수 있도록 한다.
	- 멀티 스레드
	- Cell/Zone 기반
	- 몬스터 AI 
2. TPS, 접속자 수 등 애플리케이션 정보를 모니터링 할 수 있도록 한다.

## 구현 방법
사용 툴 및 라이브러리
- Grafana : 로그 모니터링
- Loki : 로그 DB
- spdlog : 로그 라이브러리
- nlohmann::json : json 라이브러리

## 로그 종류
- Core Sysmtem
	- 프로세스 시작/종료에 관련된 로그 
- Gaem play 
	- Zone 이동 요청
	- Monster 생성 / 사망
	- Skill 사용 
- Error / Exception Logs
	- function_name
	- thread_id
	- exception
- Performance Logs
	- Zone 로직 TPS
	- Cell당 monster count
	- Cell당 user count
	- Pool 재할당

## 주의 사항

## 로그 처리 원칙
- 하위 레이어는 단순 로직 수행만 하고, 발생 상태/결과를 반환하여 상위 레이어에서 관측하도록 한다.
- 로그는 항상 발생 주체가 명확한 레이어에서만 남긴
- 예: RingBuffer → 하위 레이어, 로그 없음
- 예: ClientContext → 상위 레이어, 세션 ID 등 주체가 명확하므로 로그 기록 가능