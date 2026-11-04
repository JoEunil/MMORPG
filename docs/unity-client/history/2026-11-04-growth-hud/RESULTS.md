# 성장 진행 표시 — 2026-11-04

## 범위

PR64게시 후 codex/client-growth-hud에서 이어간 클라이언트 전용 변경이다. 기존 ServerContent.growth와 서버 확정 level/누적exp를 재사용하며 서버·ClientCore·몬스터 AI·DB·성장 수치를 바꾸지 않는다. 현재 HP/MP·스킬·채팅 배치를 유지한다.

## 구현과 검수 피드백

CombatScreen의 기존 Lv·누적EXP 표시와 별개로 얇은 Filled EXP막대·남은EXP·정상상한 MAX를 추가했다. 현재 임계값≤exp<다음임계값일 때만 표시하며 레벨 역산·보정은 없다. level범위밖/표누락·중복·역전/level-exp불일치는 새UI를 숨기고 기존 서버값만 표시한다. 수명 초기화와 전환/죽음 경로를 연결했다.

기존 HudSetup 내 공통 ConfigureGrowthHud와 단독 ApplyGrowthHud를 사용한다. HudSetup·KnightInterfaceArtSetup·WorldSceneBuilder 생성 경로가 같은 구성을 사용하며 런타임 Manager·네트워크는 추가하지 않았다. Scene은 총괄이 실제 Editor에서 성장 UI만 적용/저장했다. root1개, 기존 GUID·배치 보존. Unity가 새로 만든 빈 YAML값의 trailing space20줄만 정리했고 기존 공백은 보존했다.

첫 검수에서 현대 Scene은 정상이지만 560×184 구형 생성패널의 스킬과 성장 행이 겹치는 문제를 발견했다. 같은 제작자가 현재 Scene 기본(-24,100)은 보존하고 생성 경로만 현재 레벨텍스트 폭208/새행(-24,height-63)으로 조정했다. 별도 배치 프레임워크 없이 선택 위치 인자 하나를 사용했다. 같은 검수자가 compact 예시를 열어 겹침 해소를 재확인했다.

## 확인한 결과와 한계

Unity6000.0.47f1/CLIbeta16. 대상5코드+Scene의ROOT/정본일치와백업확인. 재컴파일 completed/failedfalse/errors[]. GrowthHud fixture3개만 실행해3/3통과(.18초): 정상누적구간·Delta·uint경계,MAX와불일치·콘텐츠오류fallback,죽음/전환/재진입/종료수명. 기존commerce7개반복0. 생성경로보완은Editor2파일만재컴파일하고통과한런타임3개를다시실행하지않았다. git diff --check 통과.

hud-editor-preview.png는 현재Scene의CombatPanel복제·실제font/icon과예시Lv3/누적EXP190/다음110/40of150을임시Canvas-camera에서렌더한에디터예시다. compact-editor-preview.png는560×184생성구성과같은기존행/버튼크기와공유ConfigureGrowthHud로렌더했다. 임시객체는정리하고Scene저장/서버상태주입을하지않았다. 첫compact미리보기의빈화면은임시Canvas렌더레이어누락을고친뒤재생성했으며제품결함으로보고하지않는다.

최종종합검수 통과/필수0. 확인범위는계산fixture와에디터배치이며 새HUD의실서버통합·실입력은미실행이다. 공개화면에도EDITOR PREVIEW/예시값을표시했다. README대표는기존실제게임화면을유지했다. 후보파일SHA는verification/candidate-files.json이다. Editor Playoff,Scene dirtyfalse,서버/helper는앞선배치정상종료상태. 실제Agent누적581(신규225/후속356).
