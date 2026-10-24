# Agent 구성과 도구 적용

정책은 [AGENTS.md](../../AGENTS.md), 현재 상태는 [HANDOFF](HANDOFF.md)다. 문서 권장·설정 파일·실제 실행을 구분한다.

## 역할 설정

| 공개 역할 | 실행 역할 | .codex/agents 파일 |
| --- | --- | --- |
| 기획 Agent | game_planner | planner.toml |
| 서버 분석 Agent | server_analyst | server_analyst.toml |
| 구현 Agent | client_implementer | implementer.toml |
| 아트 Agent | asset_artist | asset_artist.toml |
| 리뷰 Agent | code_reviewer | reviewer.toml |
| 테스트 Agent | unity_tester | unity_tester.toml |
| 기록 Agent | progress_documenter | documenter.toml |

책임은 roles/*.md에서 관리한다. 코드 개선은 기존 담당자의 업무이며 별도 프로필을 추가하지 않는다.

모델·추론은 실제 TOML과 배정 도구가 반환한 역할 정의를 대조한다. 감사 시 기획/구현/테스트/기록은 Luna, 서버 분석/아트/리뷰는 6.1 Sol 계열이었다. 관찰값이며 미래 모델을 강제하지 않는다. 총괄 설정과 역할 설정을 혼동하지 않는다.

사용자 요청에 따라 프로젝트 max_concurrent_threads_per_session=2를 제거한다. 실제 병렬 실행은 플랫폼 슬롯과 파일·Editor 소유권에 따른다. 설정 편집만으로 현재 세션 재로딩·플랫폼 슬롯 확대를 주장하지 않는다.

## 도구와 권한

- 파일·코드는 검색/읽기·Git diff부터 확인한다.
- Unity는 관련 스킬과 설치된 CLI/MCP에서 프로젝트를 지정한다. 상태 연결과 명령 성공을 구분한다.
- 서버·ClientCore 요청/응답을 직접 추적하고 옛 문서를 현재 계약으로 단정하지 않는다.
- GitHub·Slack 맥락은 연결 도구로 읽고 Slack 발신은 SLACK_WORKFLOW의 앱 도구만 사용한다.
- 실제 입력은 현재 허용·화면을 확인한다. 다른 게임이 앞에 있거나 도구가 중단되면 입력을 멈추고 허용을 확인하며 독립 작업은 계속한다.
- 패키지·외부 도구·비용은 실제 필요와 기존 범위를 확인한다. 권한·보안을 임의 변경하지 않는다.

프로젝트 config는 기본값이며 관리 정책·세션 재정의·도구 권한이 우선할 수 있다. 글로벌 config의 비밀·환경 전체를 출력하지 않는다. trusted/MCP 설정만으로 보호 경로 쓰기·네트워크·실입력 허용을 단정하지 않는다.

과거 P01은 기록이며 새 권한을 부여하지 않는다. 기존 승인 범위의 서버·맵·프로토콜 후보는 구현 가능하되 새 계약·운영·DB 파괴·비밀·비용은 공통 승인 경계를 따른다.

AGENT_USAGE에 실제 생성·재활성화·역할·산출물과 확인 가능한 모델/토큰만 남긴다. 전체·역할 합계를 대조하고 미노출 토큰은 N/A다. 호출 수를 늘리지 않는다.

초기 구축·P01·옛 모델 지침은 [원문 이력](history/2026-10-24-guideline-audit/README.md)에 보존한다.
