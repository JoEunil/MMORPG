# 기존 스킬·몬스터 전체 편집 확장

목표: 기존 ID0..4 스킬/ID0..2 몬스터를 모두 Unity 저작·검증·내보내기로 연결한다. 현재 Ring·monster0 에셋의 필드/편집값을 보존하고 남은 기본값만 확장한다. 기존 보스 패턴/능력치/보상/ID/배열 순서/MonsterData 생성자 파생값은 유지한다. BRIEF S01의 기존 콘텐츠 범위이며 신규 게임 시스템/ID/DB/프로토콜/Zone구조·큐·락/핫리로드는 제외한다.

기준: codex/content-catalog는 승인WORLD01 게시완료 head eacb744에서 분기. WORLD01회신1792420805.879599는 정확eacb744를 PR37에반영하는데1회소비했다. 이 새묶음은별도검증·게시직전Slack승인이필요하며기존승인에끼워넣지않는다. 현재새부모1792420978.697599,수신기threads등록. 원본Game/root서버WIP보존.

구현소유: C14ServerContent.cs/C14ServerContentExport.cs/C14ContentExportTests.cs 및 CoreLib/SkillData.cpp/MonsterData.h의테이블본문만. Coordinator소유: 실제asset업그레이드/생성inc/obsolete생성물정리/native동등성probe/통합/사이트/Git. 리뷰는읽기만. C15스폰관련파일은수정하지않는다.

데이터: 기존ring/monster0 이름·타입·값은유지하며 otherSkills(0,2,3,4)/otherMonsters(1,2)만추가한다. 누락된새배열만기본값이행하고nonempty사용자데이터를자동덮어쓰지않는다. 정확IDset/중복/누락/finite/range/HPsigned/phase/step/참조검증을모든출력쓰기전에마친다. 기존skill3은4phase·skill4는10phase, tick은duration이다. 출력은전체5스킬의Skills.inc와3몬스터의Monsters.inc만으로단일화하고이전작은생성조각은새전체출력확인뒤제거한다. 범용데이터플랫폼·새추상화는추가하지않는다.

수용: 기존편집값보존, 기본전체테이블의모든의미필드와파생값동일, 반복출력동일, 잘못된후보는기존출력불변. 실제UnityAPI로기존asset업그레이드, YAML손편집금지. 동일C14assetGUID유지. 실행서버자동반영으로주장하지않는다.

최소검증: 구현·독립리뷰뒤 CatalogExport2시나리오(전체출력deterministic/기존값보존·invalid기존출력보존), native한번으로이전eacb744실제C++테이블사본과새전체테이블의모든필드비교. baseline은Captures/CatalogBaseline에편집전읽기사본으로확보했다. Scene/게임입력/사냥/거래소/스폰/전체Unity·C++회귀는반복하지않는다. Inspector확인은확장항목1화면·대표캡처1장만. 통과후사이트기능범위/이력갱신과게시직전승인을한묶음으로진행한다.