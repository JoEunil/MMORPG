# C10 후보 — 내 거래소 목록 조회

2026-10-19 서버 분석 담당의 **계약 조사·최소 연결 후보**다. 구현·리뷰·실행 통과나 게시 승인을 뜻하지 않는다. 제품 코드·DB·프로세스는 변경하지 않았다.

## 기준과 권고

조사 체크아웃 HEAD는 `df172f6e7c5f93610726ec752dcbb739a5d93dc5`이며 C09 미커밋 제품 변경이 존재한다. 이를 보존하고 실제 파일을 읽었다. HANDOFF에는 C09 전달 브랜치의 PR29 및 후속 승인 근거 수정 `0e2adcf`가 기록돼 있다. 이번 조사에서 원격 병합 상태는 새로 확인하지 않았다. 다음 구현은 원격/전달 후보와 조사 파일 차이를 먼저 대조한다.

디스크의 C09 후보 MainServer SHA256은 `ABA9491459EA2444D8862199104DAD8A8ADDBD19AA52687FBE085C9CC2C123F8`다. 이것은 파일 확인이며 현재 프로세스 실행 또는 C10 지원의 증거가 아니다. DB 실프로시저 정의는 조회하지 않았다. 아래 SQL 동작은 저장소 `Resources/DB/procedure.sql` 기준이다.

**다음 작은 흐름은 `MARKET → 내 매물 → 새로고침 → 닫기`를 권고한다.** 등록·구매·취소·수령에 앞서 이미 존재하는 MyList SELECT를 선택 캐릭터에 귀속해 연결한다. 실제 소유 목록과 빈 결과를 볼 수 있고, 거래·재화 변경 없이 세션 소유권과 후속 매물 선택의 기반을 검증할 수 있다. 쓰기 기능 버튼은 만들지 않는다.

## 현재 내부 기능과 실제 연결 상태

TCP는 `PacketTypes.h`에서 C09 Search 26/27까지만 있다. `NonZoneHandler::Process`, `MessageQueueHandler::Process`, ClientCore 응답 dispatcher도 거래소는 Search만 연결돼 있다. 다음 내부 번호는 **TCP opcode가 아니다**. 선언은 `CoreLib/MessageTypes.h`, dispatch는 `CacheLib/Handler.cpp`, 구현은 `Handler_Bazaar.cpp`다.

| 기능 / 내부 요청·응답 | 입력 → 현재 결과 | 실제 변경·제약 |
| --- | --- | --- |
| MyList 18/19 | characterID:u64 → status:u8/count:u16/listing 최대30 | query11 SELECT만. seller_id 일치, CANCELLED 제외, 정렬·페이지 없음. 0행을 실패로 처리하고 실패 count/미사용 행 초기화 없음. CASE가 CLAIMED도 TRADING0으로 바꿈 |
| Register 22/23 | characterID:u64/itemID:u32/quantity:u16/price:u64 → status:u8(0실패/1성공/2아이템/3골드/4입력) | 인벤 차감 → gold 등록비 max(10,price/10) → query13 INSERT. SQL/큐 거절 시 인벤·gold 보상 복구 시도. 단일 DB 트랜잭션/요청키 멱등 등록은 아님 |
| Cancel 24/25 | characterID:u64/listingID:u64 → status:u8 | query14 sp_bazaar_cancel. 소유 판매자 행 FOR UPDATE. TRADING→CANCELLED와 반환 Outbox를 함께 commit. 재요청은 기존 event 재사용. 즉시 DeliverItem 시도하나 공간/캐시 문제는 READY로 지연 |
| Buy 26/27 | characterID:u64/listingID:u64 → status:u8/listingID:u64/itemID:u32/quantity:u16/diamondSpent:u32 | query16 sp_bazaar_buy. TRADING 행 FOR UPDATE, buyer diamond 차감·SOLD·log·판매자 claim READY·구매자 Outbox READY를 같은 transaction에서 commit. 재구매는 성공 재생이 아니라 더 이상 TRADING 아님으로 실패 |
| Claim 28/29 | characterID:u64/listingID:u64 → status:u8/listingID:u64/diamondClaimed:u32 | query17 sp_bazaar_claim. seller_id 및 SOLD 검사, claim READY→CLAIMED CAS와 diamond 지급을 함께 commit. 재수령은 코드4이며 중복 지급 방어. 현재 handler가 응답 listingID를 채우지 않음 |
| CheckOutbox 30/31 | characterID:u64 → status:u8/delivered·duplicated·blocked 각u8 | query18 READY 최대30개 ASC, DeliverItem 실행. 이름과 달리 읽기 전용 아님. durable inventory flush 이후 query19 CLAIMED |

MyList row는 Search와 같은 내부 `MsgBazaarListing`: listingID:u64/sellerCharacterID:u64/itemID:u32/quantity:u16/price:u64/registeredAt:u64/status:u8. 자연 정렬 내부 구조를 wire로 memcpy하지 않는다.

쓰기 기능의 중요한 기존 의미:

- `MsgBazaarListing.price` 주석은 개당 가격이나 buy 프로시저는 **quantity를 곱하지 않고 listing.price 자체를 차감**한다. 연결 시 주석만 보고 합계·단가나 신규 거래 정책을 만들면 안 된다.
- 저장소 buy SQL에서 자기 매물 구매 거부 조건은 확인되지 않았다. 임의 정책을 새로 추가하지 않으며 외부 연결 전 기존 허용 여부를 명시한다. Cancel/Claim의 DB 소유권 검사도 TCP 세션 바인딩을 대신하지 않는다.
- DB price는 BIGINT인데 구매/수령 응답 및 diamond 잔액은 u32/INT다. 향후 쓰기 연결 전 축소·오버플로와 실패 결과 초기화를 별도 검토해야 한다. Register는 0가격도 현재 명시 거부하지 않는다.
- claim 프로시저는 `bazaar_claim.claim_status`만 바꾸고 `bazaar.status`를 CLAIMED로 바꾸지 않는다. 따라서 **MyList SOLD만으로 미수령/수령완료를 구분할 수 없다**. C10은 단지 판매됨으로 표시하고 수령 가능·정산 잔액을 합성하지 않는다.
- Register 인벤/gold 캐시 flush와 Bazaar INSERT는 서로 다른 영속화 경로다. 보상 복구 로그가 있다고 crash까지 원자적이라고 주장하지 않는다. 이번 C10에서는 수정하지 않는다.

## Outbox·중복 방지 경계

`CacheStorageInventory::DeliverItem`은 shard mutex 아래 eventID를 전체 recentEventIds ring에서 중복 검사한다. 캐시 미적재는 재시도, 인벤/수량/ring 용량 부족은 BLOCKED다. 아이템과 eventID를 같은 inventory blob에 기록하고 WAL 경로가 있으면 호출한 뒤 dirty로 만든다. ring은 유한하며 영구 무제한 멱등 키 저장소가 아니다.

`CacheFlush::DBWrite` case6은 먼저 game inventory blob을 durable write하고, 그 스냅샷의 pending event를 bazaar query19로 CLAIMED 처리한다. 성공 prefix만 head를 전진하고 dirty로 재저장한다. bazaar 접속/갱신 실패는 재flush로 수렴한다. 구매 성공은 즉시 인벤 수령 완료가 아니다. Cancel 반환도 이 경계를 사용한다. C10 MyList는 이 경로를 호출하지 않는다.

현재 `EnqueueMessageWork` 실패 callback은 메시지 반환만 하므로 기존 MyList DB 예외/거절에서 명시 실패 응답이 보장되지 않는다. C09 Search의 입력 스냅샷·zeroed 결과·실패 응답·단일 Return 소유권 경계를 좁게 재사용하는 후보가 필요하다. 범용 큐/락 재설계는 하지 않는다.

## C10 최소 연결 후보

1. **소유권:** `IsFieldSession`은 bool만 반환한다. 검증 후 별도 `GetCharacterID` 호출은 동일 스냅샷을 보장하지 않으므로, 기존 session shard shared lock 한 번으로 현재 auth 완료·userID·characterID·field zone을 확인하고 characterID를 얻는 좁은 경계를 사용한다. 새 전역 session manager나 락 종류는 필요 없다. 클라이언트 characterID 입력은 받지 않는다. 응답에서도 session 수명과 현재 선택 캐릭터 일치를 확인한다.
2. **조회:** query11의 seller 필터·CANCELLED 제외·최대30을 유지한다. 정렬/페이지를 추가하지 않는다. CASE는 TRADING0/SOLD1/CLAIMED2/알 수 없음255로 구분하는 최소 수정 후보다(스키마·정산 정책 변경 없음). parser는 0/1/2만 수용하고 잘못된 DB 값·축소 초과·부분 읽기는 전체 실패 처리한다. CLAIMED2는 기존 DB 상태명 표현일 뿐 정산 가능성 계산에 쓰지 않는다.
3. **오류/용량:** 입력을 먼저 복사하고 결과 구조 전체를0으로 초기화한다. 정상 ResultSet 0행은 성공, nullptr/예외/큐 실패는 status0/count0이며 requestID를 보존한다. 양쪽2048 메시지 풀에 새 응답 sizeof guard, 30행 memcpy 용량을 확인한다. C09와 별도 가변 풀은 만들지 않는다.
4. **wire 후보:** 구현 직전 충돌 재확인 후 TCP28 `BAZAAR_MY_LIST`,29 응답. packed little-endian 공통 header8/flags0. 요청은 requestID:u32만으로 전체12바이트; 응답은 requestID:u32/status:u8/count:u16+39바이트 row×count로 전체15~1185바이트. 기존 bigPacketPool과 엄격한 길이·flags·ID·count 검증을 재사용한다. 내부 MyList 요청/응답에는 requestID를 추가하며, 응답 소유 캐릭터 검증에 필요한 characterID를 내부에만 보존한다. TCP에 비밀/사용자 지정 소유자 ID를 넣지 않는다.
5. **클라이언트:** 기존 ClientCore dispatcher/ViewModel/ViewData 경계와 C09 패널을 확장해 판매 중 검색/내 매물 선택만 제공한다. 내 매물에는 페이지 버튼을 적용하지 않고 최대30개 제한을 명시한다. requestID별 한 pending, timeout8초, 탭 전환·닫기·Zone 이동·disconnect·dispose의 취소/늦은 응답 무시를 유지한다. 실제 ID·수량·가격·시간·반환 상태만 표시한다.

대상 파일 후보는 `CacheLib/Config.h`, `Handler_Bazaar.cpp`와 좁은 parser fixture, `CoreLib/MessageTypes.h`, `PacketTypes.h`, `NonZoneHandler.*`, `MessageQueueHandler.*`, `PacketWriter.*`, `StateManager.h`, `NetLibrary/ClientContext.cpp`, 관련 ClientCore 경계와 C09 화면/fixture다. 기존 C09/P28 보존과 Source/바이너리 일치를 확인한다. 선언 변경만으로 실행 지원 완료로 간주하지 않는다.

## 검증·권한·복구

구현·독립 리뷰 후 한 작업 배치에서 0/1/30행·DB 실패·부분 파싱·범위 초과·enqueue 실패, field/소유권·늦은 응답·timeout·전환 경계, 정확 wire 길이와 기존 Search 회귀를 확인한다. 실제 전용 계정2개의 로그인→내 매물→새로고침→검색 복귀→닫기 및 정상 퇴장을 묶어 화면/서버 결과를 대조한다. 서로 다른 소유 목록은 실제 기존 행이 있을 때만 확인하며, 둘 다0행이면 인증 소유권은 fixture 근거와 실제 빈 결과를 분리한다. 버튼/스크롤이 이동·스킬에 전달되지 않는지 확인한다.

DB 실검사는 SELECT만 사용한다. 정상 전용 계정 로그인·퇴장 저장 외 등록/구매/취소/수령/Outbox, 직접 UPDATE/INSERT/DELETE/seed/스키마·프로시저 적용은 제외한다. `TEST_BAZAAR`/`CRASH_POINT` 통합 시나리오는 기존 행 삭제·재화 변경·abort를 포함하므로 실행하지 않는다. 향후 쓰기 테스트는 기존 실제 자산/계정/가격·수량을 확인한 별도 작업 카드에서 경계를 확정하며 자동 seed를 대안으로 쓰지 않는다.

새 wire·인증 바인딩·Cache 오류 경계는 중요한 변경이므로 후보 구현/리뷰/배치 증거를 준비한 뒤 Coordinator가 PR 직전 한 번만 Slack 승인을 요청한다. 이 문서는 승인 요청 자체가 아니다. 서비스 정상 재시작은 기존 일상 권한에 따르되 새 후보/기존 C09 서버·DLL 쌍을 보존한다. 실패하면 정상 종료와 로그/WAL 보존 후 기존 쌍으로 복구한다. 읽기 기능 rollback을 위해 DB 행을 되돌리거나 삭제할 필요는 없다.
