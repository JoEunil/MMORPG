# 거래소 구매·기존 배송 연결 계약

2026-10-20. 읽기 기준 `20b365e2f935d86f72f3808600cb547f56374685`와 현재 `codex/market-purchase` 작업 후보. 판매대금 수령 MARKET01 승인과 별개인 다음 후보다. 이 분석은 문서만 작성하며 제품코드·DB·서버·Git 게시를 변경하지 않는다. 실행 서버와 후보가 동일하다고 가정하지 않는다.

## 범위와 정확한 외부 wire

기존 구매 procedure와 배송 처리만 외부에 연결한다. 등록·취소·재화 지급·스키마·수수료·자기구매 정책 변경은 제외한다. `CoreLib/PacketTypes.h`는 현재 opcode31까지이며32~35는 비어 있다. `MAX_DEFINED_OPCODE`를35로 확장해야 `NetLibrary/ClientContext.cpp:47`의 기존 상한 검사를 통과한다. 내부MSG 번호와 TCP번호는 독립이다.

헤더는 기존 packed8바이트(magic:u16,length:u32,opcode:u8,flags:u8), 현재 Windows little endian, flags0. body는 아래 순서로 고정한다.

| 기능 | opcode | body | 총길이 |
| --- | --- | --- | --- |
| 구매 요청 | 32 | requestID:u32, listingID:u64 | 20 |
| 구매 응답 | 33 | requestID:u32, listingID:u64, status:u8, itemID:u32, quantity:u32, diamondSpent:u64 | 37 |
| 기존 배송 요청 | 34 | requestID:u32 | 12 |
| 기존 배송 응답 | 35 | requestID:u32, status:u8, deliveredCount:u32, duplicatedCount:u32, blockedCount:u32 | 25 |

각 요청/응답 requestID는0거부, 구매 listingID도0거부. 실제길이/header길이/flags/opcode를 정확히 검사하며 잘림·추가바이트를 거부한다. 구매 외부 status는 **0/1/3/4/5**를 허용한다. **5는 DB업무결과가 아닌 결과미확인**이다. 성공은 itemID·quantity 양수, 그 외 itemID/quantity/diamondSpent는 모두0. 가격0은 기존DB가 허용하므로 클라이언트에서 새 최저가격을 만들지 않는다. 배송 status0/1, count합계는 query18의 최대30을 넘지 않는다. 실패status0에도 정상적으로 처리된 앞부분의 count가 있을 수 있으므로 count0만 강요하지 않는다.

내부 `MsgBazaarBuyBody`·`MsgBazaarCheckOutboxBody`에는 requestID를 추가한다. 응답에는 requestID·characterID를 포함하고 BUY quantity를u32, diamondSpent를u64, 배송 세 count를u32로 맞춘다. 기존 이름을 사용하며 불필요한 중복상태는 추가하지 않는다. ABI가 바뀌므로 Core/Cache/MainServer는 같은 소스로 함께 빌드한다.

## 구매: DB COMMIT의 의미

`CacheLib/Handler_Bazaar.cpp::BazaarBuy` → `Config.h` query16 → `Resources/DB/procedure.sql:4` `sp_bazaar_buy`:

1. listingID의 TRADING행을 FOR UPDATE 잠근다. 다른 구매와 같은 listing 재요청은 이 경계에서 직렬화된다.
2. buyer의 `characters_diamond`에서 가격만큼 diamond 차감·total_spent 증가. 부족/통화행없음은 result4로 ROLLBACK.
3. SOLD·buyerID 변경, 거래log, seller claim READY, buyer_outbox READY를 한 transaction으로 COMMIT한다.
4. result1+item_id+quantity+price를 반환한다. 대상없음/이미판매는0, procedure의 SQL예외 ROLLBACK은3이다. **DB반환 집합은0/1/3/4**다. 기존 내부 주석의result2는 procedure 반환이 아니므로 외부에서 지원한다고 선언하지 않는다.

BUY status0/3/4는 실제 procedure 결과를 읽고 유효성을 확인한 경우에만 사용한다. SELECT null/빈결과, COMMIT 후 결과파싱예외, DBWorker 연결획득/작업예외와 enqueue거절은 외부5로 보수적으로 처리한다. DBWorker 예외를3으로 반환하면 COMMIT된 구매를 미구매로 오인할 수 있다. 5는 '구매 결과 미확인, 자동 재구매 없음, 배송·가방 확인'으로 표시하며 같은세션의 해당listing 재구매를 잠근다. 실행전 queue거절도5로 통일해 불확실한 경계에서 재구매를 유도하지 않는다. 이는 새 경제정책이 아닌 외부 관측상태다.

기존 query는 buyerID를 신뢰하므로 외부 NonZone에서 **현재 GetFieldCharacterID(session)**만 사용한다. 클라이언트 characterID는 받지 않는다. 응답도 같은 bound character와 일치할 때만 보낸다. 소유권/필드상태 실패는 요청을 처리하지 않는다. 조회한 가격을 요청으로 다시 받아 신뢰하지 않으며 구매 가격은 기존 DB 판정이다.

DB의 price는u64, quantity는u32다. 현재 handler의 `getUInt("price")`와 `static_cast<uint16_t>(quantity)`를 그대로 재사용하면 COMMIT 후 응답 절삭/예외가 날 수 있다. 구매 결과를getUInt64/getUInt로 원폭 보존하고 내부·wire·ClientCore도 같은 폭을 쓴다. item_id BIGINT를 읽을 때도u32범위를 확인하고 묵시절삭하지 않는다. 결과 파싱 오류는 결과 미확인으로 표시하며 이미 성공한 DB 거래를 실패했다고 보상/재구매하지 않는다.

quantity가65535초과인 기존 매물은 구매 procedure가 금지하지 않지만 `DeliverItem`은 BLOCKED로 남긴다. 새 가격/수량 정책이나 procedure를 추가하지 않고 구매성공과배송보류를 구분한다. 실거래에는 기존 inventory가 수용할 수 있는 확인된 매물을 선택한다.

## 배송: cache 반영과 영속화는 구분

`BazaarCheckOutbox` → query18은 **자신의 READY event를 시간/eventID순 최대30개** 읽는다. 이 호출은 조회 전용이 아니라 inventory를 바꾼다. 구매응답의 item을 클라이언트가 직접 추가하지 않는다.

- `CacheStorageInventory.cpp::DeliverItem` AVAILABLE이면 item과eventID를 cache에 함께 반영하고 delivered++. DUPLICATED면 이미 반영된event이므로 중복지급 없이 duplicated++.
- BLOCKED는 가방/스택/ring포화 또는 유효하지 않은 배송값 등이다. blocked++ 후 뒤 event 처리를 멈춘다. DB_READING/EVICTING/EMPTY 등은 status0으로 중단한다. 이미 처리한 앞부분이 있을 수 있다.
- `CacheFlush.cpp:34-120`은 game inventory blob 쓰기성공 이후 query19로 outbox CLAIMED를 바꾸고 inbox head를 전진시킨다. eventID와item은 같은blob에 저장되어 재시도/재접속 중복배송을 방지한다. flush실패는 재처리 대상이며 기존 흐름을 유지한다.

따라서 배송응답 **status1은 한 pass의 정상 처리**, deliveredCount는 cache반영event수다. status1+blocked>0도 가능하고 전체 수령완료/영속화 완료가 아니다. duplicated는 추가지급이 아니다. status0도 앞선배송을 취소하지 않는다. 0/0/0이면 이번에 처리할 READY가 없다는 뜻이며 직전구매의 영속화를 증명하지 않는다. 수치는 event개수이지 아이템수량이 아니다.

UI는 구매성공에 '구매 완료, 배송 확인 필요', 배송결과에 '반영 N건/기존반영 N건/보류 N건'을 보여주고 BAG 재조회로 현재item을 확인한다. durable완료는 wire에 ACK가 없으므로 UI에서 주장하지 않는다. 실증은 정상 flush 후 read-only DB에서 inventory blob과 해당outbox CLAIMED를 별도 관찰한다. flush기간의 근거 없이 강제flush/DB직접수정하지 않는다.

## 좁은 구현 지점과 비동기 소유권

`PacketTypes/Writer`, `MessageTypes`, `NonZoneHandler`, `MessageQueueHandler`, Cache의 기존 두 handler, ClientCore Packet/NetworkService/Interfaces/ViewModel만 필요한 연결을 추가한다. 기존 MQ는 메시지복사형이며 NonZone 원본과 Cache 비동기원본을 각각 정확히1회 Return한다.

현재 두 handler는 body포인터를 captured message 안에 보존하고 `EnqueueMessageWork` 예외시 Return만 한다. 연결 때 수령 handler처럼 요청을 값복사, 응답 value-initialize, 공통finish에서 requestID/characterID/listingID를 보존하고 response enqueue·원본Return을 수행한다. DBWorker exception/onFailure와 queue거절은 구매5/배송0의 상관있는 응답을 만들며 `msg=nullptr`로 바깥Process의 중복Return을 막는다. Core/Cache dispatch에 정확 내부length검사를 둔다. response queue거절은 로그와 UI timeout으로 남고 이미COMMIT/배송한 상태를 취소하지 않는다. queue/lock/pool 재설계는 필요 없다.

배송 work는 localresponse를 두고 try/catch하여 이미 확인된 prefix count를 status0과 함께 보존한다. 외부 onFailure는 연결획득실패 등 work를 시작하지 못한 경우의0counts를 반환한다. 배송 중 예외는 일부event가 이미 cache반영됐을 수 있고, 실패응답0/0/0도 '아무것도 지급되지 않았다'는 근거로 사용하지 않는다. UI는 **부분 반영 가능, 가방 확인 필요**로 표시하고 자동재시도하지 않는다. Delivery를 수동 재요청하더라도 기존event dedup을 사용하며 별도고유지급 시스템을 추가하지 않는다.

## ClientCore 최소 API

- UI: `RequestBazaarBuy(ulong listingID)`, `event Action<ulong,byte,uint,uint,ulong> OnBazaarBuyReceived` (listingID,status,itemID,quantity,diamondSpent).
- UI: `RequestBazaarCheckOutbox()`, `event Action<byte,uint,uint,uint> OnBazaarCheckOutboxReceived` (status,delivered,duplicated,blocked).
- Network: `RequestBazaarBuy(uint requestID,ulong listingID)`, `RequestBazaarCheckOutbox(uint requestID)`.
- Net→VM: `BazaarBuyReceived(uint requestID,ulong listingID,byte status,uint itemID,uint quantity,ulong diamondSpent)`, `BazaarCheckOutboxReceived(uint requestID,byte status,uint delivered,uint duplicated,uint blocked)`.

기존 BazaarRequestKind에Buy/CheckOutbox를 추가해 single pending/증가requestID를 재사용하고 Buy는listingID도 검사한다. 취소·timeout·패널닫기·연결종료가 dispatcher 안의 늦은응답을 무효화한다. `CancelBazaar`는 DB취소가 아니다. 수령과 동일하게 UI가행을보존하며 구매중검색/중복클릭을막는다. 성공또는결과불명인 listing은 같은세션에서 재구매하지 않는다. 배송은 별도 명시적 버튼으로 기존READY를 요청해 재화지출과 분리하고 자동반복하지 않는다. 배송응답 후 기존 `RequestInventory()`를 사용하며 inventory패킷을 새로 만들지 않는다.

## 최소 검증·데이터·복구

핵심자동2~3개는 BUY/배송 wire폭과상태·세션소유권/queue예외1회Return, ClientCore 상관관계·중복클릭·timeout, 기존DeliverItem dedup·blocked표시를 나눠 묶는다. 새로운 전체거래소/게임회귀는 기본실행하지 않는다. mock정상응답은 실제DB지급성공으로 보고하지 않는다.

Coordinator가5011의 현재diamond·기존TRADING매물·배송상태를 읽기로 확인할 예정이다. 이전관찰5011/5012 inventory빈행·내매물0만으로 현재구매가능/불가능을 단정하지 않는다. 정상전용계정 구매는 일상테스트범위지만 재화/매물/배송/가방직접seed·초기화는별도사전승인이다. 데이터가없으면 정상구매실증미실행과거절/빈결과를 구분하며 임의seed하지 않는다.

실거래가능시 한로그인에서 한매물구매→배송1pass→BAG조회→정상퇴장→read-only영속결과를확인한다. 이전완료시험을반복하지않는다. 복구는정상서버종료후이전일괄빌드/DLL/UI복원이며 이미정상COMMIT된구매/배송을되돌리려고DB를수정하지않는다. 공개게시승인은이후해당묶음의검증후별도요청한다.
