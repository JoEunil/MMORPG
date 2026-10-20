# 정통 판타지 재질 알베도 원본 초안

2026-10-20 아트 담당 재활성화. 역할 gpt-6.1-sol / medium, 생성 모델 세부 버전 미노출, 실제 토큰 N/A. 내장 image_gen 각 1회, 이번 작업 총 2회, 재생성 0회. 이전 아이템 작업 포함 이 아트 담당 누적 호출 6회. transparent_background=false. 외부 API/CLI/구매/다운로드/이미지 편집 없음.

## 산출물·출처·사용 조건

실제 후보 루트는 `UnityClient/Builds/C10-Delivery/source/`다. 아래 경로는 후보 루트 기준이다.

| 파일 | 용도 후보 | 파일 규격 | SHA256 |
| --- | --- | --- | --- |
| UnityClient/ArtSource/Materials/worn-steel-albedo-v1.png | GuardSteel / WornSteelEdge | 1254×1254 PNG, 24bpp RGB, 3,960,782 bytes | 7198005C06E138462E9EBED142A7FAE091E7A07E4CA9F66BDE3F7F931B3CB9A8 |
| UnityClient/ArtSource/Materials/oiled-leather-albedo-v1.png | OiledLeather | 1254×1254 PNG, 24bpp RGB, 3,916,030 bytes | 2BB5A2C5BE25F7B2564EC49000A80A188288EECE7C2397C313C641E67EF384EE |

출처는 내장 OpenAI 이미지 생성 도구의 독립 생성물이다. 입력 참조 이미지/외부 에셋 없음. 기본 저장 위치 `C:/Users/joeun/.codex/generated_images/01a15870-00c3-7f43-bb66-8b3c8d21cbdb/`의 다음 파일을 원본 그대로 복사했으며 원본/복사본 해시 일치를 확인했다.

- 철: `exec-b7c51d0d-cea2-4331-91b6-0c873150716a.png`
- 가죽: `exec-52445a8a-631f-43b8-ae80-03fe63605afe.png`

사용 조건: OpenAI 서비스 생성 출력물이며 별도 제3자 에셋 라이선스 없음. 현 계정 약관의 상업 이용 세부 조건은 이번 작업에서 별도 검증하지 않았다. CC0 또는 독점권 보장을 주장하지 않는다.

## 확인·한계

생성 도구에서 실제 두 이미지를 확인했다. 철은 중성 중간 회색과 미세한 불규칙 결/긁힘, 가죽은 중간~어두운 따뜻한 갈색과 작은 입자/잔주름으로 보인다. 둘 다 물체 형태/글자/프레임/봉제선/버클이 없다. 뚜렷한 한쪽 광원·스펙큘러 하이라이트·그림자·넓은 노출 gradient는 관찰되지 않았다. 다만 생성 이미지이므로 물리적인 완전 무조명 알베도를 측정/보증한 것은 아니다. 철의 밝은 긴 미세 긁힘이 적용 크기에 따라 돌결처럼 읽힐 위험이 있으며 금속성/roughness와 UV 크기를 포함한 실제 재질에서 확인해야 한다. 가죽의 잔주름 밝고 어두운 색 묘사는 셰이더 조명과 중첩될 수 있다.

System.Drawing 읽기 전용 검사: 두 파일 모두 24bpp RGB의 불투명 PNG다. 좌우/상하 맞닿을 가장자리 RGB 절대차 평균(0~255 채널 단위)은 철 12.38/11.30, 가죽 12.38/12.02였다. 비교용 중앙 인접 두 열/행 차이는 철 12.86/13.84, 가죽 10.85/13.17이었다. 경계 차이가 내부 미세 질감 차이보다 두드러지게 크지는 않지만, 가장자리 픽셀이 수학적으로 동일한 완전 주기 함수는 아니다. 이 수치는 타일 시각 통과를 뜻하지 않으며 실제 반복 표시/UV seam 확인은 미실행이다.

알베도 원본 두 장만 제작했다. 노멀/roughness/metallic 맵·새 메시·리깅 생성 없음. 원본 색보정/offset/시밀리스 후처리/축소 없음. Assets/Editor/Scene/Prefab/Material/ProjectSettings/Resources/Git/Slack 작업 없음. Unity 임포트·기존 UV·실제 조명·플레이 화면 적용은 Coordinator 후속 작업이며 완료로 기록하지 않는다.

## 최종 프롬프트

### Worn Steel
```text
Use case: product-mockup
Asset type: seamless tileable square PBR base color / albedo texture only, worn well-maintained forged iron steel for a realistic classical fantasy MMORPG guard armor material. Not a picture of an object.
Primary request: a full-frame flat material scan of neutral medium grey well-maintained forged iron steel. Subtle fine scattered scratches, gently irregular fine forged grain, tiny restrained wear marks. Uniform exposure and neutral color information only; very low-contrast edge details that can repeat smoothly in both directions. Continuous material fills every pixel, square 1:1 composition, straight-on orthographic flat view, no perspective. Seamless tileable all four edges, no visible border, no dominant focal point or large unique mark.
Lighting: completely even diffuse cross-polarized scan-like appearance; NO baked specular reflection, NO highlight, NO shadows, NO light-source gradient, NO directional lighting, NO vignette. Microtexture encoded as subtle color variation only.
Constraints: realistic fine iron/steel material; no stone, concrete, rock, large cracks, dramatic rust, rust blobs, stains, objects, panels, seams, rivets, clothing forms, text, letters, numbers, frame, logo or watermark. No normal map, no roughness map, no multi-panel sheet; deliver a single flat albedo bitmap. Opaque background, no transparency.
```

### Oiled Leather
```text
Use case: product-mockup
Asset type: seamless tileable square PBR base color / albedo texture only, oiled leather for a realistic classical fantasy MMORPG armor material. Not a picture of an object.
Primary request: full-frame flat material scan of medium-to-dark warm brown oiled leather. Fine authentic leather pores and grain, very soft shallow worn micro-creases, restrained slight color variation, supple well-maintained leather. Uniform exposure and warm brown color information only; very low-contrast edge details that repeat smoothly in both directions. Continuous material fills every pixel, square 1:1, straight-on orthographic flat view, no perspective. Seamless tileable on all four edges, no visible border, no dominant focal point or large unique mark.
Lighting: completely even diffuse cross-polarized scan-like appearance; NO baked specular reflections, NO highlight, NO shadows, NO light-source gradient, NO vignette. Surface microtexture encoded as gentle color variation only.
Constraints: no stitching, seam, buckle, rivet, clothing shape, armor shape, large folds, deep wrinkles, cracks, reptile scales, stone, fabric weave, objects, text, letters, numbers, frame, logo or watermark. No normal map, no roughness map, no multi-panel sheet; deliver a single flat albedo bitmap. Opaque background, no transparency.
```


## Coordinator 적용 확인

기존 3재질에만 albedo를 연결했고 512px/mipmap/Repeat로 임포트했다. 철 metallic .72/smoothness .43, 가죽 0/.25, texture scale 2다. 동일 카메라1280×720 전후 PNG에서 표면의 단조로움 감소를 확인했으며 쿼터뷰 이동 중 UV seam 확인은 미실행이다. Builder의 Hero 재실행에도 같은 값이 유지되도록 수정했고 독립 리뷰 차단0/컴파일 오류0을 확인했다. 조명·메시·리깅·normalmap은 변경하지 않았다. 전체 외형 개편 완료를 뜻하지 않는다.
