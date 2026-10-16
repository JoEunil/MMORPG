# AOI(Area of Interest) 적용

## 1. 개요
이 문서는 MMORPG 서버에서 Zone을 Cell 단위로 분리하고 AOI(Area of Interest)를 적용한 개선 작업에 대한 기술적 설명이다.  

## 2. 문제상황
- 스킬 기능을 추가하면서, Zone 내 유저 수가 많아지면 AOI 탐색 Depth가 깊어지고,
이는 Zone 스레드의 TPS에 영향을 미칠 위험성을 인지하였다.
- Zone을 Cell 단위로 나누어서 처리했고, Cell에 AOI를 적용하기 위해서는 기존의 패킷 처리 로직을 그대로 사용할 수 없었다.

## 3. 수정사항
__구조 변경__  
- Zone 내부를 Cell 단위로 분리하여 패킷 전파 단위 축소
	- 기존: zone 유저수 x zone 유저수 x 패킷 필드
	- Cell 적용 후: Cell 유저수 x Cell 유저수 x Cell 갯수 x 패킷 필드
	- Cell 5x5 기준: 패킷은 평균적으로 1/25 줄어든다. (AOI 적용 전)
- AOI 처리 시 개별 패킷을 보내는 대신 Chunk 단위로 묶어 전송
- overlapped 구조체를 일부 수정하고, 별도의 Send 메서드를 추가하여 Chunk 전송 처리

__처리 방식__  
[IOCP.cpp chunk 패킷 send 메서드](NetLibrary/IOCP.cpp)
```cpp   
void IOCP::SendDataChunks(uint64_t sessionID, std::shared_ptr<Core::IPacket> packet, std::vector<std::shared_ptr<Core::IPacket>>& packetChunks)
{
    ~~
    pOverlappedEx->wsaBuf[0].len = packet->GetLength();
    pOverlappedEx->wsaBuf[0].buf = reinterpret_cast<char*>(packet->GetBuffer());
    pOverlappedEx->sharedPacket = packet;
    pOverlappedEx->packetChunks = packetChunks;
    for (auto& chunk : packetChunks)
    {
        pOverlappedEx->wsaBuf.emplace_back(chunk->GetLength(), reinterpret_cast<char*>(chunk->GetBuffer()));
    }
    ~~
}
```
Chunk 단위 전송 처리 방식  
- 기존 Packet 클래스를 그대로 사용
- 패킷 헤더는 기존 경로로 처리
- 패킷 바디는 각 Cell을 Chunk 단위로 분리 후 합쳐서 전송
- overlapped 구조체에서 wsaBuf를 vector에 담아 WSA Send call 한 번으로 패킷 전송 가능.
- 현재 Grid 기반 AOI를 사용하며, 각 Cell의 AOI 단위는 미리 정의되어 있음

## 4. 설계 결정 및 트레이드오프
- [ClientContext 리팩토링](ClientContextRefactor.md)에서 Chunk 단위 전송을 고려했으나, 당시에는 필요성 낮음 판단 → 보류
- Cell, AOI 적용
	- 패킷 크기 감소, Skill 처리 탐색 범위 감소
	- 대역폭 계산  
	```패킷 크기 x 전송 대상 수 = (패킷 필드 1개 x 전송 대상) x 전송 대상 ```
	- 패킷 전송량은 전송 대상 수의 제곱에 비례
	- 따라서 Cell 단위 분리는 대역폭 절감 효과가 매우 큼
	- AOI 적용을 통해 예측 가능한 시야 범위 정보를 미리 로드하고, 불필요한 전송을 줄일 수 있음
- Chunk 단위 전송 처리 로직 추가 
	- 코드 복잡도 상승, 
	- 패킷 절감 효과 발생

## 5. Delta 패킷 전송량 비교 (AOI 적용 전/후)
__패킷 구조__
```cpp    
struct PacketHeader { // 8바이트
    uint16_t magic = MAGIC;
    uint32_t length;
    uint8_t opcode;
    uint8_t flags = 0x00; // 첫번째 비트는 시뮬레이션 로직인지 나타냄, 0x01 ~ 0x80
};

struct DeltaUpdateField { // 10바이트
    uint32_t zoneInternalID;   
    uint16_t fieldID;
    uint32_t fieldVal; // field에 맞는 타입으로 변환 해서 사용
};

struct DeltaSnapshotBody {
    uint16_t count; // 2바이트
    DeltaUpdateField updates[DELTA_UPDATE_COUNT];
};
```
__Delta 패킷 크기__  
헤더 + Count 필드 크기: 8 + 2 = 10 byte  
Delta 필드 1개:  10 byte

__계산 조건__
- Zone 유저 수: 100명
- Cell 구성: 5 × 5 = 25개
- 각 Cell 유저 수: 100 / 25 = 4명
- AOI 범위: 3 × 3 Cell
- 유저당 Delta Update: 2개

__AOI, Cell 적용 전__
- Zone 내 모든 유저에게 전체 Delta 패킷 전송
- 패킷 크기(100명 기준):  
  `20 bytes × 100 + 10 = 2010 bytes`
- 전송 대상: 100명
- 총 전송량:  
  `2010 × 100 = 201,000 bytes`

__AOI, Cell 적용 후__  
각 Cell이 AOI(3×3) 기준으로 필요한 유저 그룹만 묶어서 전송  
- 1 Cell AOI에 포함되는 유저 수:  
  4명(Cell 당) × 9개 Cell = 36명
- Cell 1개 패킷 크기
  - Delta 크기: 36 × 20 = 720 bytes
  - 패킷 헤더 + Count: 10 bytes  
  → 총 730 bytes
- 총 전송량  
  `25 × 730 × 4 = 73,000 bytes`
  
  
약 64% 대역폭 절감
→ AOI + Cell 단위 분리 효과가 매우 큼

__맵 크기와 절감률의 관계__

이 프로젝트는 포트폴리오 시연을 위해 Zone을 100×100 크기로 작게 구성했다. 전환 영역을 포함하면 110×110이며, Cell 크기 22의 5×5 Grid로 나뉜다.  
AOI는 게임 시야를 기준으로 주변 3×3 Cell에 맞췄다. 내부 Cell 기준 전송 영역은 66×66으로, 작은 Zone에서 차지하는 비중이 크다.  

위 계산은 모든 Cell이 9칸을 본다고 단순화했으므로, 유저 한 명에게 전체 25개 Cell 중 9개 Cell(36%)의 상태를 전송한다.  
따라서 같은 인원을 더 넓은 맵에 분산한 경우에 비해 관측 대상이 많고, 전체 Zone 전파 대비 절감률도 제한된다. **약 64%는 현재의 작은 맵과 시야 비율에서 나온 계산값이며, AOI 방식 자체의 고정된 효율 한계는 아니다.**  

동일한 시야와 총인원을 유지하면서 맵을 넓혀 유저를 분산하면, 시야 내 인원이 줄어 전송량을 더 낮출 수 있다.  
실제 서비스에서는 전체 접속자 수뿐 아니라 시야 내 평균·최대 엔티티 수와 상태 변경 빈도를 기준으로 용량을 산정해야 한다. 도시·보스전처럼 유저가 밀집하는 상황은 별도로 검증할 필요가 있다.  
현재 5×5 Grid의 가장자리 이웃 수까지 반영한 대역폭 계산은 [Snapshot.md](Snapshot.md)에 정리했다.  

## 6. 시연

 ![GIF 로드 실패](images/BeforeAOI.gif)
>  AOI 적용 전 임시로 Cell 단위로 처리한 상태

 ![GIF 로드 실패](images/AfterAOI.gif)
>  AOI 적용 후  
*분홍색 선은 Cell 경계를 나타낸 것이다.
## 7. 참고
- 관련 PR: [grid 기반 AOI 적용 #20](https://github.com/JoEunil/MMORPG/pull/20)
- [Cell.h](CoreLib/Cell.h)
- [BroadcastThreadPool.cpp](CoreLib/BroadcastThreadPool.cpp)
- [ZoneState_Skills.cpp](CoreLib/ZoneState_Skills.cpp)
- [ZoneState_Snapshots.cpp](CoreLib/ZoneState_Snapshots.cpp)
