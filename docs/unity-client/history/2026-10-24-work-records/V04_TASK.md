# V04 — 쿼터뷰 RPG 월드 개편

사용자 지시: 현재 그리드처럼 보이는 월드를 RPG/쿼터뷰에 맞게 수정해도 된다. HP 저장 문제 조사를 우선하며 독립적인 미술 개편을 병행한다.

목표: 기존 한 Zone의 평면 격자 인상을 줄이고 비대칭 숲 경계·굽은 길·자연스러운 지면 색/식생 군락·낮은 폐허/랜드마크로 쿼터뷰에서 방향이 읽히는 대표 필드를 만든다. 기존 정통 판타지 비율/장비와 시야를 유지한다. 새 게임 시스템·NPC·포탈·퀘스트 UI는 추가하지 않는다.

소유: 아트 역할만 UnityClient/Assets/C02/Art/V04/ 및 필요한 meta, C02SceneBuilder.cs의 V04 저작 부분, C02.unity, 이 카드의 저작/정적 검증 기록. 기존 C05/C04 코드·HUD·Prefab·C01 선택 흐름은 보존한다. 다른 역할은 같은 파일을 수정하지 않는다.

Editor: 아트 담당 Edit Mode 단독 조작, Play/실입력 금지. 서버/DB/프로세스/ClientCore 수정 금지. 논리 좌표·AOI·서버 이동 경계·기존 fieldPlane picking collider는 그대로 사용한다. 걸어갈 수 있는 땅은 서버 평면과 맞추며 높이 때문에 캐릭터가 뜨거나 가려지지 않게 한다. 시각 폐허는 낮고 통로를 열며 새 collision/서버에 없는 막힌 길을 만들지 않는다.

완료 조건: 중심/진입/외곽 실제 QuarterViewCamera 정적 렌더에서 반복 격자 인상이 줄고 지면 밖 노출이 없으며 캐릭터와 적이 읽힌다. C05 참조/3버튼, C04 component/prefab, 선택 viewport304 및 picking 보존·컴파일0. 첫 대표 렌더를 Coordinator에게 보고하고 필요한 한 번의 보정을 거쳐 동결한다. 동결 후 실제 이동/카메라 검증은 R01 승인된 저장 재현과 가능한 한 배치로 합친다. 입력 통과는 정적 렌더와 구분한다.

Slack: 변경별 전용 스레드에서 단계 완료 결과·다음 명령·재개 명령 요청을 함께 남긴다. 다음 단계의 새 사용자 신호가 필요한 경우 명령을 분명히 안내하고 자동 승인으로 해석하지 않는다.

## 저작·정적 검증 완료 — 2026-10-18

- `ApplyV04()`를 Builder의 독립 저작 부분에 추가하고 실제 Editor API로 Scene을 저장했다. 기존 V03 root와 월드 outpost의 교대 배치 수목/풀/돌은 비활성 보존했다. 로그인 디오라마·성문·성인 갑옷 캐릭터·C05 HUD·C04 Prefab은 유지했다.
- 240m 단일 평면(-70..170)에 고유한 연속 지면, 세 구간 Bézier의 굽은 흙길, 넓은 비정렬 식생 군락, Zone 바깥의 비대칭 숲, 열린 낮은 폐허/가까운 이끼 바위 군락을 배치했다. 서버 좌표·AOI·논리 경계·평면 이동·기존 picking MeshCollider는 수정하지 않았다.
- 첫 대표 렌더를 Coordinator에게 전달한 뒤 1회 보정했다. 첫 고주파 흙길이 자갈/체커처럼 보이는 문제는 주파수/대비 축소로 제거했고, 길 폭/경계를 줄이고 중앙 구석의 낮은 군락을 보완했다. 첫5장은 `Captures/v04-static/first-pass/`, 보정 최종5장은 그 상위 `quarterview_*.png`다.
- 실제 QuarterViewCamera와 런타임과 동일한 offset(12,16,-12)/lookAt(y1)/ortho7.2, 1280×720로 진입(10,10)·중앙(39,39)/(50,50)·외곽(-5,-5)/(105,105)을 Edit Mode 렌더했다. 캐릭터는 저작 프리뷰에서만 임시 위치 후 원복했다. Play·실입력·서버 호출 0회다. 정적 캡처에는 실제 몬스터/오버레이 HUD 통과 주장이 없다.
- 최종 V04 Transform1515 / Renderer1297 / 새 Collider0 / missing Script·Mesh·Material0. C05 참조5+버튼3, C04 component1/Prefab4, 선택 viewport304×87, FieldPlane position0/scale(6,1,6)/기존 MeshCollider 유지. 컴파일 completed/failed=false/errors=[]; Play/compiling/worldActive/Scene dirty=false.
- 처음 `eval ApplyV04`는 기본5초 dispatcher timeout을 반환했으나 실제 저장 완료/root1/dirtyfalse를 후속 조회로 확인했다. 재적용은 `run_script`의 timeout_ms30000으로 수행했다. 실패를 성공 응답으로 바꾸어 기록하지 않았다.
- Coordinator가 보정본의 지나치게 흐린 지면을 수용하지 않아, 새 배치/모델 확대 없이 지면 가독성 결함만 추가 수정했다. 전역 곡선/색 mask를 유지하고 URP Lit의 실제 `_DetailAlbedoMap`/`_DetailAlbedoMapScale`/`_DETAIL_SCALED`를 확인해 중립 선형 detail128²를 분리했다. V02의 두 noise 주파수를 16m 주기로 복원하고 타일 경계 연속 혼합으로 반복선을 피했다. 흐린 중간 버전은 `v04-static/pre-detail/`에 보존했다. 첫 새 shader variant 렌더는 진입 이미지의 detail이 미반영되어 재렌더 후 두 대표 지점 모두 디테일을 직접 확인했다. 기존 5개 최종 PNG는 detail 적용본이다.
- 최종 Builder SHA `551837349C673C2F42BB5C4A7B2957D474A7324C86B65A9848D422B6F0B24C31`, Scene SHA `E2D2E68CE072EBE4E7921422706700B8539B80EDEDE36ADDEED48526A3F93475`. 전체 동결 `Captures/v04-final-freeze.txt`, 정적 결과 `Captures/v04-static-check.txt`. 이전 B52367/ADD9F1 중간 동결은 이 결과로 대체했다.
- 원본 메시·재질은 저장소 자체 V02/V03 저작물을 재사용했다. 새 지면은 Unity Editor Texture2D API의 파라미터/노이즈/곡선에 의한 자체 저작이며 외부 이미지·모델·유료 에셋·별도 라이선스 의존성이 없다. 전역Texture1024²/RGB24/sRGB/mipmap/trilinear/clamp/aniso4 + 국소detail128²/RGB24/linear/mipmap/trilinear/repeat/aniso4/scale15/strength.85, URP Lit 재질3개, 평면 Mesh1개. 생성 이미지/3D 서비스를 사용하지 않았다.
- 한계: 낮은 디테일의 평면 필드 프로토타입이다. 실제 이동/카메라·몬스터 가독성과 GPU/프레임 성능은 아직 미검증이다. 전체 식생 Renderer 수가 증가했으므로 실제 배치에서 가시성·성능 문제 발생 시 근거를 남긴다. 이 기록만으로 입력 배치를 통과 처리하지 않는다. 추가 미술 반복을 종료하고 동결/Editor 조작권을 Coordinator에게 반납한다.

## 다음 단계 재개 요청

독립 정적 리뷰 차단 문제 없음(V04_REVIEW.md), root 최종 대표2PNG직접수용. UnityCoordinatorbot1792326992.578739로 `!coord 재개 V04 실행검증`을 요청했다. 실제 이동/시야/성능은 아직미검증, R01/필요운영조건을 먼저확인한다. 현재 사람재개회신없음/모든Assets동결·Editor반납.
