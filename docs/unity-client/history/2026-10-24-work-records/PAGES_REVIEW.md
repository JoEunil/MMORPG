# P01 독립 정적 리뷰

2026-10-19, `codex/ai-agent-site`, 기준 master `1e36645060947fadbc1b278b1f9b9ccf2153fe61`.

구현 담당의 동결 통보 뒤 `PAGES_TASK.md`, SHOWCASE, 변환기·미리보기·CSS·Pages workflow·ignore와 지속 갱신 지침을 읽었다. 제품 파일 수정, 빌드·기능 테스트, 브라우저·Unity·서버 조작, Git 게시를 수행하지 않았다.

## 결과

현재 후보에서 재현 가능한 차단 결함은 발견하지 못했다. 아래 정적 수용 범위를 통과했으며, 실제 브라우저와 GitHub 배포 성공을 뜻하지 않는다.

- 공개 사실: C08/C09 결과 문서의 실제 검증 수치·실패 후 영향 범위 재검증·조회 전용 한계와 설명이 맞는다. 로컬 검증 완료, PR29 검토 중, 사이트 게시를 구분하며 미연결 매매를 완료로 표시하지 않는다. 최종 PR29 병합 상태는 게시 직전 Coordinator가 다시 확인한다.
- 캡처: `field.png`는 원본 C08 최종 Zone1, `bazaar.png`는 C09 최종 weapons, `bazaar-empty.png`는 C09 최종 empty-page와 SHA256이 각각 일치한다. 원본 코드/캡처 검증과 이후 통합 전달 커밋을 구분하도록 캡션이 수정됐다.
- 원본 관리: 설명은 SHOWCASE 한 곳에서 생성하고 HTML은 ignore한다. 변환기의 HTML 틀에 기능 결과 본문을 복제하지 않으며 새 웹 프레임워크·DB·대시보드를 추가하지 않았다.
- CI: PR은 build/artifact만 수행한다. deploy는 PR 이외 이벤트와 `refs/heads/master`를 모두 요구하므로 다른 브랜치의 수동 dispatch도 배포하지 않는다. 기본 권한은 contents read, Pages/OIDC 쓰기는 deploy job에만 있다. 업로드 경로는 두 artifact 모두 site이며 내부 문서/실행 로그는 게시하지 않는다.
- 로컬 제공: HTTP는 127.0.0.1에만 bind하고 `/MMORPG/` 하위만 제공한다. URL decode 후 resolve/상위 경계 확인으로 사이트 밖 경로를 거부한다. ZIP도 site 파일만 MMORPG 하위에 넣으며 저장소 전체를 묶지 않는다.
- 화면 구조: lang/viewport, 제목 계층, nav 이름, 실제 이미지 alt, 상대 미디어 경로, 모바일 레이아웃과 reduced-motion 처리가 있다. 실제 너비·키보드·글자 가독성 확인은 Coordinator의 단일 브라우저 배치로 남긴다.
- 운영 지침: 첫 웹 확인 뒤 SHOWCASE → 생성/선별 자료 → 영향 화면 검증 → 승인된 Git 통합 → master 배포/URL 확인을 이어가며, 사이트 변경 때문에 게임 테스트를 반복하지 않도록 관련 역할 지침이 일치한다.

## 검토 중 수정 확인

처음 C08 캡션은 통합 전달 커밋 `6823367`을 직접 검증 버전처럼 표시했다. Coordinator에게 원본 `df172f6`의 실제 캡처와 통합 전달을 구분하도록 요청했고 최종 SHOWCASE에서 반영됨을 읽어 확인했다. C09도 동결 제품의 해시 대조 전달 커밋이라는 설명을 확인했다.

첫 브라우저 확인 후 동결을 다시 열어 수정한 영향 범위도 재검토했다. nav의 짧은 표시명은 기존 H2 기반 목적지 ID를 그대로 사용하므로 링크 대상이 바뀌지 않는다. 제목은 SHOWCASE에서만 줄였고 CSS의 keep-all·최대52px와 너비별 규칙을 적용했다. 대표 이미지는 원본 비율을 유지하며 겹쳐 있던 캡션을 이미지 아래의 일반 흐름으로 옮겼고 덮개 gradient를 제거했다. 명시적 focus-visible 표시도 추가했다. 이 수정에서 정적 차단 결함은 발견하지 못했다. 실제 1440/768/390 너비 결과는 Coordinator의 배치 근거를 따른다.

SHOWCASE의 다음 작업은 내 매물 조회의 계약 조사 완료·구현/실행 전으로 바뀌었다. 원본 작업 폴더의 `C10_CONTRACT.md`와 `C10_TASK.md`가 같은 조사 완료 상태와 쓰기 작업 제외 범위를 명시함을 읽어 확인했으며 C10 구현 성공을 주장하지 않는다.

## 동결 식별과 남은 확인

- build.py SHA256: `2B599B1730F4D58B9D0E3254C56A79E53418F10E922CBAA854AF1126B165141D`
- preview.py SHA256: `1D25B21D617A1234D3498656A4AF1805BED4DCCCC1529078785A205F9D817289`
- style.css SHA256: `EC39A366C9C62C28DCE0FD6D1C257D8EDEF1EFE4C331F1C4888DA0AF196908AD`
- SHOWCASE.md SHA256: `A1B5D8215DD8D9C5EAA2E69EEB08247AE1522CB632C85102B77BCC09061C7938`

Coordinator는 이 동결 후보를 한 배치로 생성·로컬 경계·링크/이미지·ZIP·데스크톱/모바일 실제 브라우저 확인한다. 외부 링크와 PR 상태, 승인된 push/PR/병합 이후 Actions 및 기본 HTTPS URL은 실제 원격 상태로 별도 확인한다. 현재 리뷰는 이 실행 결과를 만들어내지 않는다.
