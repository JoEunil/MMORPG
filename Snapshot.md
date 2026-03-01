# Snapshot 구현

## 1. 개요
이 문서는 네트워크 대역폭을 줄이기 위한 snapshot 처리 전략에 대해 설명한다.  
Snapshot은 서버가 Zone 내 캐릭터, 몬스터, 액션 상태 등의 현재 상태를 정기적으로 클라이언트에 전송하기 위해 만든 __상태 패킷__ 을 의미한다.    
Snapshot 단위로 클라이언트는 캐릭터 위치, 상태, 스킬 시전, Hit 정보 등을 수신하여 게임 화면을 동기화한다.

## 2. 목적
Zone 상태를 Full Snapshot / Delta Snapshot으로 분리하여,
전체 상태 전파 횟수를 줄이고 네트워크 대역폭 소모를 최소화하는 것을 목표로 한다.

## 3. Snapshot 종류
- Session Snapshot  
	- 전파 대상을 결정 
	- [TripleBuffer](TripleBuffer.md)를 사용
- Character Full Snapshot
- Character Delta Snapshot
- Mosnter Full Snapshot
- Monster Delta Snapshot
- ActionResult Snapshot
	- 비정기적으로 발생하며 공유되어야 하는 상태 변화값 처리
	- 현재는 Skill 캐스팅, Hit 연출 처리에 사용

Snapshot 주기
- Delta Snapshot: 20 FPS
- Full Snapshot: 1 FPS
> 대역폭 절감을 위해 Full Snapshot 주기를 더 늘릴 필요가 있음

Character Snapshot 크기
- Full Snapshot Field: 71바이트
- Delta Snapshot Field: 14바이트

Delta Snapshot은 캐릭터 구분 ID + 필드 ID + 필드 값으로 구성되어,
변경된 필드만 전송한다.  
- 한 틱에 5개 이하의 필드 변경이 발생하면, Full Snapshot 대비 효율적이다.

## 4. 추후 변경 사항
- InternalID 최적화
	- 현재 8바이트 → 2바이트로 줄이고, 자원 할당/반납 정책 활용 필요
- Full Snapshot 이름 필드 제거
	- 현재 32바이트 → 제거 시 전체 크기 감소
- 예상 Snapshot 크기
	- Delta Field 1개: 8바이트
	- Full Field 1개: 33바이트
- Delta Snapshot이 Full Snapshot 보다 효율성이 높음
- 하지만 zone 이동이나 Cell 전환 시 새로운 몬스터/캐릭터 로드가 Full Snapshot을 받은 시점에 적용되기 때문에 적절한 주기 설정이 필요함.

__대역폭 계산 예시__
- Delta
```
Cell 당 패킷 크기 * AOI 범위 * Cell당 유저 수 * Zone 수 * tick
(16바이트 * Cell 유저 수) * 9 * Cell당 유저 수 * Zone 수 * 20FPS
= 230,400 * zone * 20fps
= 4,608,000 * zone / sec
= 230,400 * zone / tick
```

- Full
```
Cell 당 패킷 크기 * AOI 범위 * Cell당 유저 수 * Zone 수
(33바이트 * Cell 유저 수) * AOI 범위 * Cell당 유저 수 * Zone 수
= 475,200 * zone / tick
```

- 현재 Grid 기반 AOI:
	- 5x5 Cell, 인접 Cell에 전파
	- AOI 범위: 9 (자신 + 인접 8개 Cell)
	- Cell당 유저 수: Zone당 유저 수 (1000명) / Cell 수(25) = 40명
- Delta 필드는 틱당 2개의 필드 변경으로 가정

__예상 bps(bit per second)__
AOI 범위 9개 Cell, Delta Snapshot 20FPS, Full Snapshot 3초마다 수행될 때
- Delta Snapshot: 약 36 Mbps / zone
- Full Snapshot: 약 1.2 Mbps / zone
이론상, Character Snapshot만 고려하면 30개 Zone, 유저 30,000명 수준에서도 1Gbps 한계 내에서 처리 가능하다.

그러나 현실에서는 다음 요소 때문에 실제 한계는 더 낮다:
- Monster/ActionResult 등 추가 패킷
- 서버 응답 패킷 및 ACK/재전송
- CPU 처리, AOI 확대, Zone 내부 Skill/Hit 처리
> 따라서 서버 1대당 안정적으로 운영 가능한 규모는 최대 (10,000 ~ 150,000) 수준이 현실적이다.

__Zone 처리 관련 주의점__
- Zone은 1개의 전용 코어를 필요로 한다.  
- Zone 내부에서는 Cell 단위, AOI 범위 단위로 Skill, Hit, 상태 전파 처리
- Skill Hit 판정 시 AOI 범위 내 모든 유저를 참조해야 하므로 TPS 유지 관점에서 Zone Depth(유저 수) 제한 필요
- Zone 내부 TPS 한계와 AOI 처리 비용은 실제 테스트를 통해 측정 필요


## 5. 참고 
- [ZoneState_Snapshots.cpp](CoreLib/ZoneState_Snapshots.cpp)  
- [PacketTypes.h](CoreLib/PacketTypes.h)
