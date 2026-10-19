# V02 독립 정적 리뷰

2026-10-18 KST. 판정: **코드상 차단 문제 없음**. 동결 builder/Scene/Art 파일과 기존 UI·서버 표시 연결을 검토했다. 리뷰어는 Editor/Play/렌더/캡처/테스트를 실행하거나 자산을 수정하지 않았으며 이 문서만 작성했다. 실제 화면과 서버 통합 검증은 총괄의 별도 실행 근거가 필요하다.

## 고정 기준

| 파일 | SHA256 |
| --- | --- |
| Assets/C02/Editor/C02SceneBuilder.cs | `1FE57BD0D940366359C7F1F7ED149BB3549B8A1D5E11913AD011A0FB516A3EA9` |
| Assets/C02/C02.unity | `7868608DB26B627AD5C775D81A05C0BC447C59F8FD09651A4B5DE1EBCE0127A5` |

V02_TASK.md, Art/README.md와 Captures/v02-art-freeze.txt를 함께 읽었다. C02WorldScreen `3226DD...`, C01LoginScreen `B8DB07...`, C01 Scene `523AF6...`, ClientCore DLL `6803B7...`, 기존 실제 UI helper `0BC34D...`는 이전 고정 SHA256 그대로다. V01의 정적 리뷰 기준을 이어 사용했고 새 UI framework나 runtime 코드가 추가되지 않았다.

## 메시·텍스처·재생성

- 기사와 환경은 Profile/CurvedPlate/ClothMesh/Shield/Rock/Pine/Grass/Ridge의 직접적인 시각 geometry로 구성된다. 각 메시의 vertex/triangle/UV를 지정하고 normals/bounds를 재계산해 기존 asset을 Clear/재작성하거나 새 asset을 만든다. 같은 이름의 mesh는 두 view에서 같은 local geometry를 사용하고 field/preview 크기는 Transform scale로만 달라진다. 서로 다른 형상이 같은 cache key를 덮어쓰는 현재 호출은 발견하지 못했다.
- Scene에서 사용하는 V02 authored mesh360개의 저장 vertex/index 데이터를 읽어 index 범위, position/normal/UV의 유한 값과 normal 크기를 확인했다. 해당 사용 mesh에서 문제0이며 최소 normal 크기는 약0.99999986이다. 양쪽 triangle을 같은 vertex/normal에 겹치는 방식 대신 FadedCrimson/PineNeedles/Moss의 _Cull=0을 확인했다. 기존 GuardCapeReverse/SplitTabardReverse 두 asset은 최종 Scene에서 사용하지 않으며 이전 저장물의 normal0을 현재 표시 결함으로 판정하지 않았다.
- Texture2D는 GroundBlend512², StoneGrain128², MailLinks128²의 세 asset이다. 저장 데이터의 mip count10/8/8, trilinear filter, repeat wrap, anisotropy4가 생성 코드·README와 일치한다. geometry UV는 vertex마다 있고 유한 값이다. Chainmail/석재/지면 material의 _BaseMap GUID는 지정한 세 texture에 연결된다.
- DetailTexture의 같은 이름/kind 조합은 같은 크기/픽셀 규칙으로 재작성한다. Material 공유에서 GuardSteel의 metallic/smoothness, chain texture, 석재 tiling, foliage/cloth culling 설정이 두 Hero/Outpost 호출 뒤에도 유지되는 현재 호출 순서를 확인했다. texture/mesh를 별도 runtime cache나 복제 source로 관리하지 않는다.
- 기존 Hero/Outpost/디오라마 및 UI 장식을 정해진 위치에서 교체하고 이름이 고정된 Art asset을 갱신한다. V01의 목록 layout tracker 정리→Canvas 갱신→top stretch 재적용→content rebuild/save 보정도 유지한다. author가 보고한 재적용/저장·재로드/별도 layout 조회는 Edit Mode 근거이며 리뷰어가 재현한 것은 아니다.

## UI·계약·범위

- Scene 내부 fileID 참조를 대조해 누락0을 확인했다. C01LoginScreen/C02WorldScreen의 기존 serialized refs와 선택 Button10개, ScrollRect의 content/viewport 연결이 유지된다. CharacterChoices는 anchorMin(0,1), anchorMax(1,1), pivot(.5,1), sizeDelta.x=0으로 viewport 폭을 따른다. content가 빈 행 상태에서 높이0인 것은 목록 활성 후 VerticalLayoutGroup/ContentSizeFitter가 갱신하는 기존 구조와 구분한다.
- 저장 시 C02World/WorldHud inactive, 선택행10개 inactive, Enter disabled다. 기존 runtime의 목록 ID 선택/중복 Enter 방지/Zone와 matching Full gate/연결 정리는 바뀌지 않았다. 서버 좌표→OwnCharacter root `(x,0,y)`도 유지하며 새 장비 외형은 그 자식이다. FieldPlane/Capsule은 참조를 보존하고 Renderer만 숨긴다.
- 실제 로그인 입력 경로/이름과 기존13개 Button(로그인/Enter/Disconnect/10행)을 유지한다. raycast=true는 실제 입력·기존 Button과 Scroll viewport뿐이고 장식 frame/text/panel은 false다. EventSystem/InputSystem UI module/GraphicRaycaster가 유지되며 로그인/목록/Enter의 기존 runtime 이벤트 배선은 변경되지 않았다. 새 입력 가림 요소를 발견하지 못했다.
- 신규 geometry에는 collider를 만들지 않거나 primitive collider를 제거한다. 저장 Scene collider는 기존 Capsule/FieldPlane뿐이다. 장식 관문/지면/수목이 서버 충돌·맵 규칙을 새로 정의하지 않는다.
- 로그인 디오라마는 별도 위치의 cosmetic preview이고 라벨을 유지한다. 성인 비율/판금·천·자연 지형·UI 재질은 미술 변경이며 실제 캐릭터 목록/ID/HP/재화/가짜 feature 버튼을 만들지 않는다. 서버·ClientCore·runtime·패키지 변경이나 신규 게임 시스템/불필요한 범용 계층은 발견하지 못했다.

## 검증 한계

아트 freeze의 컴파일 오류0·Edit Mode reload/layout 폭304·Play=false, 정적 triangle40512/texture 약3.1MB는 author 인계다. static Editor resource 관찰을 Player memory/FPS/batching 성능 결과로 확대하지 않는다. 같은 OpenScene 호출에서 layout 전 폭0을 본 기록과 이후 안정화/별도 조회의 폭304는 구분하며, 저장 top stretch에는 V01과 같은0폭 anchor 회귀가 없다.

리뷰어는 V02의 실제 로그인→목록 선택→새 AUTH 서버 Enter→Zone/own Full·좌표·화면 표시나 최종 렌더·캡처를 실행하지 않았다. 기존 V01/Editor composition 이미지는 이 실행 통과를 대신하지 않는다. 총괄의 고정 hash 실제 flow와 시각 확인, 새 Console 오류 여부·정상 종료 결과를 별도로 기록해야 한다. 테스트 중에는 동결 자산을 수정하지 않는다.

최신 사용자 지시로 Unity 입력이 있는 테스트는 실제 flow 시작 전에 다시 보류됐다. 총괄은 V02 입력 실행0회라고 전달했다. 위 미검증 항목은 변경별 검증 배치에 남겨 사용자 재개 시 수행하며, 이 정적 리뷰 완료를 실행 재개 허가나 통과로 해석하지 않는다.
