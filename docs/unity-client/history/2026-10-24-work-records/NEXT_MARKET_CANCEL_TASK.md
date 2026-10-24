# 다음 준비 — 내 판매 취소와 기존 반송 확인

2026-10-20. 구매·배송 검증 후보의 다음 독립 준비다. 판매 취소 제품 구현·DB쓰기·시험·Editor·Git 게시·Slack 발신은 수행하지 않았다. 현재 후보의 기존 서버 소스만 읽었고 원본 Game/root WIP는 변경하지 않았다. 수령·구매의 기존 게시 승인을 취소 연결에 재사용하지 않는다.

## 판단과 최소 사용자 흐름

**내 매물의 판매중 행 선택 → 취소 확인 → 서버 취소 결과 → 기존 배송 버튼으로 반송 확인 → 가방 확인**을 다음 후보로 권장한다. 등록보다 작고 완결된 흐름이다. 등록은 inventory/gold cache 보상과 DB INSERT가 분리되지만, 취소는 기존 procedure가 판매중단과 반송event를 한 transaction으로 보존하고 이번 구매 묶음의 배송·가방 갱신을 재사용할 수 있다. 새 거래 기능·규칙·수수료·DB 스키마를 만들 필요가 없다.

## 실제 소스 계약

- `Resources/DB/procedure.sql::sp_bazaar_cancel`은 listingID+sellerID 행을 FOR UPDATE로 잠근다. TRADING이면 CANCELLED 변경과 seller의 buyer_outbox READY 반송event 생성(price0)을 한 transaction으로 COMMIT한다.
- 이미 CANCELLED면 기존 seller/listing의 event를 재사용한다. 새 반송을 만들지 않으며 READY 여부만 다시 읽는다. SOLD/다른소유자/없는매물/반송event없는행은 result0, 정상 취소 또는 기존반송 재사용은 result1이다. SQL예외는 ROLLBACK 후0이다. 구매도 같은 매물행을 잠그므로 정상 BUY/CANCEL 경쟁은 한쪽만 TRADING 전환에 성공한다.
- 등록비gold 환급은 procedure·handler 어디에도 없다. 취소 UI에서 수수료 반환/다이아 반환을 약속하지 않는다. `price0`은 반송event의 기록이며 등록비 환급이 아니다.
- `CacheLib/Handler_Bazaar.cpp::BazaarCancel`은 query14 후 delivery_pending이면 `DeliverItem(seller,eventID,itemID,quantity)`를 시도한다. 배송이 BLOCKED/읽기중이어도 취소성공1을 반환하고 READY에 남겨 후속 기존배송으로 수렴시킨다. cache에서 아이템을 직접 보상/추가하는 별도 경로는 없다.
- `DeliverItem`의 item+eventID 동일blob/dedup ring과 `CacheFlush`의 blob영속화 → outbox CLAIMED 순서를 그대로 사용한다. 취소성공은 반송의 가방반영 또는 영속완료 ACK가 아니다. 수량u32의u16가방수용경계·가방/ring포화는 기존 BLOCKED로 구분한다.
- 내부MSG_CANCEL24/25와 request(characterID,listingID)/response(status)만 있다. 현재 외부TCP는35까지며 NonZone/MessageQueueHandler·ClientCore에는 취소 연결이 없다. 기존 `CancelBazaar()`는 클라이언트 pending표시 취소이며 판매취소 기능이 아니다.

## 최소 연결 후보와 안전 경계

외부36/37은 현재 빈 번호 후보이나 구현 착수 때 다시 확인한다. 요청 requestID:u32+listingID:u64(헤더포함20), 응답 requestID:u32+listingID:u64+status:u8(21)만으로 기존 배송 기능과 연결 가능하다. 내부 응답에는 requestID·listingID·characterID가 필요하다. 외부 길이/flags/0-ID를 검증하고 판매자는 오직 `GetFieldCharacterID(session)`로 정한다. 응답도 같은 bound character·요청종류·requestID·listingID·현재 model lifetime을 확인한다.

외부 상태는0확정거절/SQLrollback,1판매취소확정,5결과미확인 후보다. 0/1은 유효한 실제 procedure 결과만, worker/queue/COMMIT후파싱예외는5로 보수적으로 구분한다. 기존 handler는 body포인터 capture·일반DBWorker 예외시 Return만 하는 경로여서 그대로 TCP에 노출하지 않는다. 값복사/초기화된 상관응답/예외·queue실패/원본Message 정확히1회Return을 기존수령·구매 방식으로 보완해야 한다. 취소는 DB0/1이 모두 item배송완료 정보를 주지 않으므로 성공1 뒤에는 기존배송/가방확인을 안내한다.

상품반환 시도 후 예외가 나도 이미 취소 COMMIT/일부cache반영됐을 수 있다. 자동재취소·자동재화복원·client가짜가방가산을 하지 않는다. 결과미확인은 매물재조회와 기존배송·가방확인으로 안내한다. 같은 UI세션에서 해당행의 추가취소를 잠근다. 명시적 재시도에서도 기존procedure의 event재사용을 유지한다. MyList query11은 CANCELLED를 제외하므로 재조회 후 사라짐은 정상이며, 화면에서 사라졌다는 사실만으로 반송완료를 주장하지 않는다.

## 데이터와 최소 검증 계획

현재 전용계정 내매물0행이라는 확인 근거에서는 실제 취소할 본인 TRADING행이 없다. 실제 성공을 관찰한 것처럼 기록하지 않는다. 정상 시나리오는 fixture/mock과 실제 DB 취소성공을 분리하고, 실입력에서는 빈내매물·미지원동작노출없음을 확인할 수 있다. 기존 정상게임경로로 본인매물이 생기면 그때 같은계정의 취소를 검증한다. 임의 매물/item/통화 seed·다른사람 매물취소·정책우회는 하지 않는다.

자동검증은 최대 두 경계로 묶는다: **(1)** 실제 요청의 bound소유자/상관·queue/예외와 취소중복의 event재사용·구매경쟁 결과, **(2)** 확인클릭/중복차단·늦은응답·성공과반송보류 구분. 기존배송 dedup 시험을 새로 복제하지 않고 기존근거를 재사용한다. 실제입력은 데이터가 있을 때 한로그인에서 내매물취소→기존배송→가방확인 한흐름, 없으면 성공미실행을 명시한다. 테스트는 계획만 작성했으며 실행하지 않았다.

착수조건은 기존 procedure·outbox를 유지한 값복사/소유권·불명결과 보완 후보가 리뷰 가능한 상태인 것이다. 추가 DB정책이 필요해지는 경우에는 구현을 확대하지 않고 정확한 차이를 Coordinator에게 보고한다. 현재 소스에서는 schema나새경제규칙 없이 진행 가능한 후보로 판단한다.
