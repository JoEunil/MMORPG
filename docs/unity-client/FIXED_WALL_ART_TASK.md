# 고정 필드 석축 외관

기준 codex/zone-spawn-authoring=16b5c50. 실제 입력은 보류하고 확대 고정 맵의 한 구간을 Editor preview scene에서 비교한다. 기존 충돌 벽은 단색2×2.7×10/10×2.7×2 Cube여서 블록아웃 느낌이 강함을 wall-before.png에서 확인했다. 전체 맵/게임플레이 화면 완료를 목표로 확대하지 않는다.

## 범위와 소유권

- fixed_wall_art_direction: 실제 baseline 이미지·기존 자산을 읽고 방향 검토만. 5단 엇갈림 석축, WeatheredLimestone 돌/안으로 들어간 FoundationStone 코어의 두 재질 권장.
- fixed_wall_visuals: 신규 C21FixedWallVisuals.cs/.meta만. 기존 V02_WallStone-1_0_0.asset 메시를 작은 돌 템플릿으로 재사용해 장축 X/Z 두 결합 메시를 만드는 고정 Editor 작업. 기존 바위용 MeadowChamferedStone은 뾰족한 정상부 때문에 제외.
- Coordinator: Editor 조작/실제 새 메시 자산·선별 Scene 참조 동기화/비교/문서/Git. 원래 BoxCollider18개와 transform/통행 배열은 유지한다.
- 독립 리뷰: 정점이 기존 Collider 안에 있는지·사전 guard·멱등성·새 메시/재질 참조만 바꾸는지 확인한다.

검증은 구현/독립 리뷰 동결 후 동일 preview camera/조명의 전후 한 구간과 구조/재적용 결과를 한 번에 확인한다. 새 로그인/서버/DB/AI/전체 회귀를 실행하지 않는다. Scene 전체 복사를 금지하고 후보의 기존 데이터·컴포넌트는 보존한다. 실제 스타일 개선이 보이지 않으면 후보를 채택하지 않는다. 새 이미지 생성·구매·리깅 없음.

기존 두 재질·소스 돌 메시·StoneGrain의 ROOT/CAND 정규화 원문 일치를 확인한다. 비교는 실제 고정 맵 복제본이며 별도 preview scene/고정 카메라·조명을 사용한다. 실제 플레이 조명·입력 검증이나 전체 필드 품질 확인을 대신하지 않는다.
