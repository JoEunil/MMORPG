최신: Slack 승인 확인. 정상 q 종료 중 오류 증가로 검사 중단, DB 쓰기0/서버 정지. 후속 BAG02_REQUEST.md 참조. 아래 승인대기는 당시 이력이다.

# BAG01 — 전용 캐릭터 빈 인벤토리 보완 승인 요청

승인대기. 지정Slack부모1792407691.861929/요청1792408332.366609,짧은회신 `승인 BAG01`,지정사용자U0C9QEUHPMK원문확인/중복방지후에만실행한다.

범위: game.characters_inventory에char_id5011/5012용all-zero360바이트각1행,총2행만COMMIT. 정확소유계정unity_ai_01/02,user6981/6982,profile8192/8193및이름고정검사. 기존행하나라도있으면INSERT/UPDATE없음. 아이템0·재화0/계정·character수치·schema·서버정책변경0.

후보: UnityClient/Tools/Prepare-C12EmptyInventory.cjs. 기본SELECT-onlyROLLBACK검사통과. ABI/static_assert와codec실행통과/EMPTY_INVENTORY_REVIEW.md최종코드차단0. 정상q종료/drain와프로세스/9999연결부재/rootWAL전체0근거뒤commit모드,오류전체ROLLBACK/commit모호하면SELECT확인만/자동삭제재시도0. 조건실패면DB미실행/동일C10바이너리재시작은기존가드검토후별도정상운영절차.

실행전원문사람승인·정상종료실근거·최종후보SHA를다시확인한다. 후속기능검증은서버실가방조회/Refresh/패널전환/정상로그아웃이며cold첫실패와최종빈성공을구분한다. 성공전공개가방설명을바꾸지않는다. 전체원인·SQL·복구경계 NEXT_ITEM_CONTRACT.md.
