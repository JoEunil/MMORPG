# 필드 지면 표면 후보

2026-10-20. 밝고 자연스러운 정통 판타지 MMORPG의 기존 쿼터뷰 필드에 사용할 저대비 토양·잔디 알베도 원본 1종이다. **원본 준비 완료 / 에디터 비교 후 적용 보류**.

## 통합 비교 결과

Coordinator가 입력 테스트 종료 후 1024px·sRGB·Repeat·Trilinear·mipmap·aniso4로 임포트하고 기존 길 알베도를 보존한 채 detail 슬롯만 반복30·강도0.15로 시험했다. 임시 카메라의 같은 위치·조명으로 ground-detail-before.png/after.png를 촬영했다. 실제 네트워크 플레이 검증 캡처가 아니다. 원본보다 지면이 흐릿해 보이고 잔디 디테일 개선이 부족해 채택하지 않았다. 기존 MeadowAndOldTrail.mat와 C02SceneBuilder 코드를 복구하고 임시 Unity PNG/meta를 제거했다. 원본 ArtSource만 후속 아트 후보로 보존한다. 메시·지형·조명·서버·스폰 변화 없음.

## 산출물과 출처

- 파일: `ArtSource/field-surface/meadow-soil-albedo-v1.png`
- 크기/형식: 1254×1254 RGB PNG, 4,240,247 bytes. 원본 무편집 복사.
- SHA256: `05856C1EE89B6A39A557A111A33E8508366FB3F75EC16EAE3551A139DC3FFA31`
- 생성: OpenAI 내장 `image_gen.imagegen`, 2026-10-20. 외부 게임 이미지·구매 자산·참조 이미지를 사용하지 않았다.
- 생성 도구 원본: `C:/Users/joeun/.codex/generated_images/01a158f7-795e-78c1-881a-3873e0c6938f/exec-15acd6f6-a1b5-49e3-818b-d1fd72b434f8.png`.
- 사용 조건 기록: 프로젝트 요청으로 생성한 AI 이미지이며 사용에는 해당 생성 서비스의 적용 약관을 따른다. 제3자 CC0·독점권·권리 보증을 임의로 부여하지 않는다. 기존 게임 고유 자산을 복제하지 않았다.
- 새 리깅/모델/패키지/셰이더/normal map/금속도 맵은 생성하지 않았다.

## 실제 읽은 연결 지점

- `UnityClient/Assets/C02/Editor/C02SceneBuilder.cs:ApplyV04()`: `C02World/V04MeanderingMeadow/ContinuousWalkableMeadow`가 실제 연속 지면이다. `Assets/C02/Art/V04/ContinuousGround.asset`의 정점은 x,z 각각 [-70,170], y=-0.035, UV는 0..1이다.
- 같은 메서드의 `V04Material("MeadowAndOldTrail", Color.white)`는 URP/Lit 재질 `Assets/C02/Art/V04/MeadowAndOldTrail.mat`를 만든다. 기본 알베도 `ContinuousMeadow.asset`는 240m 전체의 완만한 색 변화와 세 구간의 굽은 길을 이미 포함한다. 이것을 반복 텍스처로 통째 교체하면 길이 사라지므로 유지한다.
- 세부 표면은 `LocalGroundGrain.asset` → `_DetailAlbedoMap`, 반복 15×15(240m/15=16m), `_DetailAlbedoMapScale=.85`, `_DETAIL_SCALED`로 연결된다. 현재 기본 알베도와 작은 입자층이 분리돼 있어 이 경계만 사용하면 된다.
- `C02SceneBuilder.cs:ApplyV03()`의 이전 `V03FieldSurround/Zone1Meadow`, `Outpost()`의 `V01Outpost/TexturedGround`와 `EarthAndGrass.mat`는 이전 외형 경로다. 현재 V04 대상과 혼동하여 함께 덮어쓰지 않는다.
- `UnityClient/Assets/C02/C02WorldScreen.cs:PlaceFieldForZone()`: 구역 원점은 `(((ZoneID-1)%3)*100,0,((ZoneID-1)/3)*100)`, V04/V03에 동일 원점, Outpost에 원점+(10,0,10)을 준다. 지면 표면만 바꾸며 이 이동과 6구역/Cell·스폰·서버 판정은 그대로 둔다.

## 직접 검수

이미지 원본과 작업 폴더의 복사본을 열어 확인했다. 작은 풀잎과 토양이 섞인 차분한 올리브·갈색으로, 큰 바위/사물/장식/길/격자/글자/원근/뚜렷한 방향 그림자가 없다. 단독 반복에서 눈에 띄는 굵은 패턴을 줄이는 방향이다.

PIL로 파일을 읽기만 해서 크기와 가장자리 차이를 확인했다. RGB 평균은 (108.05,99.52,59.68), 맞은편 가장자리 평균 절대차는 좌우18.00/상하18.03(0..255), 내부 인접열/행15.54/15.60이다. 가장자리가 픽셀 단위 일치하는 것은 아니므로 **완전 무봉합을 보증하지 않는다**. 2D 원본 검수이며 실제 반복 이음새·밉맵·플레이 시인성은 통합 후 판단한다.

## 최소 통합 제안 — 입력 배치 종료 후 Coordinator만 실행

1. 원본 보존 후 위 PNG를 지정 Texture 폴더로 복사·임포트한다. sRGB, Repeat, Trilinear, mipmap, aniso4, max size2048 후보. 1254 원본을 임의 리샘플링해서 새 원본이라 부르지 않는다.
2. **현재 ContinuousMeadow 기본 알베도/길/ContinuousGround mesh를 유지**하고 MeadowAndOldTrail의 기존 detail 슬롯 한 곳에서만 비교한다. 컬러 알베도를 URP detail에 곧바로 기존 .85 세기로 넣으면 지면이 어두워지고 노랗게 물든다. `_DetailAlbedoMapScale`은 우선 .15 전후로 낮춰 한 화면에서 확인하며 최종 수치는 실제 쿼터뷰에서 정한다.
3. 반복15(16m)를 기준으로 시작하고 잎이 지나치게 커 보일 때만 30(8m)을 비교한다. 큰 길의 매핑은 바꾸지 않는다. 별도 normal/metallic 추가 없이 기존 smoothness .1을 유지한다.
4. 실제 플레이 카메라 한 화면에서 길의 연속성·반복선·원거리 반짝임·HP/스킬 범위와 개체의 구분만 확인한다. 색/표면 변경 때문에 로그인·거래소·전체 기능 회귀를 반복하지 않는다. 테스트 중 Assets 쓰기는 금지한다.
5. `ApplyV04()`는 실행할 때 절차적 detail을 다시 대입하므로 통합 시 그 한 연결 지점도 유지되도록 수정해야 한다. 그렇지 않으면 다음 재저작에서 새 표면이 사라진다.

이번 담당은 Assets/Packages/ProjectSettings/Scene/Prefab/Material, Unity/서버, Git/Slack을 조작하지 않았다. Coordinator 실입력 검증과 독립적으로 원본 폴더와 이 문서만 작성했다.

## 최종 생성 프롬프트

```text
Use case: photorealistic-natural. Asset type: one square seamless repeating diffuse/albedo terrain texture for a Unity 3D quarter-view classic fantasy MMORPG field. Create only the material surface, an orthographic perfectly overhead flat scan of natural meadow earth, a close even mixture of compact muted warm soil and finely scattered short olive meadow grass. Plausible adult fantasy game naturalism, fine restrained detail, dry soft earth grains and tiny grass blades, no cartoon shapes. Uniform low contrast and brightness across the entire tile, subdued olive/sage green with warm grey-brown soil, no large distinctive patches, no directional gradients. It must tile seamlessly in both axes with matching opposite edges. Flat neutral diffuse illumination, no baked directional lighting, no cast shadows, no ambient occlusion, no perspective or horizon, no height relief silhouette, no shadows under grass. No paths, stones, flowers, tree leaves, footprints, objects, borders, grids, painted tiles, text, watermark, UI, metallic shine, normal map or roughness panels. One material image filling the whole square canvas, suitable for moderately scaled repetition beneath existing broad meadow/trail colour variation. Avoid bright green or saturated yellow; keep ground unobtrusive beneath characters and spell telegraphs. Output one high resolution square albedo texture, opaque background.
```
