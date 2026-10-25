# V03 — Zone 1 필드 외곽 보완

목표: V02의 밝은 판타지 MMORPG 재질과 성인 캐릭터를 보존하면서, 실제 서버 Zone 1 좌표 [-5,105]에서 쿼터뷰 카메라가 지면 끝/빈 배경을 보지 않게 한다.

- 포함: 월드 고정 확장 지면, 기존 자작 V02 나무·바위·능선의 외곽 재배치, 새 V03 재질과 Builder의 멱등 적용 경로, C02 장면.
- 제외: 런타임/서버/좌표/카메라 추적 로직, collider/navmesh, C04 캐릭터·몬스터 Prefab, 로그인 diorama와 UI 변경, Play/로그인/입력.
- 화면 방향: 흙길과 올리브 잔디, 따뜻한 석재, 먼 청록 산 능선, 플레이 영역 밖으로 물러난 소나무 숲. 장식은 충돌/서버 이동 경계를 의미하지 않는다.
- 검증: Editmode compile, 누락 참조·추가 Collider 수, 월드 좌표 4모서리 및 중앙 (39,39)의 실제 QuarterViewCamera 투영 지면 커버리지, 정적 카메라 렌더 PNG. 실제 입력/서버 검증은 Coordinator의 C05 공통 배치에서 수행한다.
- 비용/종료: 저작 1회, 오류 시 영향 범위만 수정. 정적 확인 후 소유 파일을 동결하고 Editor 제어권 반납.

출처: 저장소에서 직접 저작한 V02 Mesh/Material/Texture의 Unity Editor 재사용 및 Unity 기본 Plane. 외부 다운로드·유료 에셋·이미지 생성 없음. 기존 출처/사용 조건은 Art README를 승계한다. 새 리소스는 해당 프로젝트를 위해 자체 구성한 재질/배치다.

현재 상태: 아트 저작·정적 검증 완료, Coordinator 독립 리뷰/실제 C05 공통 배치 대기. Builder/Scene/Editor 소유권을 반납했다.

- 최종 Unity compile completed/failed=false/errors=[]; Play/로그인/입력 실행 0.
- 정적 실제 QuarterViewCamera 1280×720, (39,39) 및 (-5,-5),(-5,105),(105,-5),(105,105) 렌더 PNG 5장. 중앙과 대각 모서리 2장을 직접 시각 검수하여 지면 끝/배경 노출과 기존 짧은 지면 중첩 이음매 제거를 확인했다.
- 20개 viewport 모서리 지면 교점 모두 확장 지면 안. 고정 카메라 크기/방향에서 투영이 평행 이동하므로 Zone 1 내부도 포함된다. 현재 16:9 구도 검증이며 임의 초광폭 화면 주장은 하지 않는다.
- ApplyV03 재적용 후 root 1 / Transform174 / Renderer125 / 새 Collider0 / missing Script·Mesh·Material0. 저장된 장면 복원 후 world inactive, Play=false, dirty=false.
- C04 4개 Prefab SHA는 C04 동결본과 동일하다. 기존 V02 모델·재질·텍스처를 재저장하지 않았다. 원래 짧은 월드 지면 renderer와 월드 능선만 숨기고 로그인 diorama는 보존했다.
- 단순 지면 복제의 색/UV 차이 및 미세 높이 중첩 경계를 정적 캡처에서 발견했다. 정확한 Unity Plane 반전 UV를 확인한 뒤 원본64m 주기를 유지하고 단일 확장 지면만 렌더해 정리했다.
- 근거: `UnityClient/Captures/v03-static/coverage.txt`, `quarterview_*.png`, `UnityClient/Captures/v03-art-freeze.txt`. 전체 Zone 상세 랜드마크·새 애니메이션은 이번 범위에 포함하지 않는다.

## 추가 시각 보완 — 중앙 랜드마크

Coordinator가 빈 녹색 중앙 필드의 방향 읽기 보완을 배정했다. 기존 통합 C05 Builder BA1787E1… / Scene AED8C23E…를 기준으로 ApplyV03 범위만 확장했다. C05·C04 연결 및 HUD 생성 구간은 수정하지 않았다.

- 첫 추가 revision은 (50,50) 주위 낮은 분리 석재28개와 포장7개였으나 root 시각 리뷰에서 보드게임 타일 같은 원주 배치로 판정되어 대체했다. 최종 source는 짧은 arc3곳/작고 묻힌 석재12개/작은 포장7개/외곽의 낮은 벽조각2곳으로 제한한다. 원주의 약80%를 비우고 중앙 통로/시야를 유지한다. 경계/스킬 범위 표시나 보스 사전 배치가 아니다.
- 필드 저밀도 군락23개, 각각 기존 풀5개+낮은 돌1개. 플레이 영역 내 새 높은 나무 없이 서버 actor 시야를 유지한다.
- 새 V03 재질2개는 기존 석재/흙길 Texture 재사용. 이전 원주 revision의 정적 결과는 Transform371 / Renderer299 / Collider0 / missing Script·Mesh·Material0, compile completed였다. 마지막 source 보정 시 다른 담당의 C02 입력 helper 미정의 변수9건으로 통합 compile이 차단되어, 그 회차 PNG는 이전 code로 평가됐다. `precompile-oldring_*.png`로 구분 보존했다. 최종 새 revision의 적용/compile/캡처는 해당 차단 해결 뒤 따로 기록한다.
- 실제 QuarterViewCamera 정적 PNG (39,39)/(50,50)/(105,105) 직접 확인. 중앙 석재의 과도한 연속 띠를 돌 간격·회전·크기 변주로 한 번 보정했다. 실제 Play·입력·로그인 0.
- C05 필수 참조5개+skillButtons3개 모두 정상. CharacterChoices.sizeDelta는 변경 전부터(0,0); 실제 viewport/scroll Rect는(304,87), 10rows/spacing2를 보존했다. 304는 content height가 아니라 viewport width다.
- C04 4 Prefab 해시 보존. 저장된 C02 Scene worldInactive/dirty=false. 현재 추가 revision 해시는 `Captures/v03-art-landmark-freeze.txt`에 기록한다. 기존 `v03-art-freeze.txt`는 초기 지면 커버리지 revision 기록이다.

최종 보정 적용 완료: 통합 helper의 미정의 변수9건과 이어진 non-static ResultPath 참조2건은 해당 담당/Coordinator가 수정했고, 그 후 compile completed/failed=false/errors=[]를 확인한 뒤 새 ApplyV03를 실행했다. 최종 정적 PNG3장(39,39)/(50,50)/(105,105)은 새 source의 불연속 폐허 배치다. 이전 원주 사진과 구분된다. 최종 Transform361 / Renderer289 / Collider0 / missing Script·Mesh·Material0. C05 참조5개+버튼3개, C04 component1, 선택 viewport/scroll(304,87)·10행 보존. Play/compiling/worldActive/dirty 모두false. 저디테일 프로토타입 폐허라는 한계는 유지하며 추가 고밀도 미술 반복은 종료한다. Builder/Scene/Editor 반납.

Slack 저작·정적 완료 전용 스레드:1792320059.713559 (Unity Coordinator 앱). C05실제배치통합후이곳에도실행결과보고. 수신threads.txt에등록함.

## 실제 배치 완료

계정2 실제native5클릭으로 외곽(-4.7525,-4.6584)에 도달, 서버/OwnCharacter좌표일치·HPMP10000확인. Captures/c05-final-account2-outer.png에서 지면 밖미노출을root가직접검수했다. 중앙실전투/HP0PNG에서도최종Scene사용. Builder2D79166F/SceneE5F857DF유지, 저디테일한계유지. UnityCoordinatorbot V03완료회신1792322671.215899. 추가미술/입력반복없이종료.
