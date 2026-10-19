# B05 배치 보조 코드 리뷰

검토자: Coordinator(root), 작성자: b05_prepare. 고정 SHA0868814FEF32B5DC9648B499FAB9600639FA71FDE33D5757B7F6E69F913F2C1E.
제품 코드/서버/wire 변경 없음. 배치 테스트 코드와 준비 문서만 추가했다.

- 기존 C02 UI coroutine의 native 로그인/선택/ENTER 및 C05 입력 경로를 재사용한다. 직접 UI handler/서버 결과 주입 없음.
- scene 로드 직후 ClientChanged 구독으로 ENTER 전 raw Zone/첫 Full 수집. 필터는 실제 본인 internal ID, 정리 시 구독 해제.
- 고정 C02 산출물은 먼저 고유 사본 보존. 새 결과/baseline/캡처는 이전 증거를 덮지 않는다.
- 원래 C02 iterator를 finally Dispose하여 비밀/임시 입력 정리 보장. helper finally도 ClientCore/자체 마우스/백그라운드 상태 정리.
- alive account1을 사용하고 HP0면 추가 목적지 입력 중단. 외곽 이동 후 정상 UI disconnect/10초 무접속을 유지, 외부 READ ONLY 확인 후 phase2.
- 계정2의 먼 시작점은 기존12-step 예산에 도달하지 못할 수 있어 이번 실행은 계정1만 지정한다. MP는 현재 값 보존만 확인하며 스킬에 의한 MP 감소 영속성은 검증하지 않는다.
- V04는 외곽 한 경로/대표 캡처만 이번 배치에 포함하며 전 구간 성능·모든 몬스터 가독성으로 확대하지 않는다.

현재 범위의 코드상 차단 없음. 실제 Unity 컴파일/실행 결과는 별도 실행 증거로 판정한다. 모든 Assets/Packages/ProjectSettings는 배치 동안 동결, Editor 조작자 root 단독.

최종 컴파일에서 누락 LowLevel namespace4오류가 나와 using 한줄을 복구했다. 최종 SHA FAF29C38639AFC2F183EFDA4F2D5DB2C5BB3E056897F3DC2DC0B6AE21D72DC65, 해당delta재검토/행동불변. 영향compile 재실행 PASS/오류0. Captures/b05-compile.txt. 이후Assets동결.
