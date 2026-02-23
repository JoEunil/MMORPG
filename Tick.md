## Tick 처리 

## 1. 개요
게임 로직에서 Tick은 시간 단위로 게임 상태를 갱신하고 클라이언트에 동기화하기 위한 기준점을 의미한다.  
틱 단위로 서버는 캐릭터 상태, 스킬, 몬스터 행동, Snapshot 전송 등을 처리하여 게임 로직의 일관성을 유지한다.

## 2. 목적
Tick 처리의 주요 목적은 다음과 같다.
1. 게임 로직 일관성 유지
- 스킬 쿨다운, DPS, 몬스터 AI, 이벤트 처리 등 시간 기반 계산을 정밀하게 수행
2. Snapshot 전송 주기 보정
- Delta / Full Snapshot 주기를 Tick 기준으로 관리하여 클라이언트와의 상태 동기화를 일정하게 유지
3. 서버 부하 조절 및 Tick 보정
- Tick 지연을 보정하여 일정한 주기(GAME_TICK)로 로직 처리
- CPU burst 방지

## 3. Tick 처리 흐름
Zone 스레드는 Tick 단위로 다음과 같은 작업을 수행한다:
1. Skill Cooldown 처리  
	- Zone 내 모든 캐릭터의 스킬 쿨다운을 갱신
	- 활성 스킬의 phase 틱 갱신
2. 패킷 처리  
	- Zone의 Work Queue에서 패킷을 처리
	- 각 패킷에 대해 handler->Process() 호출 → 게임 상태 갱신
3. 게임 상태 업데이트
	- handler->ApplySkill(zoneID) : 스킬 적용
	- handler->UpdateMonster(zoneID) : 몬스터 AI 처리
	- handler->FlushCheat(zoneID) : 치트 상태 업데이트
4. Snapshot 전송
	- Full Snapshot: FULL_SNAPSHOT_TICK 주기
	- Delta Snapshot: DELTA_SNAPSHOT_TICK 주기
	- 시간 계산 후 적절한 Snapshot 전송
5. Tick 지연 보정
	- 현재 시간과 마지막 Tick 시간 차이를 계산
	- 남은 시간 만큼 sleep_for 수행
	- lastTick += GAME_TICK 으로 Tick 기준 보정

> Windows 타이머 해상도는 약 15.6ms이므로, 그보다 작은 지연은 sleep_for 호출이 의미가 없고,  
실제 보정은 16ms 이상 지연 발생 시만 수행된다.

## 4. 클라이언트 틱 처리
클라이언트에서도 서버 틱에 맞춰 상태 업데이트를 수행한다.
- Move와 같은 지속 입력 요청은 서버 Tick 기준으로 처리되므로, 별도의 송신 루프가 필요하다.
- 클라이언트 Tick은 프레임 단위와 일치하지 않음
	- 서버에서 수신한 Tick 기준 상태를 동기화
	- 중간 프레임은 보간 및 예측을 통해 부드럽게 표시
	  > 현재 보간만 적용된 상태.
> 즉, 클라이언트는 서버 Tick을 기준으로 게임 상태를 맞추고, 화면 표시 시 보간과 예측을 통해 자연스러운 움직임을 구현한다.

## 5. 참고
[ZoneThreadSet](CoreLib/ZoneThreadSet.cpp)