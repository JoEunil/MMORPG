# 낮은 자연석 윤곽 개선

2026-10-20. 후보 통합·에디터 시각 확인 완료. 실제 게임 입력은 반복하지 않았다. 토큰 사용량 N/A.

## 목표와 변경

V04의 BuriedOutcrop과 BuriedShrineStone이 얇은 직사각 판으로 보이는 문제에 한정한다. 기존 높이·위치·스케일·재질을 유지하고 비대칭 윤곽과 넓은 경사면을 가진 낮은 돌 메시로 바꾼다. 신규 이미지 생성, 지면 detail 재적용, 모델/리깅/패키지 추가는 없다.

수정 파일은 `UnityClient/Assets/C02/Editor/C02SceneBuilder.cs`다.

- `MeadowStoneMesh()`: `Assets/C02/Art/V04/MeadowChamferedStone.asset`이 있으면 재사용하고 없을 때만 만든다. 8개 비대칭 외곽점의 하부/테두리/상부와 중심이 어긋난 낮은 꼭대기를 연결한다. 48삼각형·144정점, 면별 노멀, XZ 투영 UV. y범위 [-.5,.5]여서 기존 바위의 수직 범위를 늘리지 않는다. 외부 모델 의존성 없는 직접 작성 프로시저 메시다.
- `ApplyV04()`: 기존 돌을 생성하는 두 곳에서 MeshFilter.sharedMesh만 이 전용 메시로 연결한다. 다음 전체 V04 재저작에도 연결이 유지된다. 현재 씬 비교에는 전체 V04 재생성을 사용하지 않는다.
- `C02/Art/Apply Meadow Stone Mesh` → `ApplyMeadowStoneMesh()`: Edit Mode의 저장된 C02 씬만 대상으로, 해당 씬의 C02World/V04MeanderingMeadow 내부에서 위 이름의 MeshFilter만 갱신한다. 이미 같은 메시이면 생략한다. 공유 MeshRenderer·재질·Transform·Collider에는 쓰지 않는다. MeshFilter 교체는 한 Undo 그룹이며, 전용 mesh만 SaveAssetIfDirty 후 변경 시 씬을 명시 저장한다. 기존 미저장 씬이 있으면 먼저 저장하도록 거절해 다른 편집을 함께 저장하지 않는다.

기존 공용 `Rock()`는 성문·돌 블록에도 쓰이므로 수정하지 않았다. LowShrineRemnant, MossyVergeRock, 성문, 주변 나무·잔디, 카메라·조명·지면 및 서버 경계·충돌·스폰·이동 로직도 그대로다. 자산의 배치 위치와 개수는 바꾸지 않는다.

## 인계와 최소 확인

Coordinator가 기존 입력 배치를 끝내고 파일 소유권을 넘겨받은 뒤 후보 C#를 실제 Editor 프로젝트에 통합한다. 컴파일 후 저장된 C02 씬에서 해당 메뉴만 실행하고 생성 메시의 bounds/normal과 변경 대상 개수를 확인한다. 같은 카메라의 기존/변경 Editor 화면 한 쌍으로 판 같은 윤곽이 줄었는지 확인한다. 플레이 입력/로그인/게임 회귀는 이 시각 변경 때문에 반복하지 않는다. 시각 확인 전에는 개선 성공 또는 실제 게임 검증으로 기록하지 않는다.

첫 실행에 mesh asset와 meta가 생기며 씬 MeshFilter 참조가 바뀌므로 이후 실제 생성 파일·GUID는 통합 담당이 보존한다. 코드 작성 담당은 Scene/Material/Prefab/meta/Packages/Editor/서버/Git/Slack을 조작하지 않았다. 기존 asset이 이미 있으면 내용을 덮어쓰지 않으므로 다음 형상 수정이 필요하면 이 helper만 바꿔도 기존 asset에 자동 적용되지는 않는다는 점에 유의한다. Undo는 씬 참조를 복구하며 새 공용 mesh asset 자체는 삭제하지 않는다.

## 실제 통합 결과

명시 unity recompile 0오류/0경고. 기존 C02 씬에서 ApplyMeadowStoneMesh 실행 성공: 대상50개, 144정점/48삼각형, bounds center0/extents(.5,.5,.5), Playfalse/dirtyfalse. 후보 씬 diff는 MeshFilter 참조50개 교체뿐이다. 같은 카메라 Captures/meadow-stones-before.png/after.png를 직접 열어 직사각 판 윤곽이 낮은 비대칭 돌로 바뀐 것을 확인했다. 실제 플레이/사냥·충돌 검증이 아니라 Editor 정적 비교이며 기존 재질/배치와 주변 높이를 유지한다. 이전 지면 detail 미채택 상태 유지.

별도 읽기 리뷰에서 대상제한/asset재사용/Undo/저장/winding은 확인했다. 리뷰의 'ApplyV04 호출/메뉴 없음으로 실행 불가'는 기존 버전관리된 V04 필드에 대한 실제 메뉴 실행50개 성공으로 반증되어 blocker로 채택하지 않았다. 처음 V04를 새로 만드는 Build 경로의 V03/V04 일관성은 이번 자연석 교체와 별도다.

루트 Editor의 ContinuousMeadow/ContinuousGround hash는 후보 원본과 일치하지 않아 기존 root WIP로 보존했으며 해당 자산을 후보로 덮어 복사하지 않았다. 위 before/after는 같은 기존 Editor 자산 조건에서 메시 차이만 본 결과다. 이 차이를 동기화 또는 게임 전체 동일성으로 보고하지 않는다. 공용 Material 해시는 후보와 같다.

이번 변경은 후속 아트 묶음 일부이므로 작은 수정만 별도 공개 카드/PR로 만들지 않는다. 대표 실게임 캡처는 기존 HUD/필드 이력을 유지하며 자연석 비교 PNG는 내부 근거로 보존했다.
