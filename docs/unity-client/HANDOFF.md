# 현재 인계 — 전투·아이템·보상 통합 후보

## 정본·보호 범위
ROOT=C:/Users/joeun/Desktop/workspace/Game-UnityClient. Git 정본 ROOT/UnityClient/Builds/ClientGrowthHUD/source, codex/client-clock-compat. Unity 프로젝트 ROOT/UnityClient. 원본 Game/refactor·사용자 몬스터 AI·DB스키마는 수정하지 않았다. 서버·프로토콜 기능 변경과 아래 정책은 사용자 승인 범위다.

## 구현·검증 완료 범위
- 패링: 기본 공격 제외/방향 무관/방어창0.25초/특수스킬1회/최신쿨3초(60틱). 서버2검사·공유데이터import확인통과.
- HP1번/MP2번·각3000회복·공유2초쿨·만충/사망소비차단. 일반/강화처치HP·MP각1+골드10개별지급·확정후상관wallet갱신. Cache로딩·빈행·실패·flush세대보호를연결했다. 캐시확정은DBflush완료/장애시exactly-once보장이아니다.
- 차징/예고는시전자표시좌표평행이동추종/각도고정,Hit잔광은서버확정phase좌표유지. AI/판정변경없음.
- MainServer/UnitTests/ClientCore/Dummy/export/import/Standalone빌드통과. 직접서버2/Unity4통과,리뷰Handler골드오류경로보완후wallet1만재검증통과,최종검수MP버튼라벨보완후Inventory1만재검증통과. 최종Player-Fixed재빌드통과. Astra코드·자동검사범위필수0/통과,실전미확인범위는제외. 과거통과검사반복없음.

## 입력·실행 중
사용자가Windows보안창처리완료를확인했다. 기존세션을정상종료하고최종ItemsCaster/Player-Fixed로다시시작했다. 현재Player25520/MainServer12256/Login34200,launcher세션ed93633f664f45eca4a519493c1fbc73. 지정test1로그인·캐릭터11/레벨4/Zone5진입,HP/MP슬롯·MP0개표시확인. 기준골드99980/HP포션18/MP포션0/EXP320/HP9870/11500/MP8638/10000,위치(369.582,287.122).

사용자에게일반몬스터1마리처치및R차징중이동추종을한번에직접확인요청했고답변대기다. 자동화키유지입력한계때문이며사용자조작중자동입력·게임재시작금지. 사용후수량/회복과보상확인/root후속은답변후진행한다. 현재포션은사용하지않아처치전후비교기준을보존했다. 최신기능전체실전통과로보고하지않는다.

BAT경로ItemsCaster/Player-Fixed/MMORPG.exe + ItemsCaster/Server/MainServer.exe,기존Parry/Login앱/보호환경. 최신수정빌드실행확인. 보안앱클릭/설정변경없음,원본Game/AI/DB수동변경없음. 근거verification/items-caster-follow/live-before.png와최신로그. 새패킷서버·ClientCore·Unity·Dummy동시적용필수.
## 게시·다음 행동
코드후보baed635로로컬커밋완료. PLAYITEM01/r1을새UnityCoordinator앱부모1793852712.014869(요청도같은ts)에발신했고threads.txt자동등록확인. 이코드후보및기존미게시관련묶음의push/PR1개게시만승인대기,회신·기소비없음. 실제게시전정확한후보·사람·채널·스레드·현재대기/회신ID를대조한다. 기존PARRY01/r2(1793801063.563599)는새PLAYITEM01/r1로대체/소비금지. 새승인전push/PR금지,병합은별도다. 실제입력대기는게시승인과별개다.

발신ROOT/UnityClient/Tools/Send-CoordinatorSlack.ps1,수신기복구nodePID31992. 새요청전프로세스/최근health/listening·고정Coordinator·등록스레드재확인. Agent625(신규235/후속390),토큰/비용N/A. 상세 history/2026-11-05-items-caster-follow/RESULTS.md,증거ROOT/UnityClient/Builds/ClientGrowthHUD/verification/items-caster-follow. 구역난이도·장비·수령함은후속범위다.