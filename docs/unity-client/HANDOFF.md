# 현재 인계 — 전투·아이템·보상 통합 후보

## 정본·보호 범위
ROOT=C:/Users/joeun/Desktop/workspace/Game-UnityClient. Git 정본 ROOT/UnityClient/Builds/ClientGrowthHUD/source, codex/client-clock-compat. Unity 프로젝트 ROOT/UnityClient. 원본 Game/refactor·사용자 몬스터 AI·DB스키마는 수정하지 않았다. 서버·프로토콜 기능 변경과 아래 정책은 사용자 승인 범위다.

## 구현·검증 완료 범위
- 패링: 기본 공격 제외/방향 무관/방어창0.25초/특수스킬1회/최신쿨3초(60틱). 서버2검사·공유데이터import확인통과.
- HP1번/MP2번·각3000회복·공유2초쿨·만충/사망소비차단. 일반/강화처치HP·MP각1+골드10개별지급·확정후상관wallet갱신. Cache로딩·빈행·실패·flush세대보호를연결했다. 캐시확정은DBflush완료/장애시exactly-once보장이아니다.
- 차징/예고는시전자표시좌표평행이동추종/각도고정,Hit잔광은서버확정phase좌표유지. AI/판정변경없음.
- MainServer/UnitTests/ClientCore/Dummy/export/import/Standalone빌드통과. 직접서버2/Unity4통과,리뷰Handler골드오류경로보완후wallet1만재검증통과,최종검수MP버튼라벨보완후Inventory1만재검증통과. 최종Player-Fixed재빌드통과. Astra코드·자동검사범위필수0/통과,실전미확인범위는제외. 과거통과검사반복없음.

## 입력·실행 대기
사용자가2026-11-05실입력을재개했고이전세션WASD/Space/QER완료·패링표현을확인했다. 최신아이템·골드·추종실전검증을위해새묶음Player24872/MainServer4536/Login35140기동·서버준비확인후Windows보안창에막혔다. 사용자에게mainserver.exe허용창직접판단/처리를요청한상태다. 보안앱클릭·설정변경·우회입력금지. 최신MP실사용·처치골드·이동중표현·조작감은아직미확인이다.

BAT다음기동은ItemsCaster/Player-Fixed/MMORPG.exe + ItemsCaster/Server/MainServer.exe,Login은기존Parry/Login앱/보호환경이다. 현재보안창뒤Player는라벨수정전첫빌드라사용자처리후정상종료/재시작해최신Player로검증한다. 허용전보안창을피하려고다른서버로갈아타지않는다. 새패킷서버·ClientCore·Unity·Dummy동시적용필수.

## 게시·다음 행동
코드후보baed635로로컬커밋완료. PLAYITEM01/r1을새UnityCoordinator앱부모1793852712.014869(요청도같은ts)에발신했고threads.txt자동등록확인. 이코드후보및기존미게시관련묶음의push/PR1개게시만승인대기,회신·기소비없음. 실제게시전정확한후보·사람·채널·스레드·현재대기/회신ID를대조한다. 기존PARRY01/r2(1793801063.563599)는새PLAYITEM01/r1로대체/소비금지. 새승인전push/PR금지,병합은별도다. 실제입력대기는게시승인과별개다.

발신ROOT/UnityClient/Tools/Send-CoordinatorSlack.ps1,수신기복구nodePID31992. 새요청전프로세스/최근health/listening·고정Coordinator·등록스레드재확인. Agent625(신규235/후속390),토큰/비용N/A. 상세 history/2026-11-05-items-caster-follow/RESULTS.md,증거ROOT/UnityClient/Builds/ClientGrowthHUD/verification/items-caster-follow. 구역난이도·장비·수령함은후속범위다.