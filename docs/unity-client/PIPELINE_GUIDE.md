# Agent 구성과 도구 적용

정책은 [AGENTS.md](../../AGENTS.md), 현재 상태는 [HANDOFF](HANDOFF.md)다. 문서 권장·설정 파일·실제 실행을 구분한다.

## 역할 설정

| 공개 역할 | 실행 역할 | .codex/agents 파일 | 기본 모델·추론 |
| --- | --- | --- | --- |
| 기획 Agent | game_planner | planner.toml | gpt-6.1-sol / medium |
| 서버 분석 Agent | server_analyst | server_analyst.toml | gpt-6.1-sol / medium |
| 구현 Agent | client_implementer | implementer.toml | gpt-6.1-sol / medium |
| 아트 Agent | asset_artist | asset_artist.toml | gpt-6.1-sol / high |
| 리뷰 Agent | code_reviewer | reviewer.toml | gpt-6.1-sol / medium |
| 테스트 Agent | unity_tester | unity_tester.toml | gpt-6-luna / medium |
| 기록 Agent | progress_documenter | documenter.toml | gpt-6-luna / low |
| 선택지 Agent | decision_curator | decision_curator.toml | gpt-6.1-sol / medium |

책임은 roles/*.md에서 관리한다. 코드 개선은 기존 담당자의 업무이며 별도 프로필을 추가하지 않는다. 선택지 담당은 기획·아트 제작을 중복하지 않고 비교 묶음과 사용자 답변 연결만 맡는다.

프로젝트 [agents] 기본값은 gpt-6.1-sol/medium이며 반복 테스트·기록은 해당 역할의 Luna 설정을 사용한다. Coordinator 모델은 사용자가 앱에서 선택하며 프로젝트 설정으로 덮어쓰지 않는다. 복잡한 설계·동시성·서버 계약 분석/리뷰는 필요 근거와 파일 범위를 정해 Sol/high, 특히 어려운 통합 판단은 Astra/low~medium 작업으로 별도 배정할 수 있다. 모든 작업을 일괄 high로 올리지 않는다.

실제 배정 도구의 역할 정의와 TOML을 대조한다. 기존 실행 Agent의 재활성화가 새 모델을 적용한다고 가정하지 않는다. 고정 역할 설정을 호출 인자로 덮어쓸 수 없으면 지원되는 default/worker의 새 작업에 모델·추론·역할 지침을 명시하거나 설정 재로딩 후 해당 역할을 새로 시작한다. 모델을 명시한 별도 배정은 fork_turns=none으로 필요한 근거만 전달한다. 새 프로필이 등록되지 않았으면 대체 실행 경로와 미등록 상태를 기록한다.

Fast/Ultrafast와 모델 추론은 별개다. 현재 요청은 역할 모델과 배정 방식 변경이며 service_tier·요금 모드·권한은 변경하지 않는다. 실제 1.5배 완료 속도나 아트 품질 향상을 설정만으로 보장하지 않는다.

## 병렬 배정과 선택 대기

- 첫 배정에서 범위·파일 소유권·의존 작업·완료 조건·최소 검증·복원을 정한다. 공통 파일과 Unity Editor는 한 명이 소유한다.
- 독립 기능이 있으면 총괄이 직접 전부 구현하는 대신 구현·아트·분석/선택 준비를 겹쳐 진행한다. 같은 기능의 최종 리뷰·검증은 구현 후보를 고정한 다음 맡긴다.
- 현재 관찰한 슬롯은 총괄 포함4개다. 예: 총괄1 + 구현1 + 아트1 + 분석/선택1, 완료 슬롯을 리뷰·테스트로 전환한다. 이 숫자를 프로젝트 cap으로 저장하지 않고 매 실행의 실제 슬롯을 따른다. max_concurrent_threads_per_session=2 제한은 되살리지 않는다.
- 선택지 담당은 보드와 질문을 반환하면 해당 실행을 끝낸다. 총괄이 비동기 질문을 한 번 게시하고 다른 일을 진행한다. 답변 도착 시 같은 담당에게 원문·묶음/개정·출처를 전달해 후속 실행한다. 대기만 하는 Agent·폴링·불필요한 역할 호출로 병렬성을 부풀리지 않는다.
- 검증·Unity 조작을 넘기면 총괄은 같은 Editor를 건드리지 않는다. 그동안 코드 읽기·다른 파일 구현·문서/선택 준비를 진행한다. 실제 도구 슬롯보다 많은 동시 실행을 약속하지 않는다.

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

설정 근거: [공식 Subagents](https://learn.chatgpt.com/docs/agent-configuration/subagents). 프로젝트 설정과 역할 TOML을 검증하며, 문서상 권장·실제 새 배정의 모델·현재 총괄 모델은 별개로 기록한다.
