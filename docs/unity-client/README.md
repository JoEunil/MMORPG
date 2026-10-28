# Unity 클라이언트 문서 안내

실행 규칙, 현재 상태, 기술 계약, 과거 기록을 구분한다. 모든 문서를 순서대로 읽지 말고 작업에 필요한 문서만 연다.

| 구분 | 읽을 문서 |
| --- | --- |
| 공통 실행 지침 | [AGENTS](../../AGENTS.md) · [개발 기준](DEVELOPMENT_GUIDELINES.md) |
| 현재 상태·미확인·승인 | [HANDOFF](HANDOFF.md) |
| 작업 절차·기획/아트 선택 | [WORKFLOW](WORKFLOW.md) · [역할 지침](roles/) |
| 게임 방향·범위 | [BRIEF](BRIEF.md) · [개선 순서](MMORPG_REVISION_PLAN.md) |
| 도구·로컬 환경 | [PIPELINE](PIPELINE_GUIDE.md) · [로컬 준비](LOCAL_PREPARATION.md) · [MCP 기능](MCP_CAPABILITIES.md) |
| Slack·Git 전달 | [Slack](SLACK_WORKFLOW.md) · [전달 기준](DELIVERY.md) |
| 공개 결과·지표 | [SHOWCASE](SHOWCASE.md) · [공개 기준](PORTFOLIO_GUIDE.md) · [Agent 집계](AGENT_USAGE.md) |
| 서버·맵 계약 | [서버 계약](SERVER_CONTRACT.md) · [데이터 공유](CONTENT_PIPELINE.md) · [맵 내보내기](MAP_EXPORT_GUIDE.md) · [서버 연결](WORLD_MAP_HANDOFF.md) |
| 과거 기록 | [작업·리뷰·검증](history/2026-10-24-work-records/README.md) · [이전 지침 원문](history/2026-10-24-guideline-audit/README.md) |

## 계약·저작 참고 문서

루트의 `*_CONTRACT.md`, `C03_SPEC.md`, `ITEM_ART_SET.md`, `SKILL_ART_SET.md`는 구현·저작 참고용으로 유지한다. 작성 당시 계약과 현재 실행 버전이 같다고 가정하지 않고 실제 소스와 대조한다. 계약에 섞인 옛 승인·대기·테스트 상태는 현재 지시가 아니다. 계약 변경 작업이 생겼을 때 해당 범위만 갱신한다.

## 기록 정리 원칙

완료된 작업 카드·리뷰·검증 결과와 당시 운영 기록은 `history/`의 해당 묶음에 둔다. 미확인·보류 결과도 완료로 바꾸지 않고 보존한다. 과거 인계·승인을 자동 재생하지 않는다. 현 상태는 HANDOFF 한 곳, 공통 규칙은 AGENTS/WORKFLOW/roles 한 곳에서 유지한다. 이전 기록으로 가는 링크를 남기고 완료 보고를 루트 문서에 다시 누적하지 않는다.
