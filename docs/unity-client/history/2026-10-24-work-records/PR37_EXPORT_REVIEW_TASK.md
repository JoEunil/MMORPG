# PR37 exporter 리뷰 수정

최신 사용자 요청은 세 코드리뷰 수정이며, 직전 '여기까지'로 중단한 일반 개발·게임 입력을 재개하지 않는다. PR37 실제 head eacb744에서 codex/pr37-review-fixes로 분기했고 DATA01 미게시 ea01991은 codex/content-catalog에 보존했다. 새 게시·PR 갱신은 검증/커밋 뒤 Unity Coordinator Slack 승인 대상이다.

범위: C14 exporter의 respawnTicks0 거부; 서버 uint32 HP threshold의 float 곱/절삭 결과를 검사해 non-final phase가 살아 있는 HP에서 전이 가능한 threshold>=2 요구(최종 phase threshold는 전이 gate가아니므로0절삭 허용); 두 산출물 stage·backup·교체실패 rollback·중단marker/buildguard. 서버 행동/DB/프로토콜/게임수치 변경은 하지 않는다. 미게시 전체catalog variant에도 같은 수정을 이식하되 PR게시범위를 섞지 않는다.

검증: 구현·독립리뷰후 기존 exporter 정상/invalid 통합2개+실제OSI/O 실패1개 총3시나리오만. readonly두번째출력에서원본불변,두번째경로directory로첫교체뒤실패를발생시켜원본복구·최초생성시원래없던상태복구를확인한다. 복구실패는marker/backup보존·명시실패, 중단marker가남으면재export와C++빌드가혼합사용을차단. 파일쌍의완전원자성/전원차단자동복구를주장하지않는다. 일반Play입력/사냥/전체회귀/서버재시작/DB쓰기는제외한다.

Unity root는미게시catalog확장asset을보존해야하므로runtimeC14타입/asset을과거형으로내리지않고 PR37 exporter와공통helper·3테스트만일시통합한다. 기존ring/monster0 필드와기본값은동일하며테스트는모두isolated temp directory/ScriptableObject를사용한다. 실제assetSave/export메뉴를호출하지않는다. 검증뒤공통fix를catalog에도이식해root통합을복원하며추가테스트는새로영향받는검증경로만선택한다. 원본Game/root서버WIP보존.