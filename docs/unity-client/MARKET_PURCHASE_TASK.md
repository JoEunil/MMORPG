# 거래소 구매·구매품 수령 연결

2026-10-20 사용자 계속 진행 지시에 따라 착수. codex/market-purchase는 검증된 수령 후보20b365e에서 분기했다. MARKET01 게시 승인 원문은 아직 없으며 그 정확 후보와 승인 범위를 보존한다. 다음 구매·배송 변경은 별도 로컬 후보이며 이전 승인에 포함하지 않는다. 원본Game/root WIP와 pending content848f8b도 보존한다.

목표: 판매 검색의 매물 선택→가격을 보여주는 구매 확인→서버 구매 결과→기존 outbox 수령 요청→가방 재조회 흐름을 연결한다. 기존 sp_bazaar_buy transaction/중복 차감 방지와 DeliverItem/CacheFlush의 배송 dedup·영속화를 재사용한다. 등록·취소, 새 경제 규칙·DB 스키마/프로시저, 재화 합성 표시, 실시간 잔액 조회는 제외한다.

계약은 MARKET_PURCHASE_CONTRACT.md를 따른다. 외부32/33구매(20/37bytes),34/35배송(12/25bytes). requestID·listingID·세션bound캐릭터·ViewModel 수명과 응답을 대조하고 금액u64/수량u32를 절삭하지 않는다. 구매 COMMIT은 배송 완료가 아니며 delivered/duplicate 응답도 영속화 ACK로 표시하지 않는다. 불명 결과는 자동 재구매하지 않는다. 재배송 요청은 기존 event dedup 경계를 유지한다.

파일 소유권: 서버 분석은 계약 문서만, 구현자는 CoreLib/CacheLib/ClientCore 및 직접 영향 자동 테스트와 필수 fake stub, Coordinator는 Unity 거래소 화면/Editor setup/Scene·실행 환경·실입력·Git, 리뷰는 고정 diff를 읽기만 수행한다. 같은 파일 중복 편집과 테스트 중 Assets/Packages/ProjectSettings 쓰기를 금지한다.

검증 목표는 기존 DB 정책을 바꾸지 않은 권한·상관관계·비동기 소유권, 성공 구매와 배송의 구분, 중복 입력/취소·늦은 결과/금액·수량 경계다. 동결·독립 리뷰 후 Unity 직접 영향2개와 Core/Cache 독립2경계를 한 배치로 실행하며 이전 수령·전체 게임 회귀는 반복하지 않는다. 5011 한 로그인에서 검색→구매 확인/재화 부족→빈 구매품 수령→가방 빈 결과→정상 퇴장만 확인한다. 대표 캡처1~2장으로 제한한다.

read-only 관찰: 5011/5012 diamond행 없음, outbox0. 기존 potion 매물57/56/55 seller1017/item1/qty1/price50. 재화를 주입하거나 다른 매물을 만들지 않는다. 실제 구매 성공·차감·배송 영속화는 현재 데이터로 미검증이며 자동 fixture 성공과 구분한다. 데이터 준비가 필요하면 정확 범위 사전 승인 없이는 seed/INSERT/UPDATE하지 않는다. 초기화 모드·기존 통합 테스트 전체 실행 금지.

구현·독립 리뷰·최소 검증·현재 사이트/화면 이력·로컬 커밋까지 완료한 뒤 관련 거래소 흐름을 큰 PR로 전달한다. 최종 게시 직전 Unity Coordinator 앱 Slack 승인과 플랫폼 통제는 유지한다. 일반 완료별 재개 승인은 요구하지 않는다.
