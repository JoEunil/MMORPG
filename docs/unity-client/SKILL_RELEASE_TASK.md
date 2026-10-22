# 스킬 예고와 발동 효과 분리

목표: 서버가알린C스킬charging예고와짧은발동효과를독립표시하고, 스킬발동을움직이는확장pulse로보여준다.
원인: 기존ShowReleaseEffect가linePreview를재사용해다른스킬phase1이Ccharging예고를덮음. 단순정적윤곽선외실제발동표현이미흡함.
허용: C05CombatScreen/기존C05CombatTests두파일. 기존shader/현재Circle·Rectangle콘텐츠만, 자산·서버·판정·스킬수불변.
표현: acceptedownphase1의원점/방향/범위스냅샷에서 .35초동안범위내pulse확장과굵기감소. 전용renderer하나재사용, 타skill예고와독립수명·정리. 파티클에셋제작을완료했다고표현하지않음.
검증: 독립리뷰후영향기존fixture1개만실행, 실제입력/로그인/서버재시작0. 필요하면분리previewScene정적표시로가독성확인하고실제게임화면과구분.
전달: 기존FEEL01승인스킬효과범위로큰PR44에함께모음. 다른미게시AUTHOR01범위/사용자A*는별도보존.
