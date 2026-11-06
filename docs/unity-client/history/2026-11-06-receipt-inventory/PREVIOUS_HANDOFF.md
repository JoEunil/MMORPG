# 현재 인계 — 다음 큰 작업 묶음 범위 제안

## 범위와 정본
사용자는 몬스터 처치 HP·MP 포션 지급만 제거하고 별도 후속 계획을 추가하지 말라고 요청했다. 기존 EXP·구역별 골드·보유 포션 효과/조회/사용은 유지한다. ROOT=C:/Users/joeun/Desktop/workspace/Game-UnityClient, Git정본 ROOT/UnityClient/Builds/ClientGrowthHUD/source. PR68은 원격9f12768로 병합돼 codex/remove-kill-potions를 origin/master에서 새 분기했다. 기존 추천안의 포션지급 부분은 현재 결정이 대체한다. 사용자 AI·DB스키마·패킷·새기능계획 변경 없음.

## 변경과 검증
StateManager의 처치 Item Grant loop만 제거하고 ItemData의 미사용 보상상수3개를 지웠다. 기존 보상fixture는 골드 메시지 금액/수신자를 유지하며 inventory.granted.empty를 검증한다. 코드리뷰 차단0. MainServer/UnitTests Release 빌드 성공, 해당fixture1개 Passed. 기존 Unity/서버검사는 반복하지 않았다. 실제 새 처치 입력은 실행하지 않았으며 자동검사 범위와 구분한다.

현재 GROWTH_ITEMS_PLAN 계약과 공개 영상/가방 캡션에서 자동지급 설명만 최신화했다. 과거 검증기록은 보존하고 HUNT_BALANCE_PLAN/후속 계획은 변경하지 않았다.

## 실행과 다음 행동
게임·서버·로그인은 stopped 상태 확인. ROOT/UnityClient/Builds/LocalGame/HuntingBalance/Server/MainServer.exe를 이번 산출물로 교체해 다음BAT실행부터 적용된다. 기존 Player·포션소비기능은 그대로다.

최신 사용자 지시로 처치 포션 제거만 별도 PR로 게시하지 않는다. 로컬 구현93a2e93과 기록a640a98은 보존하고 다음 관련 큰 묶음에 포함한다. HUNTREWARD01/r1 별도 게시 요청은 철회한다. 과거/늦은 승인을 적용해 게시하지 않는다. 현재는 다음 작업 계획 제안이며 새 기능 구현 승인은 아니다.

근거 ROOT/UnityClient/Builds/ClientGrowthHUD/verification/hunting-balance/no-potion-reward-test.xml/no-potion-build.log. 상세 history/2026-11-05-hunting-balance/RESULTS.md. 서버산출물SHA256 4B85CF8F01D5340C720661991641DDB0D985D65C329B405F2C3F0151B11FB6FA.

구현후보93a2e93 로컬커밋 완료. HUNTREWARD01/r1 요청1793886156.106779를 Unity Coordinator 앱으로 기존 부모1793873237.268829에 발신했다. 수신기health/프로세스/등록스레드 확인, 사용자 지시로 별도 PR 요청 철회·미소비. push/새PR게시 없음. 최종검수통과·Agent656(신규243/후속413).
