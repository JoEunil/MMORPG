# MMORPG GAME PROJECT

이 프로젝트의 전체 설명과 상세 내용은 아래 링크에서 확인할 수 있습니다.
🔗 (Notion): https://www.notion.so/2c4984fabda080c9bf2fe3eba74a0326?source=copy_link

## 소개
MMORPG 서버 구조를 직접 설계 및 구현한 프로젝트입니다.
C++ 기반 게임 서버 / Redis 인증 / NodeJS 로그인 서버 / Unity 클라이언트로 구성됩니다.
<img width="1314" height="681" alt="image" src="https://github.com/user-attachments/assets/ab3488ac-d810-4348-8f43-3952c55f8abe" />


## 주요 특징
- IOCP 기반 비동기 네트워크 구조
- RingBuffer 기반 TCP 패킷 처리
- Session & Internal Entity 분리 설계
- Full/Delta Snapshot 상태 동기화
- Zone 기반 스레드 모델 및 Tick 시스템
- Sharding + LRU 캐시 서버

## 코드 구조
- NetLibrary : IOCP, Session, ClientContext
- CoreLib : Zone, Snapshot, Broadcast, 기타 게임 로직
- CacheLib : Inventory 데이터 캐시, DB 연동
- ExternalLib : Redis 인증, 로그
- ClientCore : .NET 클라이언트 네트워크 로직
- Unity : 게임 렌더링 및 입력 처리
