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
