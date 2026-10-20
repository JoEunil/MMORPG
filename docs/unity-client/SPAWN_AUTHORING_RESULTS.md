# 몬스터 배치 편집 결과

C15 typedasset의 일반/강한 몬스터 수와 시작 간격을 편집하고, 5×5 Inspector 미리보기와 구역 합계, 검증 후 C++ 상수 내보내기를 연결했다. Cell/AOI/Zone 기하·용량 구조, 큐/락, 스폰 ID 증가·중앙 보스·재생성/AI는 변경하지 않았다. 서버 소스 변경은 CoreLib/ZoneState_Monsters.cpp의 include와 InitializeMonster 숫자 참조뿐이다.

기본값 8/2/1을 유지했다. 셀폭22의 실제 난수식은 min+1+rand()%21이므로 offset1..21이다. 탐색 보고의1..20 추정과 첫 Inspector의 inset..22-inset 표시는 원문 식에 맞춰 수정했다. 일반 type0 다음 type1, x/y 난수 호출 순서 보존은 source diff로 검토했고 특정 난수열의 전후 숫자별 동일성을 실행 비교했다고 주장하지 않는다.

검증은 최종 구현·독립 리뷰 뒤 한 묶음으로 종료했다.
- SpawnExport category 정확2/2 PASS. 기본 UTF8 noBOM/LF 출력·locale/반복 동일, 잘못된 count/inset은 기존 출력 보존. Captures/spawn-tests-final.txt.
- 실제 ZoneState_Monsters.cpp와 SkillData.cpp를 MSVC C++20으로 컴파일하고 spawn-equivalence.cpp에서 실제 InitializeMonster 호출. 241마리/24셀의8+2/type순서/internalID·셀참조/정수좌표1..21/중앙boss ID240 PASS. 첫 단독 컴파일은 External include 경로 누락으로 실패했다. 기존 프로젝트 include/UTF8 설정만 보완하고 재컴파일·실행 PASS. 제품/조건 완화 없음.
- 실제 Windows Inspector 입력으로 일반 수8→9: 24개 셀9+2, 합계265 확인→8복원: 8+2/241 확인. 최종 실제 에셋8/2/1 저장/재export, Scene dirty/Playfalse. 정리된실캡처 monster-spawn-editor.jpg1장. 패킷/상태 주입·서버 배포0.
- C15 테스트 소스/실제asset/생성inc와 root 통합본6파일 hash 일치. Captures/spawn-freeze.txt. 기존C14/사냥/전체Unity·C++회귀 반복0.
- 사이트6기능 build성공. 변경된server-content상세·이력·새JPEG HTTP200, 이전캡처보존. CSS변경없어전체모바일matrix반복0.

독립 최종 리뷰 차단0. .asset/.meta는 실제 Unity CreateAsset로 생성했다. generated MonsterSpawns.inc SHA256 83FF78BC46D584F2362476A818414DC2658840719B907CB4D2ABD0F6AC145D88. 유효성 검증을 모든 출력 쓰기 전에 수행하며1000은 후보 저작 예산이고 서버 hard cap으로 바꾸지 않는다.

실행 서버는 BAG02 PID31796을 그대로 유지했다. 실행 서버 재시작·배포·DB직접쓰기·추가 게임 로그인0. 새 기하는 미지원이므로 셀수/크기 등 고정 구조 변경은 별도 보호 영역이다. 원본Game/root서버WIP보존. 이번 새묶음은 PR37 head에서 분기한 로컬 codex/world-content이며 push·PR 갱신 전 Slack WORLD01 승인을 기다린다. PR37이 열려 있으면 같은 큰 PR에 이 콘텐츠 편집 결과를 합쳐 새 작은 PR을 만들지 않는다.

사용: Assets/C02/Content/C15MonsterSpawns.asset Inspector에서 편집, C02/C15/Export Validated Monster Spawns 메뉴로 내보내기. 서버 재빌드 전까지 실행 반영되지 않는다. native probe 재현은 repository root와 External을 /I에 지정하고 docs/unity-client/spawn-equivalence.cpp, CoreLib/ZoneState_Monsters.cpp, SkillData.cpp를 /std:c++20 /utf-8 /EHsc로 함께 컴파일·실행한다.