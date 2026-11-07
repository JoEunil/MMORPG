# 현재 인계 — PR73·최신 기본 실행 경로 전환 완료

## 정본·후보
- ROOT: C:/Users/joeun/Desktop/workspace/Game-UnityClient. Git 정본: ROOT/UnityClient/Builds/ClientGrowthHUD/source, codex/client-item-usability. PR73 open: https://github.com/JoEunil/MMORPG/pull/73 . 코드 후보d98141f, master614c61a(PR71/72 병합) 기반. 병합하지 않았다.
- POTIONTIER01/r2는 현 채팅의 PR 개시 명령으로1회소비·게시완료. Slack수신승인으로꾸미거나다시사용하지않는다.
- 기존편집ROOT/UnityClient는보존. 최신Player는고정정본1859파일을별도로복제한ROOT/UnityClient/Builds/ItemUsabilityPackage/Project에서빌드했다. 현재Editor/Play모두종료,입력배치없음.

## 완료·한계
- 가방장착배지·개별효과·슬롯상태·공유쿨초,등급HP1500/3000/4500·MP500/1000/1500·6초공유,새등급가방만/1·2키기본유지,거래소선택효과연결완료. 상점/공급/드롭/DB시딩/AI/새프로토콜변경없음.
- 기존직접영향검사/독립코드리뷰/화면검수통과. 이번추가수명감사확정결함0,억지수정없음. 이미통과한게임검사는반복하지않았다.
- 새Windows64Player build_f463960a42d8 성공(오류0·기존경고5),Windowed1280×720설정·아이템9종·ClientCore원본해시일치·테스트어셈블리제외·씬누락스크립트0확인.
- 최신서버+설치MySQLOpenSSL쌍을비공개후보에보존,정적의존성/심볼756·x64·해시확인. 원본DefensiveGear의현재WAL2개복사/해시확인·원본보존. RUNPAIR01 후속 서버 단독 검증에서 준비 완료·DB/Redis TCP 연결·app-local DLL 로딩·정상 종료(exit0)를 확인했다. TLS/사용자 로그인/실제 등급 소비/조작감은 미검증.
- 후보: ROOT/UnityClient/Builds/LocalGame/TierPotions. 입력/빌드/패키지/복구manifest: ROOT/UnityClient/Builds/ItemUsabilityPackage. 상세PACKAGING_PLAN.md·PACKAGING_RESULTS.md는history/2026-11-07-item-usability.

## 현재 상태·다음 행동
- RUNPAIR01/r1은 현 채팅의 명시 승인으로 1회 적용 완료. Slack 요청1794031948.946109/부모1794022476.887309에 대응하며 Slack 답장을 수신했다고 기록하지 않는다. 종전 자동심사 거절 뒤 정식 권한 심사를 다시 거쳐 성공했다.
- 기본 StartGame.bat의 ClientExe·ServerExe는 TierPotions 쌍이다. 두 경로만 교체하고 나머지 설정 유지, 원본 설정 백업·180파일 해시·WAL 집합/해시 대조·offline check 성공. 디버그 폴더 이동 없음.
- 계속 진행 지시와 지정 테스트 서버 일상 권한으로 숨김 서버만 검증했다. 첫 도우미 로그 공유 오류는 종료 신호 후 프로세스·포트 정리를 확인하고 도우미를 수정했다. 실패 항목만 재실행해 준비/정상 종료 성공. 제품 코드는 바꾸지 않았다.
- 현재 서버·게임·Editor 종료. 로그인 서버/Player는 기동하지 않았다. 서버의 정상 WAL 재생/flush 가능성을 기록했으며 전후 WAL 해시는 같았다. 이 결과는 DB 무변경/TLS 검증을 보증하지 않는다.
- 실제 Player 화면·기존 보유품 조작은 비동기 입력 허용 질문 대기. 등급 포션은 현재 보유하지 않아 실제 소비 미검증이며 공급/DB 지급/획득 정책을 임의 추가하지 않는다. 통과한 기능 검사는 반복하지 않는다.
- 구버전으로 복귀할 때는 최신 DB/WAL/정상 flush를 다시 대조한다. 이전 경로/이전 WAL을 그대로 복원하지 않는다. IEQ2/WAL3 구서버 복귀 제약 유지.
- 근거: ROOT/UnityClient/Builds/ItemUsabilityPackage/launcher-transition-result.json·server-runtime-check.json 및 해당 history/PACKAGING_RESULTS.md. PR73에 같은 묶음으로 기록 반영, 병합하지 않는다.
