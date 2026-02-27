 # MMORPG GAME PROJECT

## 목차
1. [프로젝트 개요]
1. [기술 스택]
1. [아키텍처 다이어그램]
1. [스레드 모델]
1. [핵심 기술 요약 ]
1. [테스트 ]
1. [트러블 슈팅 ]
1. [리팩토링 ]
1. [빌드, 실행 방법 ]
1. [추후 개선 사항 ]
1. [기술 문서 목록 ]

## 프로젝트 개요

__목적__
- MMORPG 서버 구현을 통해 서버 관점에서 요구되는 기술과 개념을 이해한다.  
- 서버 운영과 관련된 핵심 기술을 직접 경험하고 습득한다.  
- 실제로 동작 가능한 형태의 서버를 구현하고, 테스트를 통해 기능을 검증한다.

__목표__
- 클라이언트 로그인부터 사용자 접속 종료 및 재접속 로직까지, __End-to-End 데이터 파이프라인__ 을 설계하고 구현한다.  
- 프로젝트 전반에 걸친 서버 구조와 동작 흐름을 직접 설계한다.
- 채팅, 몬스터, 스킬 등 기본 콘텐츠를 구현하고 클라이언트와의 연동을 검증한다
- 더미 클라이언트를 활용해 처리 성능을 확인한다.


## 기술 스택

__클라이언트__
- 게임 클라이언트: Unity
- 클라이언트 라이브러리: .Net Standard 20 (Unity 연동 목적) 
- 더미 클라이언트, Winform: .Net 8.0 

__서버__
- 게임 서버: C++20  
- DB: Mysql  
- 로그인 서버: Node.js  
- 인증 서버: Redis 

__로그, 모니터링__
- 비동기 로그, 구조화 로그: spdlog, nlohman(json)
- 로그 저장소: promtail, loki
- 모니터링: grafana 

## 아키텍처 다이어그램

![이미지 로드 실패](images/architecture.png)
프로젝트는 클라이언트·서버·공용 라이브러리로 명확히 분리된 구조를 가진다.

클라이언트는 .NET 기반의 ClientCore 라이브러리에서 네트워크 로직을 처리하고, UI는 WinForms 테스트 후 Unity View로 대체할 수 있도록 MVVM 패턴을 적용했다.

서버는 기능별로 모듈화되어 있으며,

- __NetLibrary__는 네트워크 IO 및 네트워크 세션, 핑 처리,
- __CoreLib__는 게임 로직,
- __CacheLib__는 DB I/O와 메모리 캐시,
- __ExternalLib__는 로그 처리 및 Redis 기반 세션 인증을 담당한다.

외부 모듈로는  spdlog, hiredis, libevent, nlohmann(json), Mysql Connector C++를 사용하며, DB는 로그인 DB와 게임 DB로 분리하여 운영한다.
또한 인증 서버는 Redis에 임시 세션을 저장해 게임 서버 진입을 검증하고, 로그인 서버는 로그인 토큰을 발급해 인증 서버에 세션을 등록하는 역할을 수행한다.

## 스레드 모델

스레드별 작업 성격에 따른 분류   

__MaineServer__   
- main thread

__NetLibrary__  
- ping thread  
- net timer  
- perf collector -  IO-bound  
- iocp worker pool - CPU-bound  

__CoreLib__    
- ZoneThreadSet - CPU-bound  
- NoneZoneThreadPool  
- perf collector - IO-bound    
- chat thread - CPU-bound (작업량 증가 시)  
- broadcast thread pool - CPU-bound (작업량 증가 시)  

__CacheLib__  
- flush dispatcher  
- cache flush: IO-bound (DB)    

__ExternalLib__  
- session thread: IO-bound (event 루프 기반)  

표시되지 않은 스레드들은 작업 빈도와 CPU 소모가 낮기 때문에  
CPU-bound 또는 IO-bound로 분류하기 어렵다.  

또한 WSA Send / WSA Recv는 비동기 IO 모델을 사용하기 때문에,  
스레드가 IO 완료를 기다리지 않는다.  
따라서 해당 작업을 IO-bound로 분류할 수 없다.  

## 핵심 기술 요약 

핵심 기술 요소
__네트워크__: IOCP, 비동기 IO, TCP/IP 소켓  
__패킷__: 패킷 직렬화/역직렬화, 가변길이 패킷 처리  
__멀티 스레드 동기화__ : Spin lock, mutex, atomic 변수, memory order, lock-free 자료구조  
__메모리__: 객체 풀  
__캐시__: 캐시라인 정렬, Zone 스레드 코어 고정  

[IOCP](IOCP&epool.md)  - IOCP와 epoll 비교  
[LockFreeQueue.md](LockFreeQueue.md)  - Lock-free 큐 개념 구현과정 기록  
[memory_order.md](memory_order.md)   - memory_order 개념 정리, SpinLock 예제  
[StructuredLogging](StructuredLogging.md) - 구조화 로그 적용 방법 및 로그 분류.    
[TripleBuffer](TripleBuffer.md) - Triple 버퍼 구현 과정
[Monster](Monster.md) - 몬스터 구현 내용 기록   
[Skill](Skill.md) - 스킬 구현 내용 기록 

## 테스트

모니터링 시스템은 Grafana + Loki + Promtail 조합으로 구축하여 서버 메트릭을 실시간으로 시각화하였다. (자세한 내용은 [모니터링](Monitoring.md) 참고)  
다중 접속 환경에서의 서버 안정성을 검증하기 위해 더미 클라이언트를 활용한 단계별 부하 테스트를 진행하였다.
- 100명 테스트 (성공): AOI(Area of Interest) 효율 및 메모리 풀 안정성 검증 완료.
	- 모니터링 지표 분석 중 이동 입력 설계(방향/속도 기반)의 치명적인 결함을 데이터로 확인하였다.
- 1,000명 테스트 (실패): 단일 장비의 리소스 경합으로 인한 커널 레벨 소켓 종료 현상 발생.

[주요 분석 포인트]  
왜 1,000명 테스트에서 소켓이 강제 종료되었는가?   
왜 이동 입력이 전부 처리되지 않았는가?  

[더미 테스트](DummyTest.md)에서 자세한 내용 확인
	
## 트러블 슈팅

- [LockFreeQueue 디버그](LockFreeQueueDebug.md)  
  Lock Free Queue 설계 오류로 인한 교착 상태를 호출 스택 분석을 통해 처리

- [ContextPool memory_order 디버그](MemoryOrderDebug.md)  
  느슨한 메모리 순서(memory_order_relaxed) 사용 시 발생하는 가시성 문제를 분석하고, 적절한 메모리 순서 (acquire / release) 적용 

- [SessionManager 데드락](SessionManagerDeadLock.md)  
  순환 참조로 인해 발생한 데드락 원인 추적, 구조 개선.
- 
## 리팩토링

- [채팅 기능 리팩토링](ChatRefactor.md)
- [ClientContext 리팩토링: God Object](ClientContext.md)
- [Ping 리팩토링](PingRefactor.md)
- [AOI 적용](AOI.md)


## 빌드, 실행 방법


## 추후 개선 사항

- 현재 Zone 전환 로직에서 모든 작업에 mutex 잠금을 사용 → Zone별 lock-free 작업 큐 도입으로 mutex 제거
- 서버 성능 모니터링을 위해 메트릭 수집기 추가 필요
- 스킬, 몬스터 데이터는 데이터 드리븐으로 변경. 지금은 하드코딩으로 처리하는 상태.
- 몬스터 AI 처리 상태머신 기반으로 전환.
- 방향 + 속도를 입력 받는것이 아닌 유저가 이동후 최종 좌표를 서버에 전송하는 방식으로 변경해야 한다.

## 기술 문서 목록

### 개념
- [memory_order](memory_order.md)
- [LockFreeQueue](LockFreeQueue.md)
- [IOCP와 epoll](IOCP&epoll.md)
- [로그 구조화](StructuredLogging.md)
- [TripleBuffer](TripleBuffer.md)

### 컨텐츠 설계
- [Skill](Skill.md) 
- [Monster](Monster.md) 

### 시스템 설계
- [네트워크 Flood 탐지](FloodDetect.md) 
- [Ping 처리](PingLoop.md) 
- [ClientContext](ClientContext.md) 
- [서버 클라이언트 동기화 처리 전략(Snapshot)](Snapshot.md) 
- [서버, 클라이언트 틱 처리](Tick.md) 

### 리팩토링
- [채팅 기능 리팩토링](ChatRefactor.md)
- [ClientContext 리팩토링: God Object](ClientContext.md)
- [Ping 리팩토링](PingRefactor.md)
- [AOI 적용](AOI.md)

### 트러블 슈팅
- [SessionManager 데드락](SessionManagerDeadLock.md)
- [LockFreeQueue 디버그](LockFreeQueueDebug.md)
- [ContextPool memory_order 디버그](MemoryOrderDebug.md)

### 테스트
- [모니터링](Monitoring.md) 
- [더미 클라이언트 테스트](DummyTest.md) 
