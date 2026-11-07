# 등급 포션 데이터·가방 사용 결과 — 2026-11-07

## 승인·구현
- 사용자 call_aff6463120cd4d1a8d59f70e24fa87ac 추천 정책 승인. 기존 ID1HP1500/2MP500 유지, 신규6중급HP3000/7중급MP1000/8상급HP4500/9상급MP1500. 전부type1·무게5·공유120틱6초.
- 기존 StateManager::UseItem·캐시 소비·WAL·ClientCore requestID/epoch/itemID 상관성을 재사용했다. 서버 제품 diff는 ItemData.h4행뿐이며 패킷/DB/저장형식/AI 변경 없음.
- 가방의 포션 판별·버튼·설명·응답 처리를 서버 공유 효과 기반으로 확장했다. 새 등급은 가방만,1/2키는ID1/2 유지. 아이콘은 기존 HP/MP 재사용, 등급명으로 구분한다.
- 공급/상점/처치지급/드롭/테스트자동지급/DB시딩 없음. 정상 보유 시 사용과 기존 거래 경로를 지원하는 구현이며 신규 획득 플레이 완성으로 보고하지 않는다.

## ID 확인
- 지정 테스트DB 인벤5002행의지원형식prefix 및bazaar/bazaar_log/buyer_outbox 전체item_id읽기:1,2,3,4,33. 미지원BLOB0. 현재MainServer정지 확인, 런처DefensiveGear/Server/redo.8의11레코드prefix도같은ID. 복구로그는읽기만했고재생/삭제하지않았다.
- 실제카탈로그테이블없음. 신규6..9는위확인환경에서미참조라정의했다. 운영·다른사본·과거백업전역검사나WALCRC복구검증으로확대하지않는다. 기존미정의33의의미·수량은바꾸지않는다.

## 직접 영향 검증
- 독립코드리뷰 필수0. 신규테스트구간도달·UI음수/미등록사용금지·hotkey기존유지·응답상관성을확인했다.
- MainServer/UnitTests Release 솔루션 빌드 성공. 최초프로젝트직접빌드는SolutionDir미전달로include실패하여Game.sln의대상2개빌드로명령을수정했다. C4244/C4267·/arch:SSE등기존경고는로그에보존했다.
- 기존 서버 ItemUseCommitsOnceAndRejectsWithoutConsumption1건통과:신규4종정확회복·타자원무변경·재전송중복없음·교차등급공유쿨·만충/사망미소비·최대치제한.
- Unity컴파일오류없음. 기존SelectedItemExplains...1건통과:신규4종설명/기존아이콘재사용·실제ID9가방요청·다른ID응답잠금유지·일치응답·공유쿨차단.
- 기존ServerSnapshotImportsEntireCatalog...1건통과:9품목의효과·공유120틱·전체JSON일치·재import결정성·기존GUID보존. importer/exporter구조는변경하지않아무관한거부검사는반복하지않았다.
- 총신규영향기존3검사통과. 앞선장착/공유숫자2통과검사는변경없어반복하지않았고,선택설명검사만신규등급영향으로다시실행했다.
- 서버exporter JSON5에9종, 실제Unityimport9종·GUID e20a847d2d4fa094088b01f4c513165b유지. 기존Asset/JSON만반영했다.

## 화면·한계
- tier-ui-preview.png는실제World씬의별도PreviewScene·1280×720샘플UI다. 새4종명칭·상급MP1500설명·기존장착배지·1/2키·공유4초표시를확인했다. 샘플수량은실제DB자산/공급/소비의증거가아니다.
- 캡처API5초응답시간초과후파일최신시각·출력그림·Playoff/sceneCount1/dirtyfalse를확인했다. 같은작업을중복실행해성공을만들지않았다.
- 새Player빌드·서버정상전환·실전보유/소비·게임입력은미실행. 런처는기존DefensiveGear산출물그대로이며이번소스가실행중이라고보고하지않는다.
- 근거: ROOT/UnityClient/Builds/ClientGrowthHUD/verification/item-usability의tier-*.json/xml/log/png 및읽기전용ID스캔.
- 기존가방·장비·쿨다운개선과한PR후보로묶는다. 새push/PR은게시승인전까지보류한다.

## 최종 정리
Astra 게시후보 검수통과·필수0. 공개샘플이미지와검수이미지일치,이름/효과/장착배지잘림·겹침없음확인. 사이트생성/링크검사통과. 검증용Editor12784정상종료. 서버/런처교체·새Player빌드·실전등급소비는미실행으로유지한다.
