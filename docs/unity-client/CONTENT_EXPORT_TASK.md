# 기존 스킬·몬스터 수치 내보내기

목표: 현재 Ring 스킬과 일반 몬스터 한 종류의 수치만 Unity 타입 에셋에서 검증·결정적으로 내보내 기존 C++ 정적 테이블이 사용하게 한다. BRIEF S01/P01의 기존 콘텐츠 데이터화 범위다. 모든 기존 ID·배열 순서·기본값·생성자 파생값은 유지한다.

변경 소유: 구현 담당은 새 C14ServerContent ScriptableObject 타입/작은 Editor exporter/테스트2개, CoreLib/SkillData.cpp·MonsterData.h의 두 초기화 자리 include 연결만. Coordinator는 UnityCLI 에셋 생성/내보내기와 실제 산출물·meta/통합/빌드/검증/Git, 독립 리뷰는 읽기만 한다. 실제 Scene·Prefab·NetLibrary·DB·프로토콜·인증·거래·Zone/큐/락/풀/핫리로드는 제외한다. 기존 작업 원본과 실행 서버를 보존한다.

입력은 Assets/C02/Content/C14ServerContent.asset, 산출물은 Exports/ServerData/PlayerRing.inc·Monster0.inc. ID1 스킬: flags2/mana20/cooldown120, Charging tick0 None 공격0와 Hit tick16 Circle8 공격100. ID0 몬스터: HP1500/attack5/respawn300/speed1, threshold1의 phase1개·skill0 delay20 두 step, EXP10. aggro100/thresholdAbs1500는 기존 생성자에서 그대로 계산한다. phase.tick은 지연 tick이며 일반 절대 시각 정렬 규칙으로 바꾸지 않는다.

수용: 검증 실패 시 기존 출력 불변, 기본 에셋을 반복 export하면 바이트 동일, invariant 숫자/UTF8·LF, 기존 테이블 크기·ID·기본값·파생값 동일. 존재하는 에셋을 생성 명령으로 덮어쓰지 않는다. 큰 범용 데이터/코드 생성기나 런타임 로더를 추가하지 않는다.

검증은 완성·독립 리뷰 뒤 ContentExport 자동2시나리오와 실제 C++ 테이블을 읽는 native 동등성 probe1개만. 전체 Unity/C++ 회귀·사냥 실입력을 다시 돌리지 않는다. 기본값이 동일한 생성 테이블 후보를 컴파일 검증하며 실행 중 서버에 자동 반영되었다고 주장하지 않는다. 배포를 할 때는 별도 현재 프로세스·정상 종료/저장/WAL 근거를 유지한다. 사냥 표시·콘텐츠 작성·실증·사이트 결과를 큰 PR 한 건으로 묶는다.