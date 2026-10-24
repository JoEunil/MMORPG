# 사냥 상태 표시 묶음

목표: 기존 서버의 레벨·원시 EXP를 자기 HUD에 표시하고, 관측 중인 몬스터의 실제 HP/maxHP를 머리 위 체력바로 보여준다. 사용자 기능 묶음으로 PR36 병합 후 codex/hunting-state(master5bf5318)에서 후속 콘텐츠 작성 결과와 큰 PR 하나로 전달한다. 작은 단계별 PR은 만들지 않는다.

범위: C02WorldScreen의 기존 Full/Delta(4=exp/5=level) 반영, C05 기존 aim 텍스트 한 줄, C04 검증된 MonsterFull/Delta에 체력바 연결, 작은 런타임 uGUI 컴포넌트. 서버·프로토콜·DB·ClientCore·DLL 변경 없음. 경험치 비율/레벨업 조건·보상 팝업·피해량·타깃 선택·락온·이름 추정·아이템 추가 제외. 운영 중 서버는 변경하지 않는다.

소유권: growth 담당 C02WorldScreen/C05CombatScreen/C12HudTests만. vitals 담당 C04ObservedWorld/C13EntityVitals(.meta)/C04ObservationTests만. Coordinator는 통합/Editor/증거/공개 설명/Git. 리뷰 완료 후 Assets/Packages/ProjectSettings 동결, Editor 조작자Coordinator 한 명.

수용: 자기 Full과 EXP/level Delta 표시, 타인/이전 세션 혼입 없음, ushort 레벨 경계 검사, 초기화/재진입 보존. 몬스터 Full/Delta의 검증된 HP비율과 머리 위 표시 일치, HP0/관측만료/Zone·로그아웃에서 체력바도 제거. UI가 입력을 가로채지 않고 기존 MonsterVisual/경계 처리 유지.

검증: 구현·독립 리뷰 뒤 새 자동 시나리오 총3개만(성장 Full/Delta+타인분리, 성장 수명/경계, 몬스터 Full/Delta/제거). 실입력은 로그인→성장 표시→기존 스킬1회와 실제 몬스터 상태/체력바 관찰→정상 퇴장 한 흐름, 대표 캡처1장. 처치가 없다면 EXP획득/레벨업 실증은 주장하지 않는다. 전체103/C++/거래소/두 계정 왕복 재시험 금지. 새 실패면 영향 범위만 고치고 반복 한도를 유지한다. 소스3시나리오·실제흐름·사이트대상페이지만 통과하면 종료하고 다음 승인 범위의 미완료 작업을 이어간다.