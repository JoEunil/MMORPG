# 공개 포트폴리오 독립 리뷰

2026-10-19 최종 동결본. 결론: **코드상 차단 문제 없음**. 실제 브라우저·배포 검증을 대신하지 않는다.

## 범위와 근거

- 루트 AGENTS, reviewer 역할, PORTFOLIO_GUIDE/TASK에 따라 SHOWCASE·작업별 MD, HTML 생성기, CSS, 미리보기/ZIP 도구, Pages workflow와 ignore를 읽었다. 제품·콘텐츠 파일은 수정하지 않았다.
- 원본 체크아웃의 C08_RESULTS/C09_RESULTS와 공개 설명을 대조했다. 구역 이동·ACK 길이 검사 수정·거래소 조회와 실제 입력에서 발견한 버튼 겹침 사례는 근거와 일치한다. 사용자 감상·만족도·생산성·학습을 창작한 서술은 없다.
- 홈은 실제 필드 화면을 명시적으로 선택하며 작업 파일 정렬 순서에 의존하지 않는다. 홈 본문·참고 링크는 SHOWCASE에서 생성하고 작업 상세·날짜별 화면은 작업별 MD에서 생성한다. 공개 설명에 내부 작업 ID·PR/승인/병합 상태·테스트 숫자 나열은 없다. 기존 서버/PPT 링크는 README의 실제 주소와 일치한다.
- 홈과 두 작업 상세·두 화면 이력의 다섯 페이지 범위다. 실제 조회 UI는 중간 결과로 명시하고 등록·구매를 완료 기능으로 주장하지 않는다. 이미지 원본 확대 링크와 앞뒤 탐색이 있다.
- 프로젝트 기본 경로 `/MMORPG/`, 내부 링크·앵커/이미지의 파일 경계 확인, 미리보기의 site 경계, ZIP의 site 범위를 확인했다. PR은 build/artifact이며 deploy는 master ref의 비-PR 이벤트에만 실행한다. 추가 프레임워크·외부 서비스·게임 변경은 없다.

## 지적과 영향 재검토

1. **P2 — 이력 이미지 가로 넘침:** history 본문이 이미지 폭 규칙의 `.content-section` 밖에 있어 1280px 원본이 작은 화면을 넘었다. 구현 담당이 기존 wrapper를 재사용했다. 두 생성 history HTML과 CSS의 `width:100%;height:auto` 적용 경로를 재확인했다. 실제 모바일 폭은 Coordinator 배치에서 확인한다.
2. **P2 — 날짜별 이미지 Git 누락:** 전역 `[Rr]eleases/`가 선별한 새 미디어 디렉터리도 무시했다. 구현 담당이 `/site/media/releases/`와 하위 경로 예외를 추가했다. 예외 범위를 읽어 확인했으며 실제 네 파일의 추적/커밋 포함은 Coordinator가 최종 확인한다.

두 지적은 코드상 해결됐다. 취향이나 추측상의 확장을 이유로 추가 구조·반복 테스트를 요구하지 않는다.

## 동결 식별

SHA256:

| 파일 | 해시 |
| --- | --- |
| tools/site/build.py | C681D462C14D16EF77195481052F0947B573151880345C6635FFE169372034FE |
| tools/site/preview.py | BA10880DA47593A36196CBAB67EAC2C0A1B2F4274DC40D45F3D9C5F0538DFD40 |
| site/style.css | 633F68A228FAA92E0BBE6E98A1B56F862F5A02FB71B9048D211677F699A0C48B |
| .github/workflows/pages.yml | 4F23626D5E7C27CBF056BBAC316A1476C9DFBC36B3CD49BCE4766BD52A78EC0A |
| .gitignore | 36E23E40BEBD8B6E3E9472111CB68970FC29A71D9394D24411C483F8E3C5E8A9 |
| SHOWCASE.md | 9F82C65B57FB5398E2CBE1AC04FF3C1B291C56DBFB89FC24F1E18BF8389D13D2 |
| portfolio/bazaar-search.md | EA0ADF34EF1F1A472C50F1D182AA81F29472B1F31F87D56A4C0C287AFA83AB60 |
| portfolio/field-travel.md | 1A356E4D0A0F1383DDC2214337AA0CB9EB5E8A4571C5F349B8CD1686324FFAB8 |

## 남은 검증

이 리뷰에서는 실행 시험·브라우저·서버·Git·Slack을 조작하지 않았다. 구현 담당은 생성/링크 확인 성공을 보고했으며 직접 실행 결과로 재분류하지 않는다. Coordinator는 최종 배치에서 1440·768·390 화면의 홈/상세/이력 탐색, 이미지 확대·가로 넘침·표시 크기, 공개 ZIP 내용, 선별 PNG 네 파일의 추적과 원본 대응을 확인한다. PR/배포 성공은 아직 이 문서의 결론에 포함하지 않는다.

## 추가 지시 영향 리뷰 — 공개 명칭·워크플로우·기존 구현

617ddc7 이후 사용자 추가 지시에 따른 동결본을 읽었다. **코드상 추가 차단 문제 없음**. 변경된 SHOWCASE, 생성기·CSS, 홈과 네 하위 HTML, 기존 시연 참조를 영향 범위로 확인했다. 기존 검토 전체를 반복 실행하거나 게임 테스트를 요구하지 않는다.

- 공개 명칭은 SHOWCASE H1 `AI-AGENT Unity 클라이언트 구현 작업`에서 홈 제목·브랜드·푸터·하위 문서 제목으로 전달한다. 생성기에 별도 제목 본문을 중복 저장하지 않는다.
- `AI Agent 워크플로우`는 사용자 목표·판단, 총괄 배정, 분리된 구현/리뷰, 묶음 실검증·문제 수정, 사용자 최종 확인을 짧게 설명한다. 팀 분위기의 추상적 설명을 추가하지 않았으며 현재 운영 지침과 일치한다. Markdown의 문단 경계도 생성 HTML에서 유지된다.
- `기존 Unity 구현`은 SHOWCASE 원본에서 렌더링한다. 현재 AI Agent 작업 전 직접 구현한 화면이라는 본문·캡션으로 구별하고, Skill.md의 실제 `3. 시연` 및 `images/BossMonster.gif` 참조와 연결한다. 최근 AI 구현 성과로 주장하거나 과거 기능 전체가 현재 버전에서 지원된다고 주장하지 않는다.
- 기존 화면은 원본 비율을 유지하는 `width:100%;height:auto;object-fit:contain`과 원본 PNG 확대 링크를 사용한다. 신규 PNG는 130369 bytes이며 GIF 자체를 공개 site에 복제하는 경로가 없다. PNG 추출 프레임·원본 대응 검증과 실제 시각 검수는 Coordinator가 수행한 근거로 구분하며 이 리뷰에서 직접 실행하지 않았다.

아래 해시는 앞선 표의 해당 파일을 대체한다.

| 파일 | SHA256 |
| --- | --- |
| tools/site/build.py | 8A77436A981F4D663E2A01669DCBF52A2BB59FC1DA61E4FC2349C76DC4B24CAD |
| site/style.css | F39BA3E409A35FFD1F8A054E6C0EDE11CACC3697326000C1EC1ADD5066153493 |
| SHOWCASE.md | 6A08ECE546D78ABCBB9173A6233BD3BC03E8122DC746BC331D6E764D4FC17C00 |
| site/media/legacy/unity-client.png | A29A2C3BF30891A9E4FB4520D6B41B2143DD9424C0AF5636C8285245CE45E89E |
| site/index.html | C2D7828AB03CD1CD5AD27A46903059650C39C845AE53FAC2B124241345039976 |

남은 영향 확인은 긴 사이트명으로 바뀐 헤더의 데스크톱·태블릿·모바일 배치, 기존 화면 크기/확대/출처 링크, 변경된 본문 표시와 공개 artifact의 PNG 포함 여부다. Coordinator 실제 브라우저·웹 배치 결과로 완료 여부를 확정한다.

## 마지막 탐색 보완 영향 리뷰

Coordinator가 적용한 헤더 탐색만 읽어 재확인했다. `작업 결과 / AI 워크플로우 / 기존 Unity`는 모든 하위 페이지에서 `/MMORPG/#work`, `#workflow`, `#legacy`로 홈의 실제 ID를 가리킨다. 기존 서버/PPT 외부 링크는 푸터에 유지되고 중복된 `ref_nav` 저장은 제거됐다. 현재 SHOWCASE에 두 섹션이 존재하며 생성 홈에 해당 ID가 각각 연결된다. **코드상 추가 차단 문제 없음**. 실제 앵커 클릭과 헤더 폭은 Coordinator 영향 배치에서 확인한다.

최종 동결 해시 대체: `tools/site/build.py` = `B9BBAC57030A6183915D9D86216DA2372953528B659EE32295141235A8EE71DD`, `site/index.html` = `49BFFBB770E6C27FBEB01F1667D50B906DDDA603E96C1153675CAF15DA7C3627`. CSS·SHOWCASE·legacy PNG의 직전 검토 해시는 유지한다.
