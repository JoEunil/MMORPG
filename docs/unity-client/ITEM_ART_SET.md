# 기존 아이템 4종 원본 생성 기록

2026-10-20 아트 담당. 역할 설정 gpt-6.1-sol / medium, 생성 모델 세부 버전은 도구에서 미노출, 실제 토큰 N/A. 내장 image_gen 4회(MP 편집 1회 + 독립 신규 생성 3회), 재생성 0회. 외부 API/CLI/구매/다운로드/비트맵 편집 없음.

## 저장 위치와 출처

실제 후보 루트: `UnityClient/Builds/C10-Delivery/source/`. 아래 경로는 후보 루트 기준이다. 루트 프로젝트의 Assets/Scene/Prefab/Material/Resources/ProjectSettings, Editor, Git, Slack은 수정·실행하지 않았다.

| 서버 ID | 원본 PNG | SHA256 |
| --- | --- | --- |
| 2 | UnityClient/ArtSource/Items/item-002-mp-potion-v1.png | 2FBF7A7CCF66E4D2B0CF3264FE696E74F0CB96C520D3557E4B6E1ADA4A7FA3B0 |
| 3 | UnityClient/ArtSource/Items/item-003-iron-sword-v1.png | 15F82F45D35894DE912D825AD4699346BCC3AF46A6BB11E39552CE2E3DABAB6D |
| 4 | UnityClient/ArtSource/Items/item-004-iron-shield-v1.png | BF1D1D748883AF775FD7E1EC3F947B4B4DFC149039602811CBCFAE2300A79A2A |
| 5 | UnityClient/ArtSource/Items/item-005-leather-armor-v1.png | 9EC34F4A89EC982FF52C0172519796943806A29C3900726C0ADAFA35F39A8668 |

모든 생성 원본은 `C:/Users/joeun/.codex/generated_images/01a15870-00c3-7f43-bb66-8b3c8d21cbdb/`에 있으며, 다음 파일을 변경 없이 복사했다.

- MP: `exec-0c6c8f20-0219-477b-9900-9b900f4ff4ed.png`
- 철검: `exec-bfdf73d8-3216-42dc-af7a-3793c932c037.png`
- 철방패: `exec-d687f0bb-ce8e-4e7d-8c5b-f0031b5e1e02.png`
- 가죽갑옷: `exec-68bbfff2-b6ba-406f-9c87-e0e37726b21f.png`

출처는 내장 OpenAI 이미지 생성 도구의 출력이다. MP는 기존 자체 생성 HP 포션 `UnityClient/ArtSource/Items/item-001-hp-potion-v1.png`를 view_image로 확인 후 참조 편집했다. HP 출처는 ITEM_ART_DRAFT.md에 있다. 나머지 3종은 외부 참조 이미지 없이 독립 생성했다. 제3자 게임 이미지/상표/캐릭터를 입력하지 않았다.

사용 조건: OpenAI 서비스 생성 출력물이며 별도 제3자 에셋 라이선스 없음. 현 계정 약관의 상업 이용 세부 조건은 이번 작업에서 별도 검증하지 않았다. CC0/독점권 보장을 주장하지 않는다.

## 시각 확인과 제약

생성 도구가 표시한 실제 4개 이미지를 확인했다. 모두 단일 중앙 물체, 사실적인 거친 유리/금속/가죽과 좌상단 광원을 따르며 글자·숫자·프레임·로고·그림자판·캐릭터 몸체가 없다. MP는 HP와 같은 병/마개/가죽끈 구조를 유지한 푸른 액체다. 철검은 넓은 날/횡대/갈색 손잡이, 철방패는 뾰족한 하단과 넓은 철제 면, 가죽갑옷은 목/팔 구멍이 보이는 빈 흉갑으로 구분된다. 보석 장식 없음.

모두 transparent_background=true로 요청했고 알파를 보존했다. 요청한 10% 이상 여백보다 실제 상하/철검 모서리 여백은 좁지만 물체가 잘린 모습은 관찰되지 않았다. 미세 흠집과 봉제선은 32px에서 소실될 수 있다. 최종 32/64px 슬롯에서의 가독성·어두운 배경 가장자리·Sprite 임포트·서버 ID 연결은 미실행이며 Coordinator가 확인한다. 축소·색보정·배경 제거·atlas·복제 아이콘 대체는 수행하지 않았다. 이미지 생성 후 Assets를 수정하지 않았다.

원본/복사본 SHA256 4쌍이 각각 일치했다. System.Drawing 읽기 전용 검사로 모든 파일이 1254×1254, Format32bppArgb이며 완전 투명 픽셀이 있음을 확인했다. alpha>8 물체 경계는 아래와 같고 모든 변이 캔버스 안에 있다.

| 파일 | bytes | 완전 투명 픽셀 수 | alpha>8 경계 (x,y) |
| --- | ---: | ---: | --- |
| item-002-mp-potion-v1.png | 1,298,320 | 964,661 | (221,81)–(1064,1172) |
| item-003-iron-sword-v1.png | 633,222 | 1,370,858 | (57,48)–(1218,1208) |
| item-004-iron-shield-v1.png | 1,942,880 | 893,640 | (207,65)–(1061,1177) |
| item-005-leather-armor-v1.png | 1,908,014 | 834,754 | (228,75)–(1039,1186) |

## 사용한 최종 프롬프트

### MP Potion
```text
Use case: precise-object-edit
Asset type: classic realistic fantasy MMORPG inventory icon, existing server item ID 2 MP Potion.
Input image: the provided HP potion is the edit target and exact style reference.
Primary request: Change only the ruby red liquid and red glass coloration to deep clear sapphire blue mana liquid and blue glass coloration. Keep the same stout rounded bottle, shape, bronze cap, worn brown leather neck strap, orientation, realistic material texture, directional upper-left highlights, centered composition and clean edges. A restrained mature classical fantasy game inventory asset readable at 32–64 pixels.
Scene/backdrop: genuinely transparent alpha background, no surface and no environmental shadow.
Constraints: one single whole potion bottle; preserve transparent padding and all non-color bottle details. No text, numbers, label, border, frame, logo, watermark, aura, sparkles, grid, UI, or extra objects. No cartoon style.
```

### Iron Sword
```text
Use case: stylized-concept
Asset type: one production-ready raster inventory icon, existing server item ID 3 Iron Sword.
Primary request: an original restrained realistic classical fantasy MMORPG iron arming sword. Single straight broad double-edged iron blade with simple fuller, practical straight crossguard, short worn dark brown leather wrapped grip, plain iron round pommel. No jewels, no ornate decoration. Realistic painterly game inventory material finish, weathered cool grey forged iron with subtle scratches and restrained edge highlights, upper-left directional lighting. Match the mature tactile bronze/leather/glass quality of a classical realistic potion inventory asset.
Composition: square canvas, whole sword centered on a diagonal from lower-left grip to upper-right blade tip, blade broad enough for a recognizable solid silhouette at 32–64 pixels, subject about 78 percent of canvas diagonal, generous clean transparent margins. Slight three-quarter depth, clear readable blade/guard/grip separation.
Scene/backdrop: genuinely transparent alpha, isolated floating object, no surface, no shadow plane, no environmental or cast shadow.
Constraints: single sword only, no scabbard, no other objects, no text, no numbers, no border, no frame, no grid, no logo, no watermark, no UI, no aura or particles, no cartoon style, no body or hand.
```

### Iron Shield
```text
Use case: stylized-concept
Asset type: one production-ready raster inventory icon, existing server item ID 4 Iron Shield.
Primary request: an original restrained realistic classical fantasy MMORPG iron shield. One practical medieval heater shield with broad rounded upper shoulders tapering to a single lower point. Plain weathered cool grey iron face, simple thick iron rim, a few functional rivets, no crest or decorations or jewels. A small dark brown leather carrying strap can peek subtly from behind one edge, but the recognizable broad iron face is the focus. Realistic painterly game inventory material finish, tactile forged metal and worn brown leather, restrained scratches and edge highlights, directional upper-left lighting. Match the mature realistic material quality of a classical potion inventory asset.
Composition: square canvas, single whole shield upright centered, near frontal slight three-quarter view to reveal thickness. Compact bold shield silhouette, clean readable at 32–64 pixels, entire object occupies about 76 percent of canvas height, at least ten percent clean transparent padding on all sides.
Scene/backdrop: genuinely transparent alpha background, isolated floating object, no surface, no shadow plane, no environmental or cast shadow.
Constraints: one shield only, no body, hand, character, sword, text, numbers, logo, crest, frame, border, grid, watermark, UI, aura, glow, or particles. No cartoon style. Keep all edges fully within the canvas.
```

### Leather Armor
```text
Use case: stylized-concept
Asset type: one production-ready raster inventory icon, existing server item ID 5 Leather Armor.
Primary request: an original restrained realistic classical fantasy MMORPG empty leather cuirass. Single practical brown leather chest armor, sleeveless, open neck and arm holes showing it is hollow, no human body or mannequin inside. Structured tough chest panels in warm medium-dark brown, a few simple functional dark iron buckles and quiet stitched seams, short shoulder straps, modest waist. No gems, no ornate decorations, no fur or cloak. Realistic painterly game inventory material quality with tactile thick worn leather grain and subtle edge abrasion. Match the mature realistic bronze/leather/glass quality of a classical potion inventory asset. Directional upper-left lighting, readable broad material planes.
Composition: square canvas, one whole upright cuirass centered, near frontal slight three-quarter view, strong compact shoulder-chest-waist silhouette readable at 32–64 pixels. Whole object approximately 76 percent canvas height with at least ten percent clean transparent padding on all sides.
Scene/backdrop: genuinely transparent alpha, isolated floating empty armor, no surface, no environmental or cast shadow, no shadow plane.
Constraints: only one piece of empty chest armor; no human, head, skin, face, arms, hands, legs, body, mannequin, skeleton, stand, weapon, helmet, extra objects, text, numbers, logo, watermark, frame, border, grid, UI, aura or particles. No cartoon style. Clean transparent edges; whole armor visible.
```



## Coordinator 연결 확인

기존 HP 포함5종을 모두 실제 Resources Sprite로 연결했다. 원본PNG 바이트는 보존했고 Unity 생성meta의 maxTextureSize256/투명도/단일Sprite로 임포트했다. 직접 영향 가방 UI 시나리오1개 PASS:5종 각각 로드·서로 다른 Sprite·256px 이하·선택상세·빈목록·알수없는ID fallback. 이는 비어 있지 않은 실서버 가방 촬영이나32/64px 비교 검증을 대신하지 않는다. 실제게임수량/재화/장착·사용기능 추가0.
