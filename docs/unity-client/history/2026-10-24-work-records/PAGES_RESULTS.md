# AI-AGENT 사이트 — PR 전 검증

후속 상태: WEB01 승인으로 PR30을 병합해 master146c4f1의 Pages 실행39227574423 build/deploy가 성공했다. 실제 주소 https://joeunil.github.io/MMORPG/ 를 브라우저/HTTP200으로 확인했고 공개5파일이 이 초안의 로컬 검토 산출물과 바이트까지 일치했다. README/AI-AGENT 링크는 PR31로 연결했다. 이 초안은 배포 테스트용이며 사용자 보정 이후의 포트폴리오 전면 개편 결과는 PORTFOLIO_RESULTS.md와 PORTFOLIO_GUIDE.md를 따른다. 아래 미배포/승인 대기 표기는 최초 PR 전 기록이다.

2026-10-19. 기준 master `1e36645060947fadbc1b278b1f9b9ccf2153fe61`, 작업 브랜치 `codex/ai-agent-site`. 사이트/문서/CI만 변경하며 PR29 제품·Unity·게임 서비스·DB는 변경하지 않는다.

## 결과

- 공개 설명 원본 SHOWCASE → Python 표준 라이브러리 변환 → HTML/CSS/실제 PNG 3개. 생성 HTML은 Git 제외다. 프로젝트 목적·8개 역할·6단계 흐름·실제 화면·검증과 한계·다음 작은 작업을 설명한다.
- 독립 정적 리뷰 및 화면 수정 영향 재리뷰 차단 0. 캡처 원본 3개 SHA 일치. 캡처 당시 작업과 전달 커밋을 구별하고 PR29 미병합 상태를 명시했다.
- 첫 브라우저 검사에서 긴 상단 메뉴와 제목 줄바꿈, 캡처 위 설명 겹침을 발견했다. 메뉴 표시를 줄이고 제목·비율·캡션 배치를 수정한 뒤 영향 화면을 다시 확인했다.
- 최종 웹 배치: 1440/768/390px에서 가로 넘침 없음, 이미지 3개 원본 1280px 로드, 목차 7개·역할 8개·흐름 6단계 확인. 브라우저 오류/경고 로그 없음.
- 로컬 HTTP에서 HTML/CSS/PNG 총 5파일 200 및 빌드 바이트 일치. 내부 docs/상위 경로와 인코딩된 상위 경로 404. ZIP은 동일한 공개 5파일만 `/MMORPG/` 하위로 포함한다. 내부 문서·로그·계정·토큰은 배포 artifact에 없다.
- PR Actions는 build/check/미리보기 artifact만, 정식 Pages deploy는 master push 또는 master 수동 실행에만 허용한다. 권한은 기본 contents:read, deploy만 pages:write/id-token:write. 정책·역할 지침에 개발 묶음별 사이트 갱신을 반영했다.

## 미리보기와 재현

Python 3.10 이상에서 저장소 루트 기준 `python tools/site/build.py`, `python tools/site/preview.py --serve --port 8787` 후 http://127.0.0.1:8787/MMORPG/ 를 연다. `python tools/site/preview.py --zip`은 `site-preview.zip`을 만든다. ZIP을 풀고 압축 해제 경로에서 HTTP 서버를 열어 `/MMORPG/`로 확인할 수도 있다. 로컬 주소는 해당 PC에서만 접근된다.

현재 Coordinator의 미리보기/증거: 원본 개발 체크아웃 `UnityClient/Captures/pages-web-check.txt`, `pages-desktop.png`, `pages-mobile.png`, `pages-tablet.png`. 원본은 비공개 로컬 증거이며 웹에 올리지 않는다.

검토된 빌더 SHA256 `2b599b1730f4d58b9d0e3254c56a79e53418f10e922cbaa854af1126b165141d`, CSS `ec39a366c9c62c28dce0fd6d1c257d8edef1efe4c331f1c4888da0af196908ad`, SHOWCASE `a1b5d8215dd8d9c5eaa2e69eeb08247ae1522cb632c85102b77bcc09061c7938`, 생성 HTML `7d4c0e30439d9f2d28b3b23794b51a6cb7fed1da2b31a7449325dad8167f5184`.

## 게시 상태와 후속

사용자가 Source=GitHub Actions 설정을 완료했고 기본 주소의 첫 게시를 요청했다. PR 직전 최종 Slack 승인 절차를 유지한다. 원격 Actions 실행/Pages 첫 배포/실제 HTTPS URL 확인은 아직 수행하지 않았으며 로컬 통과로 대신하지 않는다. 승인한 사이트 PR만 통합·배포하고 게임 PR29를 함께 병합하지 않는다. 실제 URL 확인 뒤 README/AI-AGENT에 정식 링크를 추가한다. 첫 웹 확인 이후 개발 묶음마다 검증된 공개 설명·미디어·HTML 갱신을 이어간다.
