# MMORPG GAME PROJECT

## 포트폴리오 PPT
[C++ MMORPG Game Server 포트폴리오](https://docs.google.com/presentation/d/18-KmCPBSbsv9-NlrXx0bZFHqadkZOms4cijFuEKi2Q4/edit?usp=sharing)
## 목차
1. [프로젝트 개요](#프로젝트-개요)
2. [기술 스택](#기술-스택)
3. [아키텍처](#아키텍처)
4. [핵심 기술 요약](#핵심-기술-요약)
5. [스레드 모델](#스레드-모델)
6. [부하 테스트](#부하-테스트)  
7. [리팩토링](#리팩토링)
8. [트러블 슈팅](#트러블-슈팅)
9. [추후 개선 사항](#추후-개선-사항)
10. [빌드, 실행 방법](#빌드-실행-방법)
11. [기술 문서 목록](#기술-문서-목록)

## 프로젝트 개요
C++20 기반 MMORPG 게임 서버.  
IOCP, Lock-Free Queue, Write-Back Cache 등 
핵심 컴포넌트를 직접 구현하고 Unity 클라이언트로 검증.  
i3-12100F 4코어 환경에서 **2,000명 동시접속** 부하테스트 완료.

**구현 콘텐츠**    
캐릭터 이동 / 스킬 / 몬스터 / 통합 거래소 / 인벤토리 / 채팅  

> 2,000명 동시 접속 Unity 클라이언트 테스트 영상  
> https://youtu.be/2q2kZwI3uSQ

![이미지 로드 실패](images/SkillAOI.gif)
> 게임 시연 gif

## 기술 스택

__게임 서버 (C++20)__
- 네트워크: IOCP 비동기 IO, 커스텀 바이너리 프로토콜, RingBuffer 패킷 조립
- 동시성: Vyukov's Lock-free Queue, Triple Buffer, Sharded Mutex
- 캐시: In-Process Write-Back Cache (Shard, LRU eviction, ACID 설계)
- 게임 로직: Zone Tick, Cell 기반 AOI, Snapshot Delta 동기화
- 안정성: Ping 좀비 세션 탐지, Flood Detection

__인프라__
- DB: MySQL
- 인증: Redis 
- 로그인: Node.js
- 모니터링: Grafana + Loki + Promtail

__클라이언트__
- 게임 클라이언트: Unity
- 더미 클라이언트: .NET 8.0 (부하 테스트용)
- 클라이언트 라이브러리: .NET Standard 2.0 (Unity 연동)

__외부 라이브러리__
- spdlog, hiredis, libevent, nlohmann/json, MySQL Connector C++, google test

## 아키텍처 

![이미지 로드 실패](images/Architecture_current.png)

서버는 기능별로 4개 모듈로 분리된 구조를 가진다.

| 모듈 | 역할 |
|---|---|
| NetLibrary | IOCP 비동기 IO, 세션 관리, Ping |
| CoreLib | 게임 로직 |
| CacheLib | In-Process 캐시, DB I/O |
| ExternalLib | 구조화 로그, Redis 세션 인증 |

- DB는 로그인 DB / 게임 DB / 거래소 DB로 분리 운영.  
- 클라이언트는 MVVM 패턴으로 WinForms(더미) / Unity(게임) View를 교체 가능하도록 설계.
- 단일 서버 내에서도 작업 특성에 따라 __Zone__(시뮬레이션) / __Non-Zone__(비시뮬레이션) 두 영역으로 논리적으로 분리해 처리한다.  
- Zone은 tick 기반 동기 처리로 이동, 전투, 몬스터 AI 동기화를 담당하고, Non-Zone은 요청-응답 패턴으로 인벤토리, 채팅, 인증 같은 비실시간 작업을 처리한다. 

### 분산 아키텍처로의 확장 설계  

현재는 단일 GameServer 내부에서 Zone / Non-Zone을 논리적으로만 분리해 운영하지만, 단일 프로세스의 처리 한계로 수직 확장에 제약이 있다.  
실서비스 수준의 MMO로 발전시킬 경우, 이 논리적 분리를 물리적 분리로 확장한 아래 구조로 재설계할 수 있다.  

![이미지 로드 실패](images/Architecture_future.png)  

#### 1. GameServer 책임 분리와 Proxy 도입

기존 GameServer가 담당하던 책임을 도메인별 서버로 분리하고, 클라이언트는 단일 Proxy 엔드포인트에만 접속한다.  
Proxy는 opcode 기반 routing table에 따라 적절한 도메인 서버로 패킷을 분배한다.  

- __Zone (in channel server)__ : 실시간 시뮬레이션 (이동, 전투, 몬스터 AI)
- __World Services__ : Mailbox, Guild, Friend 등 비실시간 서비스

이 분리의 핵심 기준은 단순한 zone/non-zone이 아니라 __현재 player의 실시간 상태에 영향을 주는가__ 이다.   
인벤토리 사용/장착 같은 비시뮬레이션 작업이라도 character state에 직접 영향을 주고 latency에 민감하면 Zone에서 처리하고,    
채팅, 우편, 결제처럼 character 상태에 직접 영향이 없는 도메인은 별도 서비스로 분리한다.

#### 2. NetLib 추상화와 라이브러리 재구성

단일 서버에서는 NetLib가 GameServer 내부에 종속된 형태였지만, 분산 환경에서는 모든 내부 서버가 NetLib을 공통으로 사용해야 한다.  
이를 위해 transport 책임만 담당하도록 NetLib을 분리하고, 그 위에 각 서버가 자신의 packet dispatcher와 핸들러를 등록하는 구조로 재구성해야한다.  

- Proxy: routing 핸들러 (내부 서버로 forward)
- Zone Server: 게임 로직 핸들러
- World Services: 도메인별 핸들러

#### 3. Internal Header 기반 세션 식별

내부 서버는 클라이언트와 직접 1:1 socket 매핑을 갖지 않는다.  
Proxy가 내부 서버로 forward할 때 internal header를 추가해 sessionId를 명시하고, 내부 서버는 sessionId 기반으로 컨텍스트를 관리한다.  

#### 4. Redis 기반 데이터 동기화 및 캐시 전략

분산 환경에서는 서버 간 상태 공유를 위해 Redis를 read cache로 활용한다.  
Redis는 authoritative source가 아닌 캐시 계층으로 사용되며, 일부 stale 데이터를 허용하는 __eventual consistency__ 모델을 따른다.  

실시간 게임 상태는 Channel Server가 authoritative하게 관리하며 strong consistency를 유지하고, World Services에서 사용하는 비실시간 데이터는 Redis를 통해 서버 간 공유된다.  
데이터 변경은 각 도메인의 authoritative 서버에서 처리된 후 DB에 반영되며,Redis는 캐시로서 필요 시 갱신되거나 TTL 기반으로 동기화된다.  

## 핵심 기술 요약 

MMO 특성상 수천~수만 개의 동시 커넥션을 처리해야 하므로 IOCP 기반 비동기 IO를 채택, 소수의 워커 스레드로 대용량 트래픽을 처리. Lock-Free 자료구조와 Zone 기반 멀티스레드 아키텍처로 동시성 최적화.

### 1. 소켓과 패킷 수신 처리 구조
- __수신 및 전파__ : IOCP 비동기 수신 → ClientContext의 RingBuffer를 통한 패킷 조립 → PacketView를 활용한 제로 카피 지향 로직 전파.
	- [IOCP](IOCP&epoll.md) : IOCP와 epoll 비교
	- [ClientContext](ClientContext.md): TCP 수신 버퍼 처리구조, Ring buffer와 Context 누적버퍼 처리 방법
- __송신 처리__ 
	- [IOCP Send 파이프라인](IOCPSendPipeline.md): IOCP 송신 큐 처리구조, chunck 패킷 처리 구조

### 2. 멀티스레드 동기화 및 성능 최적화
- [memory_order](memory_order.md) : 멀티스레드 환경의 메모리 재배치와 가시성 문제를 방지하고 성능을 최적화하기 위해, Acquire-Release 시맨틱의 동작 원리를 분석하고 이를 SpinLock 설계에 적용한 과정을 정리.
- [LockFreeQueue](LockFreeQueue.md): Lock 경합을 방지하기 위해 atomic 변수와 CAS(Compare-And-Swap) 함수를 통해 구현한 __Vyukov's Lock-free Queue__ 구현 및 검증.
- [TripleBuffer](TripleBuffer.md) : RCU + Triple Buffer 개념을 응용한 SPMC Lock-free 세션 스냅샷 구현. Bit Packing으로 상태를 단일 atomic 변수에 압축 관리.

### 3. 네트워크 안정성
- [Ping](PingLoop.md) : Ping 루프를 통해 좀비 세션 탐지 및 순환 참조 없는 안전한 세션 종료 로직 구현.
- [Flood Detection](FloodDetect.md): 어플리케이션 레벨에서의 대역폭 공격 방어를 위해 패킷 유입량을 감시하고 차단하는 탐지 로직 적용.  
- [Tick](Tick.md) & [Snapshot](Snapshot.md) : 클라이언트와 서버 간의 틱 기반 동기화 및 패킷 크기 최적화를 위한 스냅샷 전략 수립.

### 4. 콘텐츠 구현 및 모니터링
- [Monster](Monster.md) & [Skill](Skill.md) : 간단한 AI 및 상호작용 로직을 통해 구조적 위험성 분석. AOI(Area of Interest) 및 Cell 분할 필요성 도출.
- [거래소 시스템](Bazaar.md) : 서버 통합 거래소 구현. 재화 특성 기반 저장 전략 분리 (Gold: Write-Back 캐시 / Diamond: DB 트랜잭션), Stored Procedure로 ACID 보장, 두 장군 문제로 인한 Bounded Loss 수용 및 거래 로그 기반 복구 전략 적용.
- [거래소 시스템 테스트](BazaarTest.md) : 기본 기능, Crash 시나리오, 동시 구매 경합, lock contention 관측 테스트 수행. CAS 기반 중복 구매 방지 및 Crash 후 복구 가능성 확인.
- [StructuredLogging](StructuredLogging.md) : 서버 내부 상태와 테스트 결과를 시각화하고 추적하기 위해 로그를 구조화하여 분류 및 적용.

### 5. 캐시 및 DB 설계
접근 빈도가 높은 인벤토리 데이터를 대상으로 In-Process 메모리 캐시를 직접 구현.  
Write-Back 전략을 채택하여 DB IO 부하를 줄이고, 캐시 동작 전반에 걸쳐 ACID를 고려한 설계를 적용.
- [Cache](CacheLib.md) : 캐시 배치 전략, Write-Back/Read-Through 동작 흐름, 구조 설계
- [Cache ACID](CacheLib_ACID.md) : 캐시 상태값 도입 및 ACID 보장 설계
- [Cache UnitTest](CacheLib_Test.md) : DB fetch, cache hit/miss, flush, LRU eviction 동작 검증
- [DB](DB.md) : 수직 파티셔닝, 복합 인덱스, View Table 설계

## 스레드 모델

스레드별 작업 성격에 따른 분류   

__MainServer__   
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
- memory queue  

__CacheLib__  
- flush dispatcher  
- cache flush: IO-bound (DB)    
- memory queue   

__ExternalLib__  
- session thread: IO-bound (event 루프 기반)  

표시되지 않은 스레드들은 작업 빈도와 CPU 소모가 낮기 때문에  
CPU-bound 또는 IO-bound로 분류하기 어렵다.  

## 부하 테스트
[모니터링](Monitoring.md)   
모니터링 시스템은 Grafana + Loki + Promtail 조합으로 구축하여 서버 성능 지표를 실시간으로 시각화하였다.   

[더미 테스트](DummyTest.md)  
다중 접속 환경에서의 서버 안정성을 검증하기 위해 더미 클라이언트를 활용한 단계별 부하 테스트를 진행하였다.
- 100명 테스트 (성공): 메모리 풀 안정성 검증 완료.
- 1,000명 테스트 (실패): 더미 클라이언트 단일 스레드 IO 병목 →
  TCP 수신 버퍼 초과로 소켓 종료. 서버 문제 아님을 Wireshark로 확인.

[더미 테스트2](DummyTest2.md)  
- 2,000명 테스트 (성공): 더미 클라이언트 IO 스레드풀 적용 및 서버 스레드 우선순위 설정 제거로 starvation 현상 제거. 

## 리팩토링
기능 구현 과정에서 직면한 구조적 한계와 병목 지점을 분석하고, 명확한 근거와 필요성에 따라 진행한 리팩토링 기록.

[채팅 기능 리팩토링](ChatRefactor.md)    
- 필요성: 채팅 트래픽이 메인 게임 로직(Zone Tick)의 성능에 영향을 미칠 수 있는 것을 인지. 핵심 로직의 안정성을 위해 채팅 로직 레이어 분리 결정.
- 내용: 채팅 전용 스레드/레이어 분리 및 추가 기능(Global Chat, 귓속말) 구현 

[ClientContext 리팩토링: God Object](ClientContext.md)  
- 필요성: 하나의 객체가 너무 많은 상태를 관리하여 복잡성이 높고, 버그 추적에 어려움 경험
- 내용: 기능을 쪼개어 ClientContext를 수신 버퍼 관리 전용으로 경량화, SRP

[Ping 리팩토링](PingRefactor.md)  
- 필요성: CoreLib에서 소켓을 직접 제어할 수 없어서 비정상 종료 소켓을 강제로 끊을 수 없는 구조
- 내용: NetLib로 Ping 루프를 이관하여, 비정상 종료 or Ping 응답 실패 누적된 소켓 강제 종료.

[AOI 적용](AOI.md)  
- 필요성: 스킬 판정 시 모든 대상을 조회하는 방식이 유저 수 증가에 따른 성능 하락이 될 수 있음을 인지
- 내용: 격자형 Cell 구조 도입 및 다중 Cell의 패킷 조각(Chunk)을 효율적으로 병합 전송하는 Overlapped 구조체 개선 병행.

[DB 전용 worker 스레드 분리](DBWorkerRefactor.md)
- 필요성: DB I/O 블로킹으로 인한 캐시 처리 지연 제거  
- 내용: DB 작업을 전용 워커 스레드로 분리, LoadFromDB 비동기 전환 및 클라이언트 재시도 방식 적용

[ObjectPool 리팩토링](ObjectPool.md)
- 필요성: 동적 resize 구조가 병목 상황에서 추가 할당을 유발해 지연을 전이시킬 수 있음을 인지
- 내용: Elastic ObjectPool 제거 후 FixedObjectPool로 교체, 고갈 시 Drop + 로그로 대응. 크리티컬 경로(Disconnect)는 retry loop로 별도 처리.  

## 트러블 슈팅

- [LockFreeQueue 디버그](LockFreeQueueDebug.md)  
  Lock Free Queue 설계 오류로 인한 교착 상태를 호출 스택 분석을 통해 원인 분석 및 수정

- [ContextPool memory_order 디버그](MemoryOrderDebug.md)  
  느슨한 메모리 순서(memory_order_relaxed) 사용 시 발생하는 가시성 문제를 분석하고, 적절한 메모리 순서 (acquire / release) 적용
  Debug 빌드에서 발생, Core에서 비순차 실행으로 인한 명령 재배치 케이스    

- [SessionManager 데드락](SessionManagerDeadLock.md)  
  순환 참조로 인해 발생한 데드락 원인 추적, 구조 개선.

- [DummyTest 과정중 발생한 오류 디버그](DummyTestDebug.md)  
  RingBuffer 엣지 케이스 처리.

## 추후 개선 사항

- 현재 Zone 전환 로직에서 모든 작업에 mutex 잠금을 사용 → Zone별 lock-free 작업 큐 도입으로 mutex 제거
- 서버 성능 모니터링을 위해 메트릭 수집기 추가 필요
- 스킬, 몬스터 데이터는 데이터 드리븐으로 변경. 지금은 하드코딩으로 처리하는 상태.
- 몬스터 AI 처리 상태머신 기반으로 전환.
- 방향 + 속도를 입력 받는 것이 아닌 유저가 이동후 최종 좌표를 서버에 전송하는 방식으로 변경해야 한다.
  
## 빌드, 실행 방법
### 1. 개발 환경
Client Engine: Unity 6.0  
IDE: Visual Studio Community 2022  
OS: Windows 11 (x64), 25H2  
DB/Store: MySQL8.4.0, Redis 3.0.504 (Windows 마지막 버전, 2016년 이후 업데이트 없음, Redis 서버는 Linux 환경에서 운영하는것이 일반적)  

Game Server: C++ 20, Windows SDK 10.0   
Client Core: .Net Standard 2.0  
DummyClient: .Net 8.0  
Login: NodeJs 22.14.0  

### 2. 외부 모듈
프로젝트에 필요한 모든 외부 라이브러리는 /External 경로에 포함되어 있다.  
솔루션 파일을 열어 즉시 빌드가 가능하도록 의존성이 설정되어 있으며, 라이브러리 바이너리도 프로젝트에 포함되어있다.   

포함 모듈: hiredis, spdlog, MySQL Connector/C++, nlohmann/json
- Nuget 패키지: Newtonsoft.Json, libevent2

- x32 또는 Windows 8 이하 환경에서 빌드 시, 해당 타겟에 맞는 라이브러리 바이너리를 /External 경로에 재배치해야 한다.
- Linux 및 macOS 환경에서의 빌드는 지원하지 않는다.

### 3. 외부 인프라 설정
DB  
- 스크립트 실행: Resources/DB/init.sql 
	- 로그인 DB, 게임 DB 생성
- Host: localhost
- ID/PW: root / 1234 

인증서버  
- Program Files\redis-server.exe 실행
- Host: 127.0.0.1
- Port: 6379

로그인서버  
- 터미널에서 실행
```
 cd LoginServer
 node app.js
```
- 또는 VisualStudio에서 실행.
- localhost, 포트 3000

게임 서버  
1. Mainserver 빌드 후 실행
1. 또는 Resources/Release/MainServer.exe 실행
	1. 실행파일 직접 사용 시 로그 경로 변경됨
	1. Resources/monitoring/promtail-config.yaml에서 path를 Resources/Release/logs/~~.log로 변경


### 4. 모니터링 설정
1. grafana, loki, promtail 설치.  
	1. grafana: https://grafana.com/grafana/download?edition=oss&platform=windows
	1. loki, promtail: https://github.com/grafana/loki/releases (assets에서 loki, promtail 찾아서 설치)
	
1. Resources/monitoring 폴더의 loki-config.yaml과 promtail-config.yaml 수정
	1. 파일 저장 경로를 로컬 환경에 맞게 수정
1. loki.bat, promtail.bat 순서대로 실행.
	1. bat, yaml을 각각 loki, promtail 실행파일과 같은 경로로 설정
1. Resources/grafana.db를 아래 위치에 덮어쓰기
	1. C:\Program Files\GrafanaLabs\grafana\data
1. Grafana 실행
	- ID/PW: admin / admin

### 5. 계정 생성, 캐릭터 생성
- 로그인 서버, DB 실행중인 상태에서   
   ```node Resources/monitoring/create_user.js ```

- 캐릭터 생성 sql 실행
  ```Resources/DB/createCharacter.sql 실행```

### 6. 실행 순서
DB → Redis → 로그인 서버 → 게임 서버

### 7. 클라이언트 테스트
Dummy Client 
- DummyClients/Program.cs에서 접속자 수 설정 후 DummyClients 실행

Unity Client
- Resources/UnityClient/UnityClient.exe

기본 테스트 계정
- ID: test_1 ~ test_5000
- PW: 12345


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
- [캐시 설계](CacheLib.md)
- [캐시 ACID 설계](CacheLib_ACID.md)
- [캐시 단위 테스트](CacheLib_Test.md)
- [DB 설계](DB.md)
- [IOCP Send 파이프라인](IOCPSendPipeline.md)
- [거래소 시스템](Bazaar.md)
- [거래소 시스템 테스트](BazaarTest.md)

### 리팩토링
- [채팅 기능 리팩토링](ChatRefactor.md)
- [ClientContext 리팩토링: God Object](ClientContextRefactor.md)
- [Ping 리팩토링](PingRefactor.md)
- [AOI 적용](AOI.md)
- [DB 전용 worker 스레드 분리](DBWorkerRefactor.md)
- [ObjectPool 리팩토링](ObjectPool.md)

### 트러블 슈팅
- [SessionManager 데드락](SessionManagerDeadLock.md)
- [LockFreeQueue 디버그](LockFreeQueueDebug.md)
- [ContextPool memory_order 디버그](MemoryOrderDebug.md)
- [DummyTest 디버그](DummyTestDebug.md)

### 테스트
- [모니터링](Monitoring.md) 
- [더미 클라이언트 테스트](DummyTest.md) 
- [더미 클라이언트 테스트2](DummyTest2.md) 
