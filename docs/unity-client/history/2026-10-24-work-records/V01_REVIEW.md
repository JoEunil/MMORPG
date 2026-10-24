# V01 디자인 정적 리뷰

2026-10-18 KST. 최종 판정: **코드상 차단 문제 없음**. 최초 발견한 P2 저장된 캐릭터 목록 content 폭0은 아래 최종 delta에서 해소됐다. 사용자 실행 보류를 유지해 Unity Editor/Play/렌더/캡처/테스트를 실행하지 않았다. 리뷰어는 파일을 읽고 이 문서만 작성했으며 자산을 수정하거나 다른 작업자의 변경을 되돌리지 않았다. Unity ui→ui-ugui와 ScrollView 계층 기준을 적용했다.

## 최초 고정 기준

| 파일 | SHA256 |
| --- | --- |
| Assets/C02/Editor/C02SceneBuilder.cs | `8C9081E2F355C6297D9C6AE0A932FB75BB7899AF1D25312967F0C4A8AD21BAE9` |
| Assets/C02/C02.unity | `6888E694C094A1AA151CA40B053E6E1D6B90FF2D773AF980E908948E1059A929` |

Art/README.md, 신규 URP 재질/ArmorTaper8·PineCone8 mesh, Scene 직렬화 구조와 builder를 검토했다. 최초 검토 당시 Captures/v01-static-freeze.txt의 builder/Scene 및 보존 runtime/C01/DLL 해시가 파일과 일치했다. Assets/C02/Editor/BuildProvenance.txt는 pre-V01 이력이므로 이번 최종 근거로 대체하지 않았다.

## 최초 지적 — 최종 delta에서 해소

**P2 — C02.unity:16802, CharacterChoices RectTransform(&821196738).** 서버가 실제 목록을 반환해 행을 활성화하면 저장된 content의 anchorMin=(0,0), anchorMax=(0,0), sizeDelta=(0,0) 때문에 폭이0이다. 부모는 CharacterScroll/Viewport이며 Viewport에는 Mask/Image만 있고, content의 ContentSizeFitter는 HorizontalFit=0/VerticalFit=2다. 폭을 viewport에 맞추는 다른 layout 주체가 없으므로 VerticalLayoutGroup이 행도 폭0으로 배치해 정상 캐릭터 선택을 막는다. top pivot과 bottom anchor 조합도 builder의 의도와 다르다.

Builder:167은 content를 top horizontal stretch인 (0,1)→(1,1)로 설정하지만 저장 Scene은 (0,0)→(0,0)이라 재생성/저장 결과가 일치하지 않는다. 최소 수정은 이전 Results layout의 driven 상태 해제/레이아웃 안정화 뒤 content의 top stretch·pivot·offset을 다시 적용하고 Editor API로 저장하는 것이다. builder 재생성 후에도 같은 값이 유지돼야 한다. 전체 UI 재작성이나 runtime 보정 계층은 필요하지 않다. 총괄에 전달했으며 수정은 아트 담당 소유다. 이 결함은 파일 근거이고 실제 화면 재현은 실행하지 않았다.

## 나머지 확인

- C01Client/Canvas/C01Panel/SignIn의 로그인 입력·버튼 및 Results의 StatusText/EnterButton/EntryStatusText 경로를 유지한다. CharacterChoices만 ScrollRect/Viewport 아래로 들어갔으며 C02WorldScreen은 기존 직렬화된10개 Button 배열을 사용한다. runtime/C01 소스와 단일 DLL은 이전 해시 그대로다.
- Scene의 C01LoginScreen/C02WorldScreen serialized refs와10개 선택 Button은 존재하며 모든 Scene 내부 fileID 참조를 대조해 누락0을 확인했다. EventSystem과 InputSystem UI module은 각1개, Canvas GraphicRaycaster도 유지한다. runtime OnEnable/OnDisable 이벤트 배선은 변하지 않았다.
- C02World와 WorldHud는 저장 시 inactive, Enter는 interactable=false,10개 행은 inactive다. 실제 목록 ID 선택→Enter→Zone와 일치 Full의 기존 gate는 그대로다. OwnCharacter의 ground pivot을 유지하고 Capsule Renderer만 숨겨 새 시각 Hero를 자식으로 둔다. 서버 좌표/ID/HUD 문자열을 조작하는 새 runtime 기능은 없다.
- 장식 프레임/브랜딩/텍스트/패널은 raycast=false이며 실제 입력/13개 기존 Button의 target Image는 true다. 새 ScrollRect viewport Image만 마스크/스크롤용 raycast=true다. 패널의 LayoutGroup와 ignoreLayout 프레임을 구분한다. 위 content 결함 외에 정적 계층에서 입력을 가로막는 새 장식 target은 발견하지 못했다.
- 로그인 디오라마는 (1000,0,1000)의 별도 시각 배경이며 CHARACTER & ENVIRONMENT PREVIEW 라벨을 둔다. 실제 필드는 별도 gate 아래의 카메라/주인공을 사용한다. SUNWARD 문구·장비/환경 형태는 미술 표현이며 서버 목록/재화/HP·신규 기능 버튼을 만들지 않는다.
- 신규 장식 primitive의 collider는 제거되며 저장 Scene의 collider는 기존 Capsule/FieldPlane뿐이다. 두 mesh는 builder의 Mesh API와 원본 삼각형/법선·bounds 생성으로 구성되고, 재질은 texture 없는 URP/Lit이다. README는 WarmSkin 미사용과 외부/유료 에셋 미사용, 기존 폰트 재사용을 구분한다. 범용 게임 시스템이나 가짜 서버 데이터/예비 계층은 없다.

미검증: 최종 MMORPG 리비전의 시각 품질·가독성·카메라 구도·실제 UI raycast/scroll·서버 로그인/선택/진입 회귀와 최종 캡처는 사용자 재개 전 미실행이다. 과거 초기 방향 PNG나 pre-V01 C02 실행 통과가 이번 리비전의 통과 근거가 되지 않는다. 새 동결본에서는 지적한 content 구조와 영향받는 builder/Scene delta만 정적으로 재검토한다.

## P2 수정 delta 최종 확인

| 파일 | 최종 SHA256 |
| --- | --- |
| Assets/C02/Editor/C02SceneBuilder.cs | `A29EFD042076BD045C845D48DC206A2F8B1D2FAC50C5620592652F0706990133` |
| Assets/C02/C02.unity | `CBEE9555258D349AC5897E731EBB63388887E350C2435FF8485DA6B6698324BE` |

**P2 코드/저장 데이터에서 해소, 이 delta에 차단 문제 없음**. builder:241 부근은 이전 Results layout을 rebuild하고 Canvas 갱신한 뒤 content의 top horizontal stretch/pivot/offset을 다시 적용한다. content layout을 rebuild하고 dirty/save해 새 runtime 보정 계층 없이 저장 상태를 고친다. 기존 ScrollRect content/viewport와 C02 runtime 직렬화 참조는 유지한다.

최종 Scene:17123~17127의 동일 RectTransform(&821196738)은 anchorMin=(0,1), anchorMax=(1,1), anchoredPosition=(0,0), sizeDelta.x=0, pivot=(.5,1)이다. 따라서 폭은0 고정이 아니라 viewport 폭을 따른다. 내부 fileID 참조를 다시 대조해 누락0을 확인했다. 최종 Captures/v01-static-freeze.txt의 두 해시와도 일치한다.

아트 담당은 적용/저장→재로드→재적용/저장→재로드 후 contentWidth=viewportWidth=304, dirty=false, Play=false를 확인했다고 보고했다. 이는 Edit Mode 저작 확인이며 리뷰어가 Editor로 재실행한 결과는 아니다. 최종 리비전의 Play·실제 입력/서버 회귀·렌더·캡처·디자인 시각 확인은 여전히 미실행이고 사용자 재개를 기다린다.
