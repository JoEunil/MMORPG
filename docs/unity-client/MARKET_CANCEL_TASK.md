# 내 매물 판매 취소와 반환 연결

2026-10-20 사용자 계속진행에 따라 codex/market-cancel을 cb53db64ff605278c41a0bdeeedfcce4b707d773에서 분기했다. 구매·대금수령 후보의 MARKET02 게시 승인은 아직 없으며 그 정확 후보는 보존한다. 취소를 기존 승인에 포함하지 않는다. 상세 계약과 근거는 NEXT_MARKET_CANCEL_TASK.md를 따른다.

완료 흐름: 내 판매중 매물 선택 → 수수료 환급 없음과 반환 별도 확인 → 명시적 판매 취소 → 서버 결과 → 기존 배송 버튼으로 반환 요청 → 가방 확인. 기존 sp_bazaar_cancel/DeliverItem/CacheFlush만 재사용하며 DB 구조·프로시저·경제 규칙·매물 등록은 변경하지 않는다. 취소 성공을 반환 영속화 완료로 표시하지 않는다.

외부36/37은 requestID:u32+listingID:u64 요청20/응답21bytes(+status:u8). 상태0확정거절,1취소확정,5결과미확인. 세션의 bound field character와 응답 수명·상관관계를 확인한다. 결과불명/타임아웃/닫기 뒤 자동재취소·가짜복구를 하지 않는다. 같은 모델의 성공·불명 매물 재요청은 막는다. MyList 새로고침은 취소매물을 숨길 수 있지만 반환 완료 근거는 아니다.

소유권: 구현 담당은 Core/Cache/ClientCore/native tests/fake compile stubs, Coordinator는 Unity UI/setup/Scene·UI통합시험·환경·Git·문서. 독립 리뷰는 읽기만 한다. Editor 조작자는 Coordinator 한 명이며 기능시험 중 Assets/Packages/ProjectSettings 변경을 동결한다. 원본 Game/root WIP/pending catalog는 보존한다.

검증은 동결·독립리뷰 뒤 native1(권한/wire/예외·취소와배송의의미) + Unity1(UI확인/중복·결과불명/수명)을 실행한다. 기존 배송·구매 회귀는 반복하지 않는다. 전용 계정 본인 매물0이면 실제 취소 성공은 미검증으로 남기고, 한 로그인에서 내 매물 빈 결과와 취소 비활성/반환 안내만 확인한다. 다른 사람 매물 취소, DB seed·재화/아이템 주입은 하지 않는다. 대표 캡처1장과 영향 웹 페이지만 확인한다. 실패 시 해당 항목만 수정·재시험하고 필요한 직접 영향 검증이 통과하면 종료한다.

검토·검증·로컬커밋·사이트 결과를 큰 거래소 PR에 묶어 게시 직전 Unity Coordinator 앱 Slack에 구체적 후보 승인을 요청한다. 승인대기 중 독립 로컬 작업을 계속하며 완료별 재개 승인은 요구하지 않는다.
