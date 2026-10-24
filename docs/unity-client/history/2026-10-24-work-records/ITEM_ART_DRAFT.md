# HP Potion 대표 아이콘 초안

2026-10-20 아트 담당 실행. 역할 설정: `gpt-6.1-sol` / `medium`; 실제 토큰: N/A. 내장 `image_gen` 생성 1회, 외부 API/CLI/구매 없음.

- 대상: 기존 서버 아이템 ID 1 / HP Potion (Coordinator 배정 기준).
- 산출물: `UnityClient/Captures/Art-Drafts/item-001-hp-potion-v1.png`.
- 실제 파일: 1254×1254 PNG, 32bpp ARGB, 1,285,019 bytes. 좌상단 alpha 0, 완전 투명 픽셀 971,214개. alpha>8 경계 `(223,82)–(1061,1170)`.
- 출처: 내장 OpenAI 이미지 생성 도구의 신규 생성물. 참조 이미지·기존 게임 이미지·외부 에셋 없음. 원본은 `C:/Users/joeun/.codex/generated_images/01a15838-b1da-7240-9428-6085f65f71e1/exec-c10f9a9f-c6e3-434e-bec9-47a1c7583b7f.png`; 원본을 그대로 복사했고 비트맵 수정 없음.
- 사용 조건 기록: OpenAI 서비스의 생성 출력물이며 별도 제3자 에셋 라이선스 없음. 현 계정 서비스 약관의 상업 이용 세부 조건은 이 작업에서 별도 검증하지 않음. CC0 또는 독점권 보장을 주장하지 않음.

실물 이미지를 `view_image`로 확인했다. 붉은 유리병·청동 마개·가죽끈이 있는 성숙한 판타지 외형이며 문자/프레임/숫자/배경/광휘가 없다. 둥근 몸통과 붉은 액체의 구분은 좋다. 목 부분과 마개는 사실적인 표면 묘사가 강하고, 작은 슬롯에서는 미세 흠집·기포가 사라질 수 있다. 상하 여백은 약 6.5%로 요청한 넉넉한 여백보다 좁다. 재생성은 하지 않았다.

Unity 임포트·Sprite 설정·아이템 연결·실제 32/64px 게임 화면 확인은 **미실행**이다. `Assets`, Scene, Prefab, Material, `.meta`, 프로젝트 설정을 수정하지 않았다. 스타일 확인 전 나머지 4종을 확장하지 않는다. 다음 담당은 스타일 확인 후 동결/테스트 일정을 고려해 별도 임포트하고 ID 1 슬롯에 연결하며, 실제 어두운 슬롯 배경에서 32/64px 축소와 알파 경계를 확인한다.

## 사용한 최종 프롬프트

```text
Use case: stylized-concept
Asset type: production-ready raster inventory item icon, HP Potion for existing server item ID 1, one single asset.
Primary request: an original classic restrained realistic fantasy MMORPG healing potion icon. A compact stout rounded glass potion bottle filled with rich ruby red liquid, short neck, old bronze cap, and a simple worn dark brown leather strap around the neck. Clearly readable strong silhouette at 32–64 pixels. Three-quarter view, clear directional lighting from upper left, subtle edge highlights and material depth, realistic painterly game inventory art, mature medieval fantasy craftsmanship. Bronze and leather details remain quiet; red glass is the focus. Inspired by the general mood of traditional fantasy MMORPG inventories, without copying any existing game's unique artwork.
Scene/backdrop: genuinely transparent alpha background, isolated object, no surface, no backdrop, no environmental shadow.
Composition/framing: square canvas, one centered upright potion bottle, whole object visible with generous even transparent padding, subject fills approximately 76 percent of canvas height. Near frontal three-quarter angle with strong compact shape.
Materials/textures: deep ruby translucent glass with red liquid visibly contained inside, aged bronze cap, simple dark brown leather tie, restrained wear.
Constraints: only one potion bottle, actual icon asset, no mockup, no reference sheet, no text, no numbers, no label, no frame, no border, no grid, no logo, no watermark, no luminous aura, no sparkles, no candy-like glow, no cartoon style, no surrounding UI, no extra objects. Preserve clean transparent edges.
```

Coordinator가 같은 PNG를 UnityClient/ArtSource/Items/item-001-hp-potion-v1.png에 그대로 보존했다. ArtSource는 Assets 밖이며 현재 Sprite 임포트나 게임 화면 연결은 하지 않았다.
