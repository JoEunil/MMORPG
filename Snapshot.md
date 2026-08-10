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
- Monster Full Snapshot
- Monster Delta Snapshot
- ActionResult Snapshot
	- 비정기적으로 발생하며 공유되어야 하는 상태 변화값 처리
	- 현재는 Skill 캐스팅, Hit 연출 처리에 사용

Snapshot 주기
- Delta Snapshot: 20 FPS
- Full Snapshot: 1 FPS
> 대역폭 절감을 위해 Full Snapshot 주기를 더 늘릴 필요가 있음

Character Snapshot 크기
- Full Snapshot Field: 43바이트
- Delta Snapshot Field: 10바이트

Delta Snapshot은 캐릭터 구분 ID + 필드 ID + 필드 값으로 구성되어,
변경된 필드만 전송한다.  
- 한 틱에 4개 이하의 필드 변경이 발생하면, Full Snapshot 대비 효율적이다.

## 4. 추후 변경 사항
- InternalID 최적화
	- 8바이트 → 4바이트 적용 완료 (재사용 없이 단조 증가, 상한 도달 시 발급 중단)
	- 2바이트까지 줄이려면 자원 할당/반납 정책 활용 필요
- Full Snapshot 이름 필드 제거
	- 적용 완료. 32바이트 charName을 profile_id(4) + version(4)로 대체했다.   
	  이름은 클라이언트가 profile_id로 별도 조회해 캐싱한다. ([Profile.md](Profile.md))   
- 예상 Snapshot 크기 (InternalID를 2바이트까지 줄였을 때)
	- Delta Field 1개: 8바이트
	- Full Field 1개: 41바이트
- Delta Snapshot이 Full Snapshot 보다 효율성이 높음
- 하지만 zone 이동이나 Cell 전환 시 새로운 몬스터/캐릭터 로드가 Full Snapshot을 받은 시점에 적용되기 때문에 적절한 주기 설정이 필요함.

__대역폭 계산 예시__
- Delta (유저당 필드 2개 = 20바이트 가정)
```
20B * AOI 범위(9) * Cell당 유저 수(40, 송신) * Cell당 유저 수(40, 수신)
= 20 * 9 * 40 * 40 = 288,000 byte / tick / zone
= 288,000 * 20FPS  = 5,760,000 byte / sec / zone   → 약 46 Mbps / zone
```

- Full (유저당 43바이트)
```
43B * AOI 범위(9) * Cell당 유저 수(40, 송신) * Cell당 유저 수(40, 수신)
= 43 * 9 * 40 * 40 = 619,200 byte / tick / zone
= 3초 주기         → 약 1.7 Mbps / zone
```

- 현재 Grid 기반 AOI:
	- 5x5 Cell, 인접 Cell에 전파
	- AOI 범위: 9 (자신 + 인접 8개 Cell)
	- Cell당 유저 수: Zone당 유저 수 (1000명) / Cell 수(25) = 40명
- Delta 필드는 틱당 2개의 필드 변경으로 가정

__예상 bps(bit per second)__
AOI 범위 9개 Cell, Delta Snapshot 20FPS, Full Snapshot 3초마다 수행될 때
- Delta Snapshot: 약 46 Mbps / zone
- Full Snapshot: 약 1.7 Mbps / zone
이론상, Character Snapshot만 고려하면 20개 Zone, 유저 20,000명 수준에서 1Gbps에 근접한다.  
Full Snapshot 비중이 크게 줄어 이제는 Delta가 대역폭을 사실상 전부 차지한다.  

그러나 현실에서는 다음 요소 때문에 실제 한계는 더 낮다:
- Monster/ActionResult 등 추가 패킷
- 서버 응답 패킷 및 ACK/재전송
- CPU 처리, AOI 확대, Zone 내부 Skill/Hit 처리

__Zone 처리 관련 주의점__
- Zone은 1개의 전용 코어를 필요로 한다.  
- Zone 내부에서는 Cell 단위, AOI 범위 단위로 Skill, Hit, 상태 전파 처리
- Skill Hit 판정 시 AOI 범위 내 모든 유저를 참조해야 하므로 TPS 유지 관점에서 Zone Depth(유저 수) 제한 필요
- Zone 내부 TPS 한계와 AOI 처리 비용은 실제 테스트를 통해 측정 필요


## 5. 참고 
- [ZoneState_Snapshots.cpp](CoreLib/ZoneState_Snapshots.cpp)  
- [PacketTypes.h](CoreLib/PacketTypes.h)
