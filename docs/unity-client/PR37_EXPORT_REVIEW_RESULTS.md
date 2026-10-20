# PR37 exporter 리뷰 수정 결과

대상은 PR37 실제 head eacb744의 세 리뷰다. 일반 개발은 사용자 '여기까지' 중단 상태를 유지하고 이 요청 범위만 수정했다. DATA01 전체catalog확장 ea01991은 별도 로컬 브랜치에 보존했으며 게시 승인으로 재사용하지 않는다.

수정:
- respawnTicks==0 거부. 실제 서버의 dead branch가 unsigned --respawnTick을 사용하므로 0은 wrap된다.
- C++와 같은 float(maxHp)*threshold→uint 절삭값을 검사한다. non-final phase의 절삭값은2이상이어야 살아있는HP(strict<이며HP0는먼저continue)에서 전이할 수 있다. 최종 phase는 전이 gate가 아니므로 양수 float가0으로절삭돼도허용한다.
- 두 결과를 임시 파일에 완전히 기록하고 원본 백업을 확보한 뒤 교체한다. 교체 실패 시 두 원본을 독립적으로 복구하고 최초 파일이 없었다면 새 파일도 제거한다. 읽기 전용은 원본 변경 전에 거절한다. 복구 실패는 원래 예외와 묶어 알리고 백업/marker를 남긴다.
- C14Export.pending을 CreateNew로 확보해 재진입을 거부하고, 두 C++ 소비자가 표시가 남았을 때 #error로 혼합 출력 사용을 차단한다. 완전한 원자적 쌍 교체나 전원 차단 자동 복구를 주장하지 않는다. 중단/복구실패 후 marker와 backup을 확인한 수동 복구가 필요하다.

구현 담당의 테스트 file/directory 삭제 혼동과 한쪽 rollback 실패 시 다른 복원을 건너뛰는 경로는 실행 전에 바로잡았다. 독립 최종 정적 리뷰 차단0.

검증:
- Unity ExportPairReview3/3 PASS: 기존 정상 UTF8/결정적 출력, invalid 데이터 출력불변(새respawn/절삭0·1거부/최종절삭0허용포함), 실제OSI/O 정상교체·두번째readonly거부·두번째directory교체실패에서원본복구/최초없던첫파일제거. Captures/pr37-review-tests-final.txt.
- 격리복사본으로 C++ 두 소비자 문법검사 정상exit0, pending marker 존재시 예상 C14 오류로 컴파일 거절 PASS. Captures/pr37-guard-normal.txt/pr37-guard-blocked.txt. 실제 export directory에 marker를 만들지 않았다.
- Unity가꺼져있어기존프로젝트Editor만열었으며Play입력/게임서버재시작/DB쓰기0. 기존root C14runtime는미게시catalogsuperset그대로유지해기존asset값이유실되지않게했고 PR37 exporter/helper와세테스트만임시통합했다. 테스트는모두임시폴더/독립ScriptableObject사용,실제asset Save/export메뉴호출0. 검증후catalog버전exporter도같은fix로복원할예정.

서버런타임행동·실제스폰수치·프로토콜·DB·ClientCore 변경없음. CoreLib수정은불완전export빌드차단전처리guard뿐이다. 공개 기능·화면 변화가없어사이트본문/캡처갱신·게임입력회귀는하지않았다. 기존시험을통과수늘리기위해반복하지않았다.

게시: 이 고정리뷰묶음만로컬커밋뒤지정UnityCoordinator Slack에서PR37FIX01 게시승인을요청한다. 자동push/PR갱신/병합/Pages게시없음. 기존WORLD01/DATA01승인을재소비하지않는다. 원본Game/rootWIP보존.