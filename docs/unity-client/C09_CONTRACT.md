# C09 거래소 첫 연결 — 읽기 전용 검색

2026-10-19 서버 분석 담당. 조사 HEAD `d7524822a8237cfec023f94cb18dc6abb8bb5a16`; 현재 C08의 CoreLib/ClientCore 미커밋 후보를 읽기 기준으로 보존했다. 소유는 이 문서뿐이며 제품·서비스·Editor·테스트·Slack·DB 조작은 하지 않았다. 실행 바이너리에 아래 계약이 연결됐다고 추정하지 않는다.

## 다음 작업 권고

**기존 거래소의 판매 중 목록을 유형별로 검색해서 표시하는 읽기 전용 화면**으로 첫 작업을 제한할 수 있다. 기존 Cache BazaarSearch→SELECT query12를 연결한다. 검색/새로고침/페이지 전환/닫기만 제공한다. 등록·구매·취소·수령·배송 확인 버튼이나 빈 더미 화면은 추가하지 않는다.

DB 행이 없어도 실제 검색 성공의 빈 목록을 확인할 수 있다. 양수 목록은 기존 실제 행이 있으면 관찰하고, 없으면 fixture의 표시 검증과 실DB 빈 응답 검증을 구분한다. 성공 화면을 만들기 위해 등록/seed/재화를 생성하지 않는다.

BRIEF의 S04 전체 목표에는 향후 등록/구매/수령·두 계정 테스트가 있지만, 이번 첫 연결의 완료로 이를 전부 구현했다고 보고하면 안 된다.

## 실제 접속 경로

- `CoreLib/PacketTypes.h`: TCP opcode는1~25까지만 정의되며 거래소 opcode가 없다. `NetLibrary/ClientContext.cpp`도 MAX_DEFINED_OPCODE 초과를 거절한다.
- `NonZoneHandler::Process`: AUTH/목록/진입/채팅/Zone/인벤토리/profile만 처리한다. Bazaar TCP 요청 entry가 없다.
- `CoreLib/MessageQueueHandler::Process`: 내부 Bazaar 응답 switch가 없어, Cache 응답을 TCP로 전달하는 경로도 없다.
- `CacheLib/Handler.cpp`: 내부 MSG_BAZAAR_SEARCH18~31 계열 중 Search는20/21, MyList는18/19이며 등록/취소/구매/수령/Outbox도 dispatch한다. **내부 messageType 숫자를 TCP opcode로 재사용하지 않는다.**
- `MainServer/IntegrationTestBazaar.h`: MakeMsg가 내부 메시지에 characterID를 직접 넣고 CacheNode의 MessageQueue로 보낸다. MockMessageQueue 결과를 읽는 테스트이며 실제 게임 TCP/세션 소유권 검증의 증거가 아니다.

기존 Cache 기능은 존재하므로 새 거래 시스템을 만드는 작업이 아니다. 기존 내부 기능을 인증된 TCP 경로에 연결하는 최소 프로토콜 후보가 필요하다.

## 기존 조회 계약과 필요한 보완

`MsgBazaarListing`: listingID:u64, sellerCharacterID:u64, itemID:u32, quantity:u16, price:u64, registeredAt:u64, status:u8. 내부 구조는 packed가 아니므로 wire로 memcpy하지 않는다.

| 내부 명령 | 실제 SQL/효과 | 현재 문제 |
| --- | --- | --- |
| Search(MSG20) | item_type:u16/page:u16, query12. item_type일치 AND status=TRADING, listed_at DESC, LIMIT30 OFFSET(page×30). 순수 SELECT | nullptr(DB실패)와 결과0행 모두 status1,count0으로 처리하여 오류를 빈 성공처럼 표시 |
| MyList(MSG18) | characterID:u64, query11. seller_id일치 AND status!=CANCELLED, LIMIT30, 별도 정렬/페이지 없음. 순수 SELECT | 정상0행을 status0로 처리하고 실패 count/배열 초기화 없음. status SQL은TRADING0/SOLD1/그외0으로 매핑하므로 CLAIMED 등을TRADING처럼 해석할 위험 |

Search만 첫 단위로 연결하면 MyList 정책·소유권 UI와 잘못된 상태 매핑을 이번 범위에서 분리할 수 있다. MyList를 함께 넣으려면0행 성공/실패 구분과 실제 상태 도메인을 별도 후보에 명시해야 한다. query11을 의미 변경 없이 안전하게 표시할 수 있다고 단정하지 않는다.

### 반드시 먼저 해결할 메시지 용량

Core/Cache 양쪽 MessagePool은 `MESSAGE_LEN=1024`의 고정 Message를 준다. 내부 MsgBazaarListing은 자연 정렬의 Windows x64에서48바이트가 예상되며,30행 body와 header는 약1464바이트다. `sizeof(MsgStruct<MsgBazaarSearchResBody>)`가1024보다 크다. 실제 응답은 fixed size 전체를 SetLength하고 MessageQueue가 그 길이로 복사하므로,0행이어도 기존 full-struct copy 자체가 할당 용량을 초과한다.

새 TCP 연결 전에 `CoreLib/Config.h`, `CacheLib/Config.h` 양쪽 메시지 용량을 해당 최대 응답 sizeof 이상으로 맞춰야 한다. 단순 후보는2048이다. Cache pool10000개 기준 추가 약10MB, Core pool100개 기준 약100KB의 고정 버퍼 비용이 생긴다. 최종 수치는 실제 sizeof/할당 기준으로 검증한다.30행을 임의로 줄여 기존 계약을 바꾸거나 낮은 count만 테스트해서 문제를 숨기지 않는다.

동일 응답의 sizeof<=MESSAGE_LEN compile-time guard를 적절한 구현 파일에 넣고, Core/Cache queue full-copy 경로를 함께 확인한다. 신규 request correlation 필드로 구조 크기가 달라지면 실제 sizeof를 기준으로 한다. Message의 m_capacity가 있다고 SetLength/copy가 자동 범위 검사하는 것은 아니다.

### Search handler 최소 수정

`CacheLib/Handler_Bazaar.cpp::BazaarSearch`에서 입력 필드를 로컬로 먼저 보존한 후 응답 body를0으로 초기화한다(요청/응답이 같은 Message buffer). query 실패 nullptr는 status0/count0, 유효 ResultSet의0행은 status1/count0으로 구분한다. 최대30행만 채우며 DB→u32/u16 축소값을 검사한다. SQL 예외를 성공으로 처리하지 않는다. 실패/부분 로드 잔여 레코드를 송신하지 않는다.

현재 EnqueueMessageWork는 queued가 아니면 false이고 해당 호출자가 이를 무시한다. 최소 후보는 검색 enqueue 실패를 명시적 status0 응답으로 바꾸거나, 적어도 클라이언트가8초 같은 제한 시간 뒤 실패 잠금으로 끝나도록 한다. 어떤 경로든 성공으로 표시하지 않는다. Message 소유권 이전/반환은 기존 helper 계약을 지킨다.

## 최소 새 wire 후보

다른 미완료 작업과 opcode 충돌이 없는지 구현 직전 재확인한다. 현재 추가 사용이 없으면26/27을 Search 요청/응답으로 제안한다. flags0, 공통 packed little-endian header8바이트를 쓴다.

- 요청26: `requestID:u32, itemType:u16, page:u16` → 전체16바이트. characterID/userID는 받지 않는다.0 requestID는 거절한다.
- 응답27: `requestID:u32, status:u8, count:u16, rows[count]` → 전체15+39×count바이트, 최대1185바이트. row는 기존 listing 필드를 위 순서의 packed39바이트로 명시 직렬화한다. 실패는 count0. 레코드 배열의 미사용 영역은 송신하지 않는다.
- 최대1185바이트이므로 서버 normal PacketPool512를 사용하면 안 된다. 기존 bigPacketPool을 사용한다. 신규 풀/범용 packet framework는 필요 없다.
- `MAX_DEFINED_OPCODE=27`, 새 body/parser의 엄격한 길이·flags·status0/1·count<=30·record 범위 검사. 기존 enum/model에 숫자만 추가해서 지원 완료로 보지 않는다.
- 내부 MsgBazaarSearchBody/ResBody에 requestID를 왕복시킨다. 이는 async 완료의 늦은 응답/검색 조건 변경을 판별하는 작은 필드이며 새로운 세션 매니저를 만들 이유가 아니다. 응답에서 요청ID를 먼저 읽고 초기화 후 복사한다.

기존 opcode를 바꾸지 않으므로 이전 기능 wire는 유지되지만, 구 서버는26을 거절한다. 새 서버+DLL 후보를 함께 검증하고 구 서버 연결을 호환 성공으로 주장하지 않는다. ClientCore가 기존 경계에서 새 parser/ViewData/ViewModel 이벤트를 제공하고 Unity는 그 결과만 표시한다.

## 인증·세션·소유권

Search는 공개 판매 목록 조회지만 게임 인증을 우회해 열지 않는다. NonZone entry에서 정확한 요청 크기/flags를 검사하고, 해당 세션이 인증 완료·선택 캐릭터 귀속·field Zone>0인지 기존 StateManager의 안전한 단일 조회 경계로 확인한다. 별도의 Client가 보낸 characterID를 신뢰하지 않는다. 기존 dispatcher의 HealthCheck만으로 필드 준비/선택 캐릭터까지 검증됐다고 추정하지 않는다.

응답은 원 sessionID에만 보내며 닫힌 세션을 재생성하거나 다른 캐릭터 세션으로 재배정하지 않는다. ClientCore는 한 번에 하나의 검색만 보내고 requestID가 현재 pending과 일치할 때만 소비한다. Close/Zone 전환/Disconnect/Dispose에서 UI와 pending을 정리하고 늦은 결과를 버린다. 전환이 끝나고 사용자가 다시 열면 새 요청을 보낸다. timeout 뒤 같은 ID를 재사용하지 않는다. 재접속은 새 Client 수명과 기존 socket cleanup 경계를 따른다.

MyList/등록/취소/구매/수령/Outbox는 characterID가 필요한 내부 명령이다. 향후 wire에는 characterID를 받지 않고 StateManager에서 확인한 소유 캐릭터를 붙여야 한다. Cancel/Claim의 DB 프로시저가 listing 소유권을 확인하더라도, 게임 세션→characterID 바인딩을 대신하지 않는다.

## UI와 검증 범위

기존 itemMap의 itemType은1(포션),2(무기·방패),3(갑옷)이다. Search SQL은 item_type=?이므로0을 전체 검색이라고 보내면 안 된다. 현재 존재하는 유형1~3만 선택하며, 이름/아이콘이 아직 연결되지 않은 itemID는 실제 ID로 표시한다. 없는 이름·판매자 이름·재화 잔액·수수료·예상 총액을 합성하지 않는다. 가격/수량/등록 시각은 반환값 그대로 표시한다.

목록은 최대30행, page는 기존 zero-based u16이다. 다음 페이지의0행도 정상 결과로 표시하고 이전 페이지 복귀를 허용한다. 같은 listed_at의 정렬은 query12가 보장하지 않으며 실시간 거래 중 완전한 페이지 안정성을 주장하지 않는다. UI클릭/스크롤/입력이 이동·스킬로 전달되지 않게 기존 HUD 입력 차단 경계를 재사용한다.

구현·리뷰를 마치고 한 배치에서 다음만 검증한다.

1. Core/Cache sizeof와 queue copy 용량,0/30행·DB실패·enqueue실패 writer/parser 회귀.
2. 인증 전/캐릭터 미선택/field 미진입/잘못된 flags·길이·type·ID 요청 거절. requestID 불일치/늦은 응답/timeout/전환/재접속 cleanup.
3. 실제 로그인→필드→거래소 Search→유형/페이지/새로고침→닫기. SQL 조회 실제 응답과 UI 일치, 패널 클릭 전후 이동·HP/MP 불변. 실제0행과 fixture30행을 구분한다.
4. 보존된 기존 기능 회귀 중 새 wire/메시지 용량에 영향받는 항목만 실행. 실제 DB SELECT 외 테스트 거래/seed/초기화는 수행하지 않는다.

## 쓰기 기능과 실행 금지 경계

Register는 inventory 제거와 gold 등록비(최소10 또는 price/10), query13 INSERT/실패 복원을 수행한다. Buy는 sp_bazaar_buy에서 diamond 지출·매물 상태·Outbox를 변경한다. Cancel은 sp_bazaar_cancel 및 아이템 배송, Claim은 sp_bazaar_claim의 diamond 지급, CheckOutbox도 DeliverItem 및 후속 durable flush/CLAIMED를 유발한다. 이름이 Check/조회처럼 보여도 Outbox 명령은 읽기 전용이 아니다.

`TEST_BAZAAR`/IntegrationTestBazaar는 char50~110의 bazaar_log/buyer_outbox/bazaar DELETE, 아이템·재화 변경, 테스트 INSERT, crash abort 등을 포함한다. 기존 두 전용 계정 테스트 권한을 이 파괴적 통합 테스트 실행 허용으로 확대하지 않는다. 해당 환경변수로 서버를 실행하지 않는다.

쓰기 기능의 구현 후보는 별도 단위로 분석·로컬 fixture 검증할 수 있지만, 실제 테스트 거래·seed/삭제/재화 조작은 Coordinator가 사용자 사전 경계에 따라 판단해야 한다. 이번 읽기 후보는 그런 작업 없이 진행할 수 있다. 후보 완성·독립 리뷰·배치 결과 준비 뒤 PR 직전 승인만 요청하며, 계획만으로 미리 승인 대기를 만들지 않는다.
