# 현재 인계 — 2026-10-30

## 현재 목표와 위치
사용자는 서버 refactor 작업을 직접 진행한다. 기존 클라이언트 CAND 워크트리에서 서버 변경 없는 개선만 진행했다. 몬스터 AI·A*·서버 이동 속도·패킷·DB·서버 전환은 이번 범위가 아니다.

- ROOT: C:/Users/joeun/Desktop/workspace/Game-UnityClient
- Git 정본 CAND: ROOT/UnityClient/Builds/C10-Delivery/source
- 현재 브랜치: codex/client-motion-smoothing, 기준 master37e92f4(사용자가 수신 틱 제거를 커밋)
- 실제 Editor 프로젝트: ROOT/UnityClient. 변경한 클라이언트 소스·DLL·씬만 동기화했다. 원래 서버 Workspace Game과 master는 수정하지 않았다.
- 별도 ClientReceiveMain은 detached 보존 사본이다. 기존 BAT commit ace9e7e는 codex/local-game-launcher에 보존했고, 미커밋 계획은 명명된 stash와 ROOT/UnityClient/Builds/ClientMotion/PlanningBackup에 보관했다. 현재 PR 범위에 BAT/NetLibrary 변경을 섞지 않았다.

## 완료와 검증
원격 보간을75ms→150ms/8표본으로 조정하고 메인 스레드 큐 대기 시간을 보정했다. 서버 시각이 없는 패킷의 네트워크 묶음 도착까지 복원하는 것은 아니다. 게임 직교 카메라7.2→9.36으로 가시 폭·높이1.3배를 적용했다. 로그인 원근 카메라는 유지했다.

HP 포션 바로가기1과 수량/사용 가능 표시, 비HP 가방 사용 버튼 숨김, 판매/사용/보상 후 재고 갱신을 기존 가방 요청·쿨다운·불명확 결과 잠금에 통합했다. 미조회 수량은?이며 첫 입력은 조회만 한다. MP 포션 사용은 서버 미지원이므로 노출하지 않는다.

ClientCore Release 빌드, 새 Unity PlayMode fixture3개 통과. 실제 실행 개수2+1을 XML로 확인했다. 초기 권한 제한 라이선스 실패와 잘못된 EditMode0개 선택은 성공으로 집계하지 않았다. 포션 슬롯3개 참조와 카메라를 Editor에서 확인했고 씬을 저장했다. 별도 미연결 Editor 예시를 촬영했으며 실제 게임/계정/입력을 검증한 화면이 아니다. 독립 코드 리뷰2건 수정 후 새 포션 검사에 포함했다. Astra 고정 후보 게시 준비 검수 통과/필수0이다.

근거: [결과](history/2026-10-30-client-motion-smoothing/RESULTS.json), [제품 해시](history/2026-10-30-client-motion-smoothing/product-manifest.json), [검수](history/2026-10-30-client-motion-smoothing/quality-review.md), [Editor 예시](history/2026-10-30-client-motion-smoothing/editor-preview.png).

## 현재 제한과 다음 행동
실제 이동 체감·실서버 지터·키1/클릭 입력·확대 시야의 AOI는 미확인이다. 사용자 서버 리팩토링과 계약을 대조한 뒤 실제 입력을 한 배치로 확인한다. 테스트를 위해 서버를 교체하거나 이미 통과한 검사를 반복하지 않는다. 독립 실행 게임은 재빌드/교체하지 않았다.

게시 전 로컬 후보를 고정하고 Unity Coordinator 앱의 새 작업 스레드에서 PR 게시 승인만 요청한다. 아직 push/새 PR은 하지 않았다. 승인 대기는 요청 ID·정확한 후보·스레드·지정 사람·회신 ID·기소비를 확인한 뒤 처리한다. 수신확인은 게시 승인이 아니다.

나머지 방향/스킬·MP·골드·레벨 구역·배송/장비 범위는 [후속안](PLAY_FEEDBACK_PLAN.md)을 따른다. 사용자 AI를 변경하거나 서버 경제/계약 작업을 자동 재개하지 않는다.

## 과거 기록
PR60은 병합 완료다. [이전 인계](history/2026-10-30-client-motion-smoothing/previous-handoff.md)는 당시 기록이며 현재 명령이 아니다. 이전 승인 RECOVERY01은 이미 소비됐고 이번 후보에 재사용하지 않는다.