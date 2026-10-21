# 직선 스킬 방향 일치

원인: movement0=+Y/1=-Y와Rectangle의수직판정이반대. 클라이언트만dirflip하면Move가영웅방향도뒤집으므로서버Rectangle2분기교정+HUD방향안내를동일규약으로맞춘다. 범위/폭/피해/쿨다운/wire/DB변경0. 실제범위프리뷰컴포넌트는현재없으며이번에구현됐다고주장하지않는다.
최소diff CoreLib/SkillData_Range.cpp, C05CombatScreen.cs LineDirection. 일반몹0/1·보스현재skill데이터는Rectangle을쓰지않으므로기존몬스터AI회귀확대0. 과거원본2D도+Ydir0인관례와일치.
검증동결후 nativeRectangle1개(4방향정면/후면/폭끝경계/외부), 기존C05UIfixture1개에4방향HUD확인병합, 실제로그인1회에서수직이동→C1회→서버dir·대상HP반영확인. 전후캡처1~2, 이전처치/가방/채팅전체회귀0. 실제실행조건미충족이면미실행기록.
실행비용: 별도Line-Aim출력폴더에MainServer/UnitTests빌드,기존서비스DB/Redis보존,기존로그인서버정상시작. 게임서버는현재중지되어후보준비뒤정상1회기동예정. 기존UI-Chat-Map실행파일/로그보존. 준비실패시강제종료없음/중복기동없음.
종료: 방향판정/UI직접검증통과+실제일치확인또는실제차단사유. 반대방향의몬스터유도·범위시험을여러계정/여러접속으로반복하지않는다. 성공예정만으로기록하지않음.
역할: line_aim_contract(server_analyst)신규1/리뷰재활성화1, line_aim_implementation(client_implementer)신규1/수정재활성화1. 총괄이테스트코드의잘못된forward*2외부기대를정적으로발견하여실제길이12밖으로정정지시. 실행실패숨김/기존assert약화아님. 누적신규18/재활성화51/69회, tokenN/A.
복구: 기존서버exe로정상전환및이커밋revert, DB마이그레이션없음. 실행플랫폼승인은프로젝트Slack과별도유지.