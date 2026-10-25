# 기존 콘텐츠 전체 편집 확장 결과

기존 스킬5개·몬스터3개를 모두 C14 typedasset과 검증 exporter로 연결했다. 기존 ring/monster0 필드·타입·값·GUID는 보존하고 otherSkills(0,2,3,4)/otherMonsters(1,2)만 추가했다. 서버의 테이블 struct/생성자·프로토콜·DB·Zone/큐/락·경제규칙은 바꾸지 않았다. CoreLib/SkillData.cpp·MonsterData.h의 배열 본문만 Skills.inc/Monsters.inc 전체 생성 데이터로 연결했다. C15 스폰 배치는 변경하지 않았다.

고정 diff 리뷰와 직접 원문 대조에서 보스 스킬3/4가 RingSkill 기본 mana20/cooldown120을 물려받는 값을 발견했다. 각 initializer에 기존값0/0을 명시하고 다시 동결했다. phase3의20→10 duration와 skill4의10phase를 유지했으며 시간순 정렬이나 임의8phase 제한을 추가하지 않았다. 정확IDset·중복/누락·순서·finite/range·signed HP·몬스터phase/step·참조를 전체 검사한 뒤 두 문자열을 구성하고 출력한다.

실제 Unity API의 UpgradeExistingCandidate/ExportValidated를 실행했다. 업그레이드 직전후 기존 Ring/monster0의 값이 같고, 이전 원본 YAML의 전체 prefix와 .meta hash도 일치했다. 새 배열만 추가됐으며 asset GUID e20a847d2d4fa094088b01f4c513165b는 그대로다. 빈 새 배열만 기본값으로 이행하며 nonempty 사용자 배열은 덮어쓰지 않는다. 실제 새 전체 산출물을 만든 뒤 오래된 PlayerRing.inc/Monster0.inc 조각을 제거해 live source 참조0 확인했다. 비교 baseline 사본은 ignored Captures에 보존했다.

최소 검증은 구현·독립 리뷰 뒤 한 배치로 종료했다.
- CatalogExport category2/2 PASS: 전체ID순서·기존편집값보존·결정적반복출력, 잘못된ID/phase/HP에서두출력불변. Captures/catalog-tests-final.txt.
- 실제 이전eacb744 C++ 소스/테이블 사본과 새 테이블을 같은 native probe에서 비교해5스킬/3몬스터의모든유효field·phase/step·float range·duration·보상·생성자파생값 PASS. MSVC C++20/UTF8 compile/link/실행1회, Captures/catalog-native.txt. 비교사본은시작전확보한CatalogBaseline이며기대값을신규출력으로덮지않았다.
- 실제 Inspector에서 보스스킬4/10phase·마나쿨다운0·강한몬스터/보스값 표시를 확인하고 content-catalog-editor.jpg1장 저장. 첫 capture는 최소화된 창으로 실패해 창 복원 뒤 같은 대상 재관찰했다. JavaScript 실패 셀의 바인딩 미생성은 재선언으로 정정했으며 값·실행 상태 변경은 없다.
- 실제asset/통합소스/출력6파일과 candidate hash 일치, Captures/catalog-freeze.txt. 기존 사냥/스폰/거래소·전체회귀·게임입력·서버재시작을 반복하지 않았다.
- 사이트 기존6기능 build성공. 변경server-content상세/이력/새캡처 HTTP200, 과거 화면 보존. CSS변경없어전체모바일matrix반복0.

최종 독립 리뷰 차단0, 생성asset/전체inc/include연결과obsolete참조정리도확인했다. 실행서버31796/BAG02와DB는유지했다. 신규스킬/몬스터ID나실게임수치변경·배포를주장하지않는다. 새묶음은 codex/content-catalog 로컬커밋 뒤 DATA01 Slack 게시승인을 따로요청한다. WORLD01회신1792420805.879599는이전eacb744의PR37반영에만소비했으며재사용하지않는다.

재현: 이전eacb744의CoreLib/{MonsterData.h,SkillData.h,SkillData.cpp}와UnityClient/Exports/ServerData/{PlayerRing.inc,Monster0.inc}를CatalogBaseline/아래같은구조로확보한다. native probe를현재SkillData.cpp와함께컴파일하며 repositoryroot/CoreLib/baseline상위경로를include로설정한다. Upgrade Existing Server Content Candidate 메뉴뒤Export Validated Server Content를실행하고 .asset/Skills.inc/Monsters.inc를함께커밋한다. 실행서버반영은별도재빌드·배포다.