# 현재 인계 — 사냥터·밸런스 PR 승인 대기

## 정본과 승인
ROOT=C:/Users/joeun/Desktop/workspace/Game-UnityClient. Git 정본 ROOT/UnityClient/Builds/ClientGrowthHUD/source, codex/hunting-balance(origin/master fcd12f4, PR67 병합 후 분기). Unity 실제 ROOT/UnityClient. 사용자는 추천안 전체 적용·입력 재개를 승인했다. 새 PR 게시는 아래 요청을 별도 대조한다. 사용자 몬스터 AI·DB 스키마·패킷은 변경하지 않는다.

## 구현·검증 완료
HUNT_BALANCE_PLAN의 구역별 수치·일반/강화/보스 총수0/120/132/144/156/168, 스킬 정의 분리, Q/E/R 피해300/1200/1800(공격10 기준)·MP0/100/160, HP1500·MP500·공용6초 반영. 성장·AI/state/배회/추적/phase순서·범위/쿨/타격 시점 유지. 서버JSONv3→실제Unityasset import·씬 연결과 Unity스폰→서버 행렬 재export동일해시 확인.

코드 리뷰 보완 후 차단0. 서버4/Unity3 직접 영향 검사와 MSVC 서버/UnitTests·최종Windows Player 빌드 통과. 실전 E/R 비용·MP9480→9980/포션소비·공용6초·사망 후 마을귀환 확인. 최초 한글깨짐은 원담당폰트보완 후 최신 실행본에서 정상 렌더 확인했다. 재접속 MP9980·HP포션22/MP2·Gold100020 유지. 종합 게시검수 통과·필수0. 이미 통과한 검사는 반복하지 않는다.

## 한계·후속
전체 구역 실제 처치 시간·보스 체감·난이도 균형은 미평가다. 구역스탯/배치/보상과 HP1500은 자동 검사 근거다. 귀환 직후 포션수량 '?'의 첫 클릭 조회·둘째 클릭 소비는 기존 warmup 수명 문제로 추적하고 자동 소비 재시도는 추가하지 않았다. 다음 별도 기능은 사용자 선택 전 진행하지 않는다.

## 실행·게시 상태
BAT는 ROOT/UnityClient/Builds/LocalGame/HuntingBalance/Player/MMORPG.exe와 HuntingBalance/Server/MainServer.exe 사용. 이전 ItemsCaster 복구본 보존. 최종세션 c56a38d82f2c42c7a445060ba77ef95d 게임/서버/로그인 정상종료. 입력 배치 종료, 별도 PC조작 없음.

로컬 변경을 한 커밋으로 모으고 HUNTBAL01/r1 PR게시 승인 요청을 Unity Coordinator 앱으로 기존 부모1793873237.268829에 남긴다. 요청 전 push/새PR게시 없음. PR67의 승인을 재사용하지 않는다. 실제 수신자·스레드·현재대기·회신ID·미소비를 대조한 뒤 처리한다.

Agent654(신규243/후속411), 토큰·비용N/A. 상세 history/2026-11-05-hunting-balance/RESULTS.md, 근거 ROOT/UnityClient/Builds/ClientGrowthHUD/verification/hunting-balance/korean-label-final.jpg 및 같은 폴더 검사/빌드 결과. Slack도구 ROOT/UnityClient/Tools/Send-CoordinatorSlack.ps1.
