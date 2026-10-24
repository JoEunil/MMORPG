# 몬스터 스폰 배치 편집 묶음

목표: 기존 맵 셀 구조를 유지하며 셀별 일반/강한 일반 몬스터 수와 가장자리 시작 간격을 Unity 에셋에서 편집·검증·미리보기하고 C++ 초기화가 생성 상수를 사용하도록 연결한다. BRIEF S02의 콘텐츠 배치만 진행하고 Zone/Cell 기하·AOI·큐·락·용량 구조는 변경하지 않는다.

기준: codex/world-content는 PR37 head ded6c582에서 분기했다. PR37은 조회 시 open/unmerged이므로 새 묶음은 원격 게시 전 기반 병합 상태를 다시 확인한다. 이전 PR 권한을 새 게시 승인으로 재사용하지 않는다. 테스트·리뷰가 끝난 구체 diff를 지정 Slack 부모1792420090.451099에서 승인 요청한 뒤에만 push/PR 생성한다. 로컬 구현·검증·커밋은 계속한다.

기존 동작: 한 Zone의 5×5 셀 중 중앙(2,2)을 제외한24셀에 type0 8마리→type1 2마리 순서, 중앙 type2 보스1. 총241. 셀폭22에서 offset=1+rand()%21, 범위1..21이며 1..20이 아니다. x/y마다 기존 rand호출과 순서를 유지한다. uniqueID 증가/셀 monsterIndexes/보스 위치·중앙 제외·재생성·AI는 그대로다.

에셋은 regularPerCell=8,strongPerCell=2,edgeInset=1만 편집한다. counts>=0, inset1..21, 총24*(counts합)+1<=1000을 저작 예산으로 검사한다. 1000은 서버의 reserve 값에서 선택한 후보 저작 상한이며 서버 hard cap이라고 표현하지 않는다. 기하5×5/22는 고정 설명과 컴파일 assert로 묶는다. 기존 Ring/monster0와 C14 출력은 변경하지 않는다.

구현 담당 소유: C15MonsterSpawns 타입, Editor exporter/UI Toolkit Inspector, 최소 자동2시나리오. Coordinator 소유: ZoneState_Monsters.cpp InitializeMonster의 숫자 연결만, 실제 .asset/.inc 생성과 meta, 통합/검증/기록/Git. 리뷰는 읽기만. Scene/Prefab/Material/기존데이터/DB/ClientCore/프로토콜/인증/거래/ZoneState 구조는 제외한다.

수용: 기본값 내보내기로 기존241·type순서·rand식 유지, 잘못된 값은 기존 출력 불변, 반복 출력 동일. Inspector 5×5 배치 규칙과 합계는 편집 미리보기이며 실제 서버 위치/반영 상태와 구분한다. 실제 생성에 필요한 typedasset은 UnityAPI로 만들며 YAML을 손으로 생성하지 않는다.

검증은 모든 구현·독립 리뷰 뒤 동결하고 SpawnExport 자동2개(기본 deterministic/invalid 출력 보존), 실제 C++ 초기화 연결 compile/probe1회, Inspector 실제 편집 반영·기본 복원 한 흐름/대표캡처1장으로 끝낸다. 기존 사냥/거래소/전체 Unity·C++ 회귀는 반복하지 않는다. 기본수치 동일 후보이므로 실행 서버를 재시작하거나 추가 게임 입력을 할 이유는 없다. 신규 배포로 주장하지 않는다.