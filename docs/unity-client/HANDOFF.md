# 현재 인계 — PR73 게시·최신 실행 후보 준비 완료

## 정본·후보
- ROOT: C:/Users/joeun/Desktop/workspace/Game-UnityClient. Git 정본: ROOT/UnityClient/Builds/ClientGrowthHUD/source, codex/client-item-usability. PR73 open: https://github.com/JoEunil/MMORPG/pull/73 . 코드 후보d98141f, master614c61a(PR71/72 병합) 기반. 병합하지 않았다.
- POTIONTIER01/r2는 현 채팅의 PR 개시 명령으로1회소비·게시완료. Slack수신승인으로꾸미거나다시사용하지않는다.
- 기존편집ROOT/UnityClient는보존. 최신Player는고정정본1859파일을별도로복제한ROOT/UnityClient/Builds/ItemUsabilityPackage/Project에서빌드했다. 현재Editor/Play모두종료,입력배치없음.

## 완료·한계
- 가방장착배지·개별효과·슬롯상태·공유쿨초,등급HP1500/3000/4500·MP500/1000/1500·6초공유,새등급가방만/1·2키기본유지,거래소선택효과연결완료. 상점/공급/드롭/DB시딩/AI/새프로토콜변경없음.
- 기존직접영향검사/독립코드리뷰/화면검수통과. 이번추가수명감사확정결함0,억지수정없음. 이미통과한게임검사는반복하지않았다.
- 새Windows64Player build_f463960a42d8 성공(오류0·기존경고5),Windowed1280×720설정·아이템9종·ClientCore원본해시일치·테스트어셈블리제외·씬누락스크립트0확인.
- 최신서버+설치MySQLOpenSSL쌍을비공개후보에보존,정적의존성/심볼756·x64·해시확인. 원본DefensiveGear의현재WAL2개복사/해시확인·원본보존. 실제기동/DB·TLS/실제등급소비/조작감미검증.
- 후보: ROOT/UnityClient/Builds/LocalGame/TierPotions. 입력/빌드/패키지/복구manifest: ROOT/UnityClient/Builds/ItemUsabilityPackage. 상세PACKAGING_PLAN.md·PACKAGING_RESULTS.md는history/2026-11-07-item-usability.

## 현재 대기·다음 행동
- 기본런처는기존DefensiveGear경로유지. 새Player/Server경로로원자교체하는명령은자동승인심사에서거절돼실행되지않았다. 설정해시동일·게임/서버미기동·디버그폴더원위치확인. 우회하지않는다.
- RUNPAIR01/r1 전환승인대기: 채널C0C9VFZMV43,스레드1794022476.887309,요청1794031948.946109. 명령 승인 RUNPAIR01. 두경로만백업/배타잠금/복구불변재확인후교체하고오프라인check만요청. 기동·DB변경제외. 아직회신미수신·미소비. 승인후에도도구심사를다시적용한다.
- 새후보를향후기동한뒤구버전으로돌아갈때는최신DB/WAL/정상flush를재대조한다. 단순이전경로복원으로안내하지않는다. 기존IEQ2/WAL3구서버복귀제약도유지한다.
- 사용자부재중PC입력/보안창/새정책작업은보류. 이번2시간세션은주요결정마다계획/검수했고영구공통지침은바꾸지않았다.
- 실제활성화724(신규264/후속460),총괄직접작업/도구/검사제외,토큰·비용N/A.
