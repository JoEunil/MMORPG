# 플레이 HUD 독립 리뷰

결론: **코드상 차단 문제 없음**. 아래 P2 수정의 영향 범위를 재검토했다. HUD 소스와 Setup, fixture의 정적 검토이며 Unity 실행·렌더링·입력 검증을 수행하지 않았다.

## 수정 확인

- **P2 — Sprite 없는 Filled Image는 게이지 비율을 그리지 않는다.** `UnityClient/Assets/C02/Editor/C12HudSetup.cs`의 `Gauge`는 새 Image를 생성하면서 `sprite`를 할당하지 않고 `type = Filled`, `fillAmount`만 설정한다. 설치된 uGUI `UnityClient/Library/PackageCache/com.unity.ugui@f3fac7af1578/Runtime/UGUI/UI/Core/Image.cs:884`는 `activeSprite == null`이면 Filled 분기 이전에 `base.OnPopulateMesh`로 반환한다. 처음 Setup을 실행한 Scene에서 두 게이지는 현재값이 0 또는 절반이어도 전체 사각형으로 표시되어 정확한 수치와 어긋난다. 최소 수정은 영속적으로 참조 가능한 유효 Sprite를 fill Image에 지정하거나, 단일 방식으로 RectTransform의 가로 길이를 비율에 맞추는 것이다. 현 fixture는 `fillAmount` 프로퍼티만 확인하므로 이 문제를 탐지하지 못한다. 수정 후 실제 0/절반/최대 렌더 범위를 배치에서 확인해야 한다.

재검토: Setup이 영속적인 내장 `UI/Skin/UISprite.psd`를 조회하고, 없으면 Scene 변경 전 중단하며, 두 track/fill에 명시적으로 할당하도록 수정됐다. Sprite가 있는 Image는 기존 Horizontal/Left Filled 분기에 들어가므로 최초 원인이 제거됐다. fixture는 런타임 Sprite를 생성·할당하고 Dispose에서 정리하며 type/sprite 필수 조건을 확인한다. 이 fixture는 Setup 자체를 실행하지 않으므로 **실제 저장 Scene의 Sprite 참조 및 렌더 검증을 대신하지 않는다**. 해당 실화면 확인은 배치에 남긴다.

## 확인한 범위

- `C05CombatScreen.cs` 변경은 HP/MP 참조 두 개와 비율 반영·초기화에 한정됐다. 기존 Full 수명, Zone/세대 전환, HP0, 로그아웃/disable 정리, 세 스킬 및 가방·거래소·채팅 입력 guard는 변경되지 않았다.
- 현재/최대 텍스트는 서버 값을 유지하고 비율은 최대 0 이하에 0, 나머지는 0~1로 clamp한다. 추가 서버 수치나 쿨다운을 만들지 않았다.
- Setup은 C02 Scene 및 idle/clean 조건을 확인하고 기존 버튼·텍스트를 재사용한다. 이름 기반 track/fill 재사용으로 반복 적용 시 추가 객체를 누적하지 않는다. 기존 FrameOrnaments는 stretch anchor이므로 부모와 버튼 크기 변경에 따라간다.
- 새 범용 추상화나 런타임 중복 상태는 없다. fixture는 실제 World Full/Delta 반영, HP0·로그아웃·재진입, 불완전 수치의 비율을 다루지만 Scene Setup의 실제 렌더 구성은 검증하지 않는다.
- 상태 글자는 396×24에 10pt/한 줄로 배치되며 HP0 안내 포함 실제 잘림, 기존 WorldStatus/Disconnect/Zone·가방·시장·채팅과 겹침, 버튼 raycast 및 클릭 성공은 Coordinator의 1280×720 실화면 검증 대상이다. 정적 검토로 통과를 대신하지 않는다.

## 검토 파일 고정값

| 파일 | SHA256 |
| --- | --- |
| C05CombatScreen.cs | 5FE76A83E9F2582E279E3A0DF429DB7EEFDED809AF83CAF5A036638CE146202C |
| Editor/C12HudSetup.cs | 1ACEE8D8F939D2923DD6FBAF265409544E6CCCE0353410A51EDAF52FECA7DB03 |
| Tests/C02/C12HudTests.cs | 8F5EC817F61373AA7FBA95AB91AA48F5A82BA88364B8167E8E9072360AB5A11A |

두 신규 `.meta`의 guid/MonoImporter 형식도 확인했다. DB 후보·공개 문구·Scene 저작은 이번 리뷰 범위가 아니다. 제품 소스·Scene·테스트를 수정하거나 테스트·Git·Slack 작업을 실행하지 않았다.
