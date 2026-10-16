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
- 계산 조건

  - Zone당 1,000명, 5x5 Cell에 균등 분포 → Cell당 40명
  - 모든 유저가 Delta마다 2개 필드를 변경 → 유저당 20바이트
  - Full Field는 유저당 43바이트
  - 현재 설정: Delta 50ms, Full 1초 (`CoreLib/Config.h`)
  - 패킷 헤더, Monster/ActionResult, TCP/IP 오버헤드를 제외한 Character payload만 계산

Zone 전체 전송량은 **각 수신 Cell에서 보는 송신 Cell의 payload × 그 수신 Cell의 유저 수**를 모든 Cell에 대해 합산해야 한다.
`20B × 9 × 40 × 40`은 수신 Cell 하나의 전송량이므로, 모든 Cell이 9칸을 보는 모델에서 Zone 전체 전송량을 구하려면 Cell 25개를 합산해야 한다.

모든 Cell이 9칸을 본다고 단순화하면 AOI Cell 수의 합은 `25 × 9 = 225`다.
현재 5x5 Grid는 경계에서 이웃 수가 줄어들므로 실제 합은 다음과 같다.

```
모서리 4개 × 4칸 + 가장자리 12개 × 6칸 + 내부 9개 × 9칸 = 169
```

- Delta (경계를 반영한 균등 분포 모델, 20회/초만 전송한다고 가정)

```
20B × Cell당 송신 유저 40명 × Cell당 수신 유저 40명 × AOI Cell 수 합 169
= 5,408,000 byte / broadcast / zone
× 20회/초 × 8 = 865,280,000 bit/sec → 약 865.28 Mbps / zone
```

- Full (경계를 반영한 균등 분포 모델, 현재 1초 주기)

```
43B × 40명 × 40명 × 169
= 11,627,200 byte / broadcast / zone
× 1회/초 × 8 = 93,017,600 bit/sec → 약 93.02 Mbps / zone
```

__예상 bps(bit per second)__

| 균등 분포 모델 | Delta만 20회/초 | Full만 1회/초 | 두 전송량의 단순 합 |
|---|---|---|---|
| 모든 Cell이 9칸을 보는 단순화 (AOI 합 225) | 1,152 Mbps | 123.84 Mbps | 1,275.84 Mbps |
| 현재 5x5 Grid의 경계 반영 (AOI 합 169) | 865.28 Mbps | 93.02 Mbps | 958.30 Mbps |

단순 합은 Full 전송 시에도 Delta를 생략하지 않는다고 가정한 payload 예산이다.   
현재 `ZoneThreadSet::WorkerFunc`는 Full을 전송한 틱에는 Delta를 생략한다.  
따라서 지연 없이 1초에 Full 1회와 Delta 19회를 수행하는 명목 스케줄에서는 다음과 같다.  

```
(5,408,000B × 19 + 11,627,200B × 1) × 8
= 915,033,600 bit/sec → 약 915.03 Mbps / zone
```

이 조건에서는 **Zone 하나의 Character payload만으로 1Gbps에 근접**한다.
동일한 1,000명 조건의 Zone 20개라면 Character payload는 약 **18.30Gbps**다.

Full을 3초 주기로 변경하는 가정이라면 Full 자체는 약 31.01Mbps/Zone이다. 이는 현재 설정이 아닌 별도 가정이며, Delta를 생략하는 횟수도 달라진다.

위 수치는 균등 분포·전원 2필드 변경을 가정한 이론값이며 실측 처리량이나 수용 인원을 뜻하지 않는다.  
실제 전송량은 Cell별 유저 쏠림, 변경 필드 수, 스냅샷 전송 빈도에 따라 달라지고, Monster/ActionResult·패킷 헤더·ACK/재전송 등 추가 비용도 발생한다.  
지원 인원은 대역폭뿐 아니라 CPU, Zone 틱 지연, 큐 drop을 함께 측정해 판단해야 한다.  

__작은 테스트 맵과 고밀도 조건__

이 프로젝트의 Zone은 포트폴리오 시연을 위한 100×100 크기이며, 전환 영역을 포함한 110×110을 5×5 Cell로 나눴다.  
AOI는 게임 시야에 맞춰 주변 3×3 Cell로 설정했다. 작은 맵에서 시야가 차지하는 비중이 크므로, 같은 Zone에 다수의 더미 유저를 배치하면 시야 내 관측 대상도 많아진다.  
위 대역폭은 이러한 작은 맵의 고밀도 조건을 유지한 계산이며, 일반적인 MMORPG의 전체 접속자 수에 그대로 적용할 수 있는 값은 아니다.  

같은 총인원을 더 넓은 맵에 분산하고 시야는 유지하면, 유저당 관측 대상이 줄어 전송량도 낮아질 수 있다.  
따라서 대역폭 수치를 낮추기 위해 시야를 축소하기보다, 실제 맵의 인구 밀도와 **시야 내 평균·최대 엔티티 수**, 상태 변경 빈도를 함께 제시해야 한다. 도시·보스전처럼 사람이 몰리는 구간은 별도 부하 조건으로 검증해야 한다.  
AOI 적용 전후 절감률과 맵 크기의 관계는 [AOI.md](AOI.md)에 정리했다.  

__Zone 처리 관련 주의점__
- Zone은 1개의 전용 코어를 필요로 한다.  
- Zone 내부에서는 Cell 단위, AOI 범위 단위로 Skill, Hit, 상태 전파 처리
- Skill Hit 판정 시 AOI 범위 내 모든 유저를 참조해야 하므로 TPS 유지 관점에서 Zone Depth(유저 수) 제한 필요
- Zone 내부 TPS 한계와 AOI 처리 비용은 실제 테스트를 통해 측정 필요


## 5. 참고 
- [ZoneState_Snapshots.cpp](CoreLib/ZoneState_Snapshots.cpp)  
- [PacketTypes.h](CoreLib/PacketTypes.h)
