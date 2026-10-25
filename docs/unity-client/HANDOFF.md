# 현재 인계 — 2026-10-25

## 목표·정본
주변 플레이어 체력·피격·쓰러짐/회복·원격스킬을 하나의 PR로 준비 완료. Git 정본 C:/Users/joeun/Desktop/workspace/Game-UnityClient/UnityClient/Builds/C10-Delivery/source, branch codex/nearby-player-feedback. ROOT C:/Users/joeun/Desktop/workspace/Game-UnityClient, Editor ROOT/UnityClient6000.0.47f1/Pipeline7800. ROOT/CAND C04/C19/테스트3파일 동기화.

## 기준과 게시
PR54는 사용자 병합 지시 후 GitHub merged=true·origin/master e06d45f 확인, 정상 merge로 현재 브랜치에 통합. 기존 로컬 변경 보존. PR55는 아직 생성하지 않음. 이전 HUDWALLET01 채팅 게시 승인과 HUNTFEEDBACK01 Slack1792920766.249229는 소비완료이며 이번 새 PR 승인으로 쓰지 않는다.

## 이번 묶음
서버HP로 주변 플레이어 게이지·피격 강조, HP0는 객체를 유지하며 쓰러짐/공격억제, 양수HP 회복시 자세복원. 실제 C14/ActionResult 위치·방향·phase로 원/직사각형 차징·짧은 발동 영역. own/unknown/dead/잘못된데이터 제외, 만료/AOI/구역/종료 정리. 기존 몬스터 동작 유지. 서버·사용자AI·DB·패킷·DLL·에셋 변경 없음. 이름 표시는 공개API가 없어 이번범위 제외.
모든 구현 완료 후 통합 독립리뷰 차단없음. 컴파일0/0. 신규2통합검사 최종통과: 원격영역 .025s, 쓰러짐 .946s. 쓰러짐 첫실패는 테스트의 잘못된 MotionVisual 참조를 실제 GuardVisual로 교정한 것, 조건 완화 없음. 실패항목만 재실행, 기존 통과검사 반복없음.
Play 임시장면 기존에셋+예시패킷 렌더1280x720 확인/공개 캡션 명시. 실제 다중접속의 가시성/시점은 미실행. README 대표는 기존 실제사냥 화면 유지. 실제거래/재화차감은 기존 데이터 부족으로 미검증; seed하지 않음.

## 상태·다음 행동
현재 Edit/clean/backgroundfalse. 실제입력 허용 유지. 이번에는 서버 시작하지 않음. WORKFLOW에 전체구현→통합리뷰→묶음검증→자료→게시승인 순서 명시.
전체 후보를 커밋·사이트검사 후 NEARBYCOMBAT01로 정상push+새PR 한 건 게시 승인 요청. 동일 작업 부모1792922124.815219 사용. 승인 전 push없음, 병합/배포 제외. 승인 이후 원격 기준 재확인하고 게시하며 작은 후속별로 PR을 나누지 않는다.

상세 [RESULTS](history/2026-10-24-client-playability/RESULTS.txt), 범위 [BRIEF](BRIEF.md).
