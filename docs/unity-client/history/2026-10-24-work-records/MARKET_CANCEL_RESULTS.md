# 판매 취소·아이템 반환 연결

현재 후보는 cb53db6의 구매·수령 흐름에 내 매물 판매 취소를 더했다. 판매중인 본인 매물만 2회 확인 후 취소하며 등록비 환급을 약속하지 않는다. 서버 취소 확정과 반환 아이템의 가방 반영·영속화를 구분한다. 기존 procedure14와 DeliverItem/CacheFlush를 재사용했고 DB 스키마·프로시저·경제 규칙을 수정하지 않았다.

외부36/37 요청20·응답21bytes와 내부34/35를 연결했다. bound field 캐릭터, requestID·listingID·모델수명을 확인한다. DB 결과를 확인하지 못한 경우 status5를 전달하고 자동재취소하지 않는다. 기존 내부 취소24/25는 보존했다. 구현·최종 독립 읽기 리뷰에서 남은 차단 문제는 없다.

리뷰에서 응답 전에 창을 닫거나 타임아웃되면 이전 가방이 보일 수 있는 P2를 찾았다. 불확실한 취소가 남으면 같은 모델의 수동 가방 열기마다 새 조회를 보내며, 오래된 조회 완료가 그 요구를 지우지 못하게 보완했다. 기존 인벤토리 pending/timeout 경계는 유지했다.

Unity 직접2 최종PASS: 가방 신선도는 첫 배치에서 통과했고, 취소 UI 통합시험은 잘못된 flags 예외가 삼켜져 실패했다. 실제제품 Handler를 simulation 분기 전 파싱·기존 소켓 오류경계로 재전파하도록 수정하고 해당 실패1개만 재시험해 통과했다. 테스트 expectation을 완화하지 않았다. 초기 테스트 코드 네임스페이스 참조 컴파일 오류는 정확한 공개 namespace로 수정했다. 근거는 root UnityClient/Captures/market-cancel-tests-final.txt와 market-cancel-ui-rerun-final.txt다.

네이티브 검증은 기존 구매 경계에 취소 권한·상관·응답과 반환 BLOCKED의 의미를 합친 단일 필터다. 최초실행은 기존 구매시험의 loads.accept=false가 취소구간에도 남아 정상요청이 거절되어 실패했다. 미인증 거절 assertion과 응답 기대를 보존하고 정상구간에서 큐허용 전제를 복원했다. 수정 후 같은 단일 필터를 재실행해 1/1 통과했다. 최초실패와 최종결과는 root Captures/market-cancel-native.txt 및 market-cancel-native-corrected.txt에 각각 보존했다.

Release ClientCore/MainServer/UnitTests 빌드가 통과했다. 첫 병렬 빌드는 worker진행이 없어 취소입력으로 중단했고 별도obj-single/bin-single에서 단일프로세스 빌드로 해결했다. 수정시험 증분빌드는 프로젝트 직접호출 시 SolutionDir 포함경로가 없어 실패했고 솔루션 타깃으로 다시 수행해 빌드0으로 완료했다. 이 빌드도구 실패와 제품결함을 구분하며 로그를 보존한다.

실제입력은 기존구매후보서버23696을 유지한 상태의 UI 배치 한 번이다: 5011 native로그인·선택·월드 → 거래소검색30 → 내매물0 응답·CANCEL SALE 표시/비활성 → 대표PNG1 → 물리Close/Disconnect·Modelnull·Playstop. 프레임28382→35912 증가를 확인했다. 취소할 본인 매물이 없으므로 새 취소 TCP/DB 성공·반송·배송영속화는 실서버에서 미실행이며 자동 fixture와 구분한다. 새 취소 MainServer는 빌드만 했고 실행하지 않았다. 준비된 정상전환 helper도 실행하지 않았다. DB seed·다른판매자취소·재화/아이템주입·서버재기동은0이다. 정상logout1168은 기존 비차단 진단이다.

원본Game/root WIP/pending catalog848f8b와 게시대기구매cb53db6를 보존한다. 공개원본·캡처1장·이전이력을 갱신하고 HTML 생성, 실제 브라우저의 설명·이력 링크·이미지8개 로딩과 가로넘침 없음을 확인했다. 기존 반응형 스타일을 바꾸지 않아 이전 모바일 검증을 반복하지 않았다. root Captures/market-cancel-web.png에 새 기능 설명 화면을 남겼다. 로컬커밋·게시승인은 별도 인계에서 확정한다. MARKET02 승인에 취소범위를 포함한 것으로 간주하지 않는다.
