# 스킬·몬스터 데이터 편집 결과

Ring 스킬과 일반 몬스터 0만 Unity ScriptableObject에서 편집하고 검증 후 C++ initializer로 내보내도록 연결했다. 기존 기본값·ID·테이블 순서와 MonsterData 생성자의 aggroTimeOut/threshold 계산을 유지했다. 적용 파일은 새 C14ServerContent 타입·실제 Unity 생성 에셋·Editor exporter·생성 inc 두 개, CoreLib/SkillData.cpp와 MonsterData.h의 두 include 자리다. DB·인증·프로토콜·거래·Zone/큐/락/풀·ClientCore/DLL 변경은 없다.

Coordinator가 root Editor의 Play=false와 타입 로드를 확인한 뒤 Unity API의 CreateDefaultCandidate/ExportValidated를 실행했다. 수작업 YAML 생성 없이 실제 .asset/.meta와 UTF8 noBOM/LF 산출물을 delivery checkout에 복사했다. 기존 에셋 생성은 덮어쓰기 거부, 데이터 전체 검증은 디렉터리/출력 쓰기 전에 수행한다. float은 invariant roundtrip literal로 생성한다.

독립 리뷰 P2: maxHp uint의 상위 절반은 실제 서버 hp/wire int 범위를 벗어난다. maxHp > int.MaxValue를 거부하고 기존 invalid 시나리오에 2147483648u/기존 출력 불변을 추가했다. 최종 독립 리뷰 잔여 차단0. tick은 각 phase까지의 지연으로 유지하며 일반 절대시간 정렬 제한은 추가하지 않았다.

검증은 작업 카드의 최소 범위로 종료했다.
- ContentExport category 2/2 PASS: locale가 달라도 동일 기본 출력/반복 바이트, 잘못된 값은 두 기존 출력 보존. Captures/content-tests-final.txt.
- 실제 CoreLib/SkillData.cpp와 MonsterData.h를 MSVC C++20으로 컴파일·링크·실행한 content-equivalence.cpp PASS. 5 skills/3 monsters의 ID 순서, Ring 전체 초기값, monster0 기본·aggro100·thresholdAbs1500·두 step 일치. Captures/content-native.txt.
- 실제 Inspector에서 Ring/Monster 항목을 펼쳐 값 표시를 확인하고 server-content-editor.jpg를 선별했다. 데이터 변경·추가 Play입력0.
- 사이트 build 성공. 변경된 HUD·콘텐츠 상세/이력, 홈과 새 두 이미지 HTTP200. 스타일 변경이 없어 전체 브라우저/모바일 매트릭스는 반복하지 않았다.

첫 사이트 build는 후보 cwd에서 root 캡처 상대 경로가 맞지 않아 이미지 누락으로 실패했다. 실제 파일 절대 경로로 복사 후 build 성공. Computer Use의 잘못된 목록 API 명칭/이미지 PNG 가정은 입력·데이터 변경 없이 정확 API와 JPEG 원본으로 정정했다. 실패를 제품 검증 통과로 세지 않았다.

실행 서버 PID31796/BAG02-Candidate는 변경·재시작하지 않았고 DB 직접 쓰기도 없다. 새 편집 산출물은 C++ 후보 테이블에서 동등성을 확인했으며 실행 서버에 배포했다고 주장하지 않는다. 전체 Unity/C++ 회귀·사냥 입력 재실행0. 사냥 HUD·실캡처·공개 기록과 같은 codex/hunting-state PR 묶음으로 전달한다.

사용: Unity 메뉴 C02 → C14 → Export Validated Server Content. Assets/C02/Content/C14ServerContent.asset을 편집하고 UnityClient/Exports/ServerData의 두 inc를 함께 커밋한다. 이 후보는 서버 재빌드 방식이며 런타임 hot reload는 없다. 생성 결과를 직접 편집하지 않는다. 동등성 probe는 repository root를 include path로 지정하고 docs/unity-client/content-equivalence.cpp와 CoreLib/SkillData.cpp를 /std:c++20 /EHsc로 함께 컴파일·실행한다.