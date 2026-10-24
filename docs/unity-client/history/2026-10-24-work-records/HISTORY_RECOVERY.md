# 기본 브랜치 이력 복구

사용자가 2026-10-20 명시적으로 원격 복구와 완료 브랜치 정리를 요청했다. 이번 복구 게시만 해당 요청을 직접 승인으로 사용하며 일반 신규 기능의 PR 직전 Slack 승인 규칙을 바꾸지 않는다.

- 기존 원격 f1b5e44 문서 변경과 PR37 병합 0f9b96d를 모두 조상으로 보존한다. 이미 검증한 후속 리뷰 수정 4cc541f도 포함한다. reset/강제 push 없이 일반 fast-forward push로 master를 복구한다. push 직전 원격 head가 움직였으면 덮어쓰지 않고 다시 병합·확인한다.
- 충돌은 소개·운영 문서와 ignore 규칙이다. 사용자 AI 경험/PR 자동 리뷰 설명을 보존하며 이미 공개한 Pages 안내와 최신 최소 테스트·Slack 앱·기능 묶음 규칙을 유지한다. 루트 원본 Game 및 Game-UnityClient의 기존 미완료 변경은 건드리지 않는다.
- 브랜치 삭제는 GitHub PR 병합 확인 + 현재 복구 이력에 head 전체 포함 + 추가/미게시 커밋 없음 + 열린 PR 없음에 한정한다. 로컬은 체크아웃된 worktree의 브랜치를 보존하고 강제 삭제하지 않는다. dev/docs/claude 등 사용자 장기·외부 작업 브랜치는 범위에서 제외한다.
- 원격 삭제 후보: codex/ai-agent-site, hunting-state, pages-live-link, player-hud-inventory, portfolio-site, site-my-list-results, unity-ai-client. 현재 head와 병합 상태를 직접 확인한 목록이며 실행 전 다시 확인한다.
- 보존: content-catalog, market-*는 미게시 작업이다. unity-c09-delivery, unity-c10-my-list, item-market-ui는 PR 병합 후 추가 이력이 복구 기준에 포함되지 않아 보존한다. 사용 중인 worktree의 로컬 브랜치도 유지한다.
- 게임 코드는 검증한 4cc541f와 동일한지 확인한다. 관련 native1+Unity2 통과 근거를 재사용하고 같은 시험을 반복하지 않는다. 공개 사이트 생성/내부 링크 확인은 6개 상세 페이지 포함 성공했다. 원격 복구 후 head·조상 포함·필수 파일 존재와 Actions 배포 상태를 확인한다.