# Unity MCP 연결과 테스트 안내

공통 운영·승인·병렬 실행·테스트 기준은 [현재 운영 기준](CURRENT_POLICY.md)을 따른다. 현재 워크플로우는 시작된 상태다. 아래 준비·연결·승인 기록은 당시 이력이며, 현재 대기 상태나 실행 권한을 뜻하지 않는다.

## W03 연결 이력

사용자 단계 완료/Slack 재개 지시에 따라 작업 체크아웃 Tools/SlackReceiver로 새 수신기를 준비했다. 현재 채팅01a14d46-8b23-7590-8ad5-7efc20f8ecd9/등록C05·W03스레드만 대상이며 이전 원본Game 준비 수신기/상태는 보존한다. 독립 정적 리뷰·필터7/7·실제BOT_AUTH/Socket재연결/LISTENING까지 확인, 실제 지정 사람 회신 왕복은 아직미검증이다. 최신 운영은 SLACK_WORKFLOW.md를 따른다. 아래 준비 단계 연결·ID는 이전 시점 기록이다.

## 이전 준비 상태

- 완료: `master` 기반 `codex/unity-ai-client` worktree, 빈 URP 프로젝트, Unity MCP 연결·컴파일, Recorder GIF 저장, Slack 수신·재연결·중복 억제·동일 준비 채팅 자동 재개.
- 완료: 임시 Play Mode 오브젝트에서 마우스 입력→위치 `(1,0,0)`, Space 입력→초록 재질을 assertion으로 확인했다. `Assets/Captures/input-smoke-before.png`와 `input-smoke-after.png`에서 실제 화면 변화도 확인했다. Game View 포커스가 필요했고 캡처 시 정지된 이전 프레임을 피하려고 시험 동안만 `Application.runInBackground`를 켰다. 입력은 `down`/`up`을 분리해 전달했다. 임시 InputAction을 해제하고 Play 종료 및 기존 백그라운드 설정 복구를 마쳤다. 게임 이동/스킬·OS 입력 검증은 아니다.
- 완료: 이 작업 체크아웃의 루트 `AGENTS.md`와 공통·역할 문서를 적용했다. 개발은 시작 대기 상태다. Slack 실행 스크립트는 원본 `C:/Users/joeun/Desktop/workspace/Game/scripts/unity-client/`에 있으며 수신기는 아직 준비 채팅에 연결되어 있다. 이 문서의 Slack 스크립트 상대 경로는 해당 원본 Game을 기준으로 한다.
- 완료: 승인된 로컬 로그인/게임 서버를 실행하고 전용 계정 2개의 회원가입·로그인을 검증했다. 게임 월드 진입은 개발 단계 검증이다. 경로·권한·프로시저 반영 근거는 LOCAL_PREPARATION.md를 따른다.
- 확정: 개발 시작 후 ClientCore Unity 최소 호환 수정과 관련 검증 후 작업 브랜치 커밋 허용. push·병합·공개 및 범위를 벗어난 변경은 별도 승인. GitHub Pages는 공개 단계에서 설정한다.
- 시작 조건: 위 준비 결과를 인계한 뒤 사용자의 워크플로우 시작 지시로 개발을 시작한다. 역할 문서의 모델 권장값은 실제 실행 설정과 구분한다.

## 이전 Slack 연결과 로컬 도구 기록

현재 사용자 정책(2026-10-18): 변경 단위마다 #unity-client에 새 부모 메시지/스레드를 만든다. 기존 상시 연결 스레드는 연결 시험/과거 기록이며 새 변경 승인·결과는 각 전용 스레드로 묶는다. 수신기는 원래 단일 thread 필터이므로 새 스레드 자동 전달이 되는 것으로 보고하지 않는다. Coordinator 대상 설정은 그대로 유지하고 현재는 커넥터로 채널과 각 thread를 직접 확인한다. `oldest`를 사용한 조회가 답변을 놓친 사례가 있어 승인 확인 때 전체 해당 스레드를 읽고 원문/발신자/ts를 대조한다.

아래 `scripts/unity-client/` 도구는 원본 PC의 이전 로컬 도구이며 저장소에 포함하지 않는다. 새 체크아웃에서 그대로 실행할 수 있는 설치 절차가 아니다. 현재 수신기는 `UnityClient/Tools/SlackReceiver/`를 기준으로 실제 설정을 확인한다.

### 로컬 토큰 저장

Slack 앱의 App-Level Token(`xapp-`, `connections:write`)과 설치 후 발급된 Bot User OAuth Token(`xoxb-`)은 사용자 PowerShell에서 `scripts/unity-client/Save-SlackTokens.ps1`을 실행해 입력한다. 입력은 숨김 처리되며 명령어 기록에 토큰 값을 넣지 않는다.

저장 위치는 `%LOCALAPPDATA%\UnityCoordinator\slack-tokens.xml`이다. Windows DPAPI로 암호화하며 같은 PC의 같은 Windows 계정에서 로컬 수신기가 읽는다. 기존 파일은 덮어쓰지 않는다. 토큰을 저장소·문서·Slack·대화에 복사하지 않는다. 저장 완료는 Slack 인증이나 자동 재개 검증 완료를 뜻하지 않는다.

### 연결 확인 상태

- 전용 봇의 Bot Token 인증과 Socket Mode 연결 성공. [로컬 수신 시험 스레드](https://app.slack.com/archives/C0C9VFZMV43/p1792248488597149)에서 지정 사용자 `U0C9QEUHPMK`의 `수신 확인` 회신(`1792248540.259609`)을 로컬 WebSocket으로 수신했다.
- 시험 도구: `scripts/unity-client/Test-SlackReceiver.ps1` 및 `test-slack-receiver.mjs`. 지정 채널·시험 스레드·사용자의 일치하는 답장만 확인하고 종료하며, 5분 제한을 둔다.
- [자동 재개 시험 스레드](https://app.slack.com/archives/C0C9VFZMV43/p1792248686381279)의 `재개 확인` 회신(`1792248925.925489`) → 로컬 Socket Mode 수신 → `codex queue` → 준비 채팅 `01a143fd-0297-7fe0-a928-7afa4c2f04c3` 자동 재개를 확인했다. 재개된 채팅에서 로컬 기록의 채널·스레드·사용자·회신 ID를 대조했다. 게임 개발은 시작하지 않았다.
- 1회 시험 수신기는 종료했다. 상시 수신기는 아래 별도 스크립트로 실행한다. 향후 실제 개발용 Coordinator 채팅을 만들면 대상 ID를 별도로 연결해야 한다.

### 상시 수신 운영

- 실행: `scripts/unity-client/Start-SlackReceiver.ps1 -Coordinator <채팅 UUID> -CodexPath <codex.exe 절대 경로>`. 현재 준비 채팅은 `01a143fd-0297-7fe0-a928-7afa4c2f04c3`이다. 토큰은 기존 암호화 파일에서 읽으며 추가 패키지는 필요 없다(Node.js 22 사용).
- [상시 연결 스레드](https://app.slack.com/archives/C0C9VFZMV43/p1792249122665229)에서 지정 사용자가 `!coord 내용`으로 답장하면 전달한다. 일반 채널 메시지·다른 스레드·다른 사용자·봇·수정 메시지는 무시한다. Coordinator의 Slack 출력에는 이 접두어를 사용하지 않는다. 전달된 답장이 보호 영역 변경의 포괄 승인이 되는 것은 아니다.
- Socket 종료/오류 및 Slack disconnect 이벤트 이후 2~30초 간격으로 재연결한다. 실제 연결을 한 번 끊은 뒤 다시 `SOCKET_READY`와 `LISTENING`으로 복귀하는 것을 확인했다. 두 번째 수신기 실행도 mutex로 차단됨을 확인했다.
- `%LOCALAPPDATA%\UnityCoordinator\receiver-state.json`에 스레드·대상 채팅·처리한 회신 ID를 저장한다. `pending`을 먼저 저장하고 Slack에 ACK한 뒤 Codex 큐로 보낸다. 같은 회신 ID는 재연결·재시작 후에도 다시 보내지 않는다. 서로 다른 회신에 같은 문장을 보내는 것은 새 입력이다.
- 큐 실패나 중단으로 남은 `pending`/`needs_review`는 자동 재전송하지 않는다. 실제 채팅 수신 여부를 확인한 뒤 필요한 경우 사용자가 새 답장을 보낸다. 큐 등록과 로컬 상태 저장을 하나의 원자적 작업으로 묶을 수 없으므로 무손실·정확히 한 번 전달을 보장하지 않는다.
- 검증: 가짜 데이터로 중복 재수신, 상태 재로딩 후 중복 억제, 허용 대상 필터, 실패/불명확한 전달 재시도 억제, 서로 다른 답장 처리를 확인했다(테스트 4개 통과). 실제 Slack 재연결과 중복 프로세스 차단도 통과했다.
- 상시 수신의 회신 `1792249169.360249`가 지정 준비 채팅에 도착해 자동 재개된 것도 확인했다. 수신기 부모 PowerShell 및 자식 Node 프로세스가 실행 중임을 재확인했다.
- 현재 사용자 세션에서 숨김 실행한다. 로그는 같은 로컬 폴더의 `receiver.stdout.log`/`receiver.stderr.log`, 실행 부모 PID는 `receiver.pid`에 기록한다. 중지할 때는 PID와 실행 명령을 대조하고 해당 수신기 자식 Node 프로세스도 함께 종료한다. PC 재부팅 후 자동 시작·프로세스 장애 후 자동 재실행·오프라인 기간의 메시지 이력 복구는 포함하지 않는다.

- 워크스페이스: `AI Agent` (`T0C9ECC3GS3`)
- Coordinator 채널: `#unity-client` (`C0C9VFZMV43`, 공개 채널)
- [연결 시험 스레드 SLACK-001](https://aiagent-vqu1541.slack.com/archives/C0C9VFZMV43/p1792246783410039)
- 채널 생성·테스트 메시지 발송·사용자 메시지 및 스레드 회신 읽기 완료. 시험 스레드의 `연결 확인` 회신(`1792246945.967719`)을 확인했다.
- 사용자 확인 계정: `U0C9QEUHPMK`. 연결 시험 회신을 코드 변경 승인으로 사용하지 않는다. 연결 도구가 같은 사용자 명의로 발신하므로 승인 판정은 사용자 ID만으로 하지 않고 요청·회신·도구 발신 기록을 함께 구분한다.

자동 재개 연결은 Slack Socket Mode로 지정 채널의 스레드 회신을 로컬 수신기에 전달한다. 현재 Slack 커넥터에는 이벤트 구독 도구가 노출되어 있지 않다. 로컬 CLI의 `codex queue --thread <명시적 ID> --message <내용>`을 통한 준비 채팅 재개는 위 시험에서 확인했다. 다른 Coordinator 채팅을 사용할 때도 명시적 ID로 연결하고 실제 재개를 확인한다. 큐 등록만으로 자동 재개 성공을 선언하지 않는다. 계정 로그인·앱 생성/설치는 사용자가 진행하고 토큰은 로컬에만 저장한다. 연결 준비 중에는 게임 개발 워크플로우를 시작하지 않는다.

**Unity 7 업그레이드는 선행 조건이 아니다.** 공식 Unity CLI의 Editor 연결은 Unity 6.0 LTS 이상, CoplayDev의 MCP는 Unity 2021.3 LTS 이상을 안내한다. 현재 `6000.0.47f1`은 명시된 버전 범위에 들어가며 실제 패키지 호환성은 연결 시 검증한다.


빈 URP 프로젝트·역할 모델/추론·ClientCore 단일 원본/P01 정책·Slack 승인 왕복과 자동 재개를 포함한 전체 구축 순서는 [PIPELINE_GUIDE.md](PIPELINE_GUIDE.md)를 먼저 읽는다. 연결 준비와 실제 기능 개발을 구분한다.

## 사용할 연결

직접 MCP 서버를 만들지 않는다. **공식 Unity CLI의 MCP + Unity Pipeline 패키지**를 먼저 검토하고, 필요한 도구/호환성에 문제가 있으면 기존 후보인 [CoplayDev의 MCP for Unity](https://github.com/CoplayDev/unity-mcp)를 비교한다. CoplayDev 제품은 Unity 공식 제품과 별개다. 검증할 때는 연결 한 종류만 사용하고 도구명·설정을 섞지 않는다.

Unity 공식 안내는 이전 `com.unity.ai.assistant` 안의 MCP 서버를 폐기 대상으로 설명하고 **CLI의 `unity mcp`로 이전**하도록 안내한다. CLI의 MCP와 서드파티 MCP가 폐기됐다는 뜻은 아니다. [공식 이전 안내·버전 요구사항](https://docs.unity.com/en-us/unity-cli/replace-mcp-server-unity-cli), [Unity AI 도구 안내](https://unity.com/features/ai)

```text
사용자 ↔ Codex Coordinator → 구현 / 리뷰 / 테스트 등 필요한 역할
                          테스트·배치 담당 한 명
                              ↔ 로컬 Unity MCP 서버
                              ↔ Unity Editor 연결 패키지
                              ↔ UnityClient/ 프로젝트

Unity 클라이언트 ↔ 로그인/게임 서버 ↔ Redis/DB
```

게임 접속은 기존 ClientCore가 처리한다. MCP는 Editor 조작/조회 도구이며, 역할 배정·리뷰·재시도는 Codex Coordinator가 담당한다. 역할 지침과 MCP 연결만으로 테스트 시나리오가 생기지는 않는다. 입력/서버 결과 검증 코드를 작성하고 작은 기능 하나의 전체 흐름을 확인한 뒤 본 개발을 시작한다. 처음부터 무인 운영 서버를 따로 만들지 않는다.

## 같은 저장소에서 Game 읽기

같은 Game 저장소의 전용 worktree를 Codex 작업 루트로 두고, Unity Editor는 그 안의 `UnityClient/`를 연다. 파일 도구는 같은 체크아웃의 서버 소스를 읽는다. 원래 Game 작업 폴더의 미완료 변경은 별도로 확인하며 서버 소스와 실행 빌드가 일치한다고 추측하지 않는다.

| 경로/기능 | 기본 접근 |
| --- | --- |
| `UnityClient/`, `site/`, `docs/unity-client/` | 작업 카드에 배정한 파일 읽기·쓰기 |
| README·지침·ignore·Pages 설정 | 해당 작업에 필요한 지정 변경만 |
| 기존 서버 코드·프로토콜·DB·설정·원본 ClientCore | 사용자 승인이 없는 범위는 읽기만. P01의 콘텐츠 범위는 자율 변경, 보호 영역은 별도 승인 |
| Unity Editor | 해당 클라이언트 프로젝트만 연결, 조작권 순차 배정 |
| 승인된 서버 작업 | 같은 기능 브랜치 또는 필요시 별도 체크아웃에서 지정 범위만 쓰기 허용 |

Coordinator는 `CoreLib/PacketTypes.h`, `ClientCore/Interfaces.cs` 같은 일반 소스 읽기를 확인한다. Game 전체를 Unity `Assets`로 가져오거나 읽기 확인을 위해 서버 파일에 시험 쓰기를 하지 않는다. 다른 PC/클라우드의 일반 채팅이 사용자의 localhost Editor에 바로 접근할 수 있다고 가정하지 않는다. 개발은 로컬 Editor와 연결 가능한 Codex 실행 환경에서 진행한다.

Codex의 지원 권한 설정에서 해당 경로를 읽기 접근으로 지정한다. 환경에 따라 설정 방식은 다르므로 새 프로젝트에서 실제 적용을 확인한다. CLI의 `--add-dir`는 추가 **쓰기** 권한이므로 읽기 전용 참조 설정으로 사용하지 않는다. 하위 에이전트 권한은 부모를 상속하므로 “서버 요청 담당만 쓰게 한다”는 역할 문구로 격리가 생기지 않는다. [파일 권한 안내](https://learn.chatgpt.com/docs/permissions), [CLI 옵션](https://learn.chatgpt.com/docs/cli/reference), [하위 에이전트 권한](https://learn.chatgpt.com/docs/agent-configuration/subagents)

Coordinator가 일반 질문/승인 요청을 취합하더라도, 앱이나 운영체제의 필수 권한 확인 창은 사용자에게 직접 표시될 수 있다. 이 창을 없애기 위해 전체 접근 권한을 켜는 방식을 기본값으로 삼지 않는다.

## 공식 연결 순서

**착수 구분:** 이 절차의 연결·Play/Stop·임시 입력 테스트·PNG/GIF·Slack 왕복은 Coordinator 개발 워크플로우를 시작하기 전에 현재 준비 작업으로 수행한다. 게임 기능 구현은 시작 후 진행한다. 상세 선행 조건은 [Coordinator 시작 전 준비](PIPELINE_GUIDE.md#coordinator-시작-전-연결-준비와-시작-후-개발)를 따른다.

1. `UnityClient/`를 열고 Editor 버전·렌더 파이프라인·정상 컴파일을 확인한다.
2. [Unity의 Codex 플러그인 안내](https://docs.unity.com/en-us/ai/unity-plugin/codex)에 따라 로컬 Codex에 공식 플러그인을 설치하는 방식을 검토한다. 플러그인은 Unity용 지침/도구 설정을 제공하며 GPT 앱 목록의 Unity 항목 추가와 동일한 절차라고 가정하지 않는다. 새 세션에서 실제 활성 상태를 확인한다.
3. [공식 CLI 연결 안내](https://docs.unity.com/en-us/unity-cli/replace-mcp-server-unity-cli)에 따라 CLI와 대상 프로젝트의 Unity Pipeline 패키지를 준비한다. 지원되는 클라이언트 목록과 실제 CLI 버전을 확인한 뒤 Codex MCP 설정에 연결한다. 기존 설정을 덮어쓰지 않는다.
4. 자동 설정 명령이 변경할 파일과 권한도 확인한다. 일부 CLI 버전의 Codex 설정은 sandbox/network 설정을 함께 변경하므로 서버 보호 경계를 유지하는 최소 설정을 적용한다. [CLI 변경 내역](https://docs.unity.com/en-us/unity-cli/release-notes)
5. 실제 MCP 도구 목록, 프로젝트 절대 경로, Editor 인스턴스, Console·Scene 읽기, 컴파일, EditMode/PlayMode 작은 테스트를 확인한다. 설치 성공이나 도구 목록 표시만으로 연결 검증을 끝내지 않는다.
6. 임시 테스트 Scene에서 입력→화면 변화→assertion→Game View 캡처를 확인한다. Recorder 패키지로 짧은 GIF도 저장·재생한다. 이 연결 시험은 Coordinator 시작 전 수행하고, 실제 게임 기능의 구현→리뷰→서버 통합 테스트는 시작 후 수행한다. 누락 도구는 작은 테스트 도우미 또는 공식 CLI로 보완하고 실제 가능한 범위를 기록한다.

공식 문서의 Codex 플러그인 설치 명령은 아래와 같다. 설치 시 현재 로컬 Codex가 해당 명령을 지원하는지 먼저 확인하고 설치 후 새 세션에서 활성 상태를 검증한다. 플러그인 활성화와 실제 Editor 연결 검증은 둘 다 필요하다.

```text
codex plugin marketplace add Unity-Technologies/unity-agent-plugin
codex plugin add unity@unity-agent-plugin
codex plugin list
```

Unity CLI 쪽은 대상 프로젝트에서 Pipeline 패키지 설치와 MCP 클라이언트 설정을 진행한다. `unity mcp configure --list`로 지원 대상을 확인하고, 자동 설정에 앞서 위의 파일·권한 변경 범위를 확인한다. 현재 Unity 프로젝트와 서버 보호 정책을 모르는 상태에서 전역 설정을 일괄 적용하지 않는다.

## CoplayDev 대안 연결 순서

1. 새 Unity 프로젝트를 열고 정확한 Editor 버전과 사용할 렌더 파이프라인을 기록한다. 패키지 설치 전 프로젝트가 정상 컴파일되는지 확인한다.
2. 후보 MCP의 설치 요구사항인 Git, Python 3.10 이상, `uv`를 확인한다. 미설치 도구는 실제 설치 단계에서 안내에 따라 준비한다.
3. Unity Package Manager의 Git URL 설치를 사용한다. 프로젝트 문서의 시작 URL은 다음과 같다. 지속 사용 전 검증한 release tag 또는 commit으로 고정하고 패키지 잠금 파일을 관리한다.

   ```text
   https://github.com/CoplayDev/unity-mcp.git?path=/MCPForUnity#main
   ```

4. `Window → MCP for Unity`에서 의존성 설정과 로컬 서버 시작을 완료한다. HTTP 방식과 실제 표시된 주소를 확인한다. 문서의 기본 주소는 `http://localhost:8080/mcp`다. [설치·연결 원문](https://coplaydev.github.io/unity-mcp/getting-started/install)
5. Codex가 사용하는 작업 체크아웃 루트의 `.codex/config.toml`에 아래 설정을 **기존 설정과 병합**한다. 이 HTTP 주소는 CoplayDev용 예시이며 공식 CLI 설정에 복사하지 않는다. 실제 MCP 창의 값으로 맞추고 전역 설정을 통째로 덮어쓰지 않는다.

   ```toml
   [mcp_servers.unityMCP]
   url = "http://localhost:8080/mcp"
   ```

   Codex는 프로젝트 단위 MCP 설정을 지원한다. 프로젝트 신뢰/권한을 확인하고 클라이언트를 다시 연결한 뒤 도구 목록을 조회한다. CLI를 사용하는 경우 `codex mcp list`로 설정 등록을 볼 수 있지만 이것만으로 Unity 연결 성공은 아니다. [Codex MCP 설정](https://learn.chatgpt.com/docs/extend/mcp?surface=cli)
6. MCP로 연결된 프로젝트 경로/Editor 인스턴스를 확인하고 Console과 활성 Scene을 읽는다. 여러 Editor가 있으면 대상 인스턴스를 명시한다. [다중 인스턴스 안내](https://coplaydev.github.io/unity-mcp/guides/multi-instance)
7. 전용 테스트 Scene에서 작은 오브젝트 생성·조회·정리와 의미 있는 EditMode 테스트 1개를 확인한다. PlayMode도 1개 실행해 최종 결과를 받는다. 설치 버전의 실제 도구명·인자는 연결 후 조회해 사용한다.

Editor 대상은 `UnityClient/`로 한정한다. Editor 스크립트 실행 도구는 Codex 파일 sandbox와 별도 프로세스 권한으로 동작할 수 있다. 선택한 MCP의 도구 허용 목록과 실행 계정/파일 권한을 함께 확인한다. 지침만으로 “서버 수정이 기술적으로 불가능”하다고 주장하지 않는다.

## 자동 테스트 범위

| 층 | 자동 확인 항목 | 필요 환경 |
| --- | --- | --- |
| EditMode | 패킷 fixture 해석, TCP 분할/합침·길이 한계, ID/좌표 변환, export 검증 | 서버 없음 |
| PlayMode | 표시 생성/갱신/제거, 메인 스레드 적용, UI 상태, 이벤트/연결 수명 | 일반적으로 서버 없음 |
| 실제 서버 통합 | 로그인·진입·두 계정 이동/전투·채팅·인벤토리 조회·Zone 전환·재접속 | 지정 DB/Redis/로그인/게임 서버, 테스트 계정 |
| 거래소 UI | 등록·검색·구매·아이템 지급·판매 대금 수령 | 실제 클라이언트 API 연결, 전용 계정·아이템/재화. 부족한 서버 연결은 승인 후 구현 |
| 화면/빌드 | 플레이 카메라·UI 겹침·범위 예고·재질, 독립 실행 클라이언트 | Editor 및 필요한 빌드 도구 |

CoplayDev를 선택했다면 `run_tests`와 `get_test_job`으로 실행과 결과를 확인한다. 공식 CLI를 선택했다면 실제 노출된 테스트 도구/CLI와 원본 보고서를 사용한다. **실제 실패 수와 상세 결과**를 확인하며 테스트 코드와 합격 조건은 먼저 작성한다. Play 진입이나 도구 호출 성공 자체를 테스트 통과로 보지 않는다. [CoplayDev 실행 도구](https://coplaydev.github.io/unity-mcp/reference/tools/testing/run_tests), [결과 조회](https://coplaydev.github.io/unity-mcp/reference/tools/testing/get_test_job)

컴파일/리로드 중에는 새 테스트를 시작하지 않는다. 실행 중 코드·에셋 수정은 멈춘다. 연결 timeout은 환경 문제와 테스트 실패를 구분해 조사하고, 진행 중인 작업을 확인 없이 지우거나 겹쳐 실행하지 않는다. 두 클라이언트 검증은 Editor 1개 + 별도 실행 빌드 1개·서로 다른 계정을 기본으로 한다. Standalone 조작까지 MCP가 제공한다고 가정하지 말고 빌드에 필요한 작은 테스트 진입점이 있는지 확인한다.

스크린샷은 화면 검수의 근거로 남기며 대표 기능에는 짧은 GIF를 자동 생성한다. 실제 입력 경로·서버 assertion·Recorder·보관 정책은 [DELIVERY.md](DELIVERY.md)를 따른다. “멋있다”는 최종 미술 판단은 첫 구역에서 사용자가 확인한다. 전체 회귀를 매 수정마다 반복하지 않고 변경의 영향에 맞춰 실행한다.

## 개발 서버 실행 정책

첫 연결은 **사용자가 DB/Redis를 준비하고, 허용하면 에이전트가 로그인/게임 서버의 실행 상태를 확인·시작**하는 방식이 간단하다. 아직 시작 권한이나 경로가 정해지지 않았다면 기존 서비스를 사용하거나 환경 차단으로 보고한다.

- 준비할 정보: 서비스/실행 파일 절대 경로, 작업 디렉터리, 실제 포트, 준비 확인 방법, 정상 종료 방법. 기본 포트 후보는 MySQL 설정값·Redis 6379·로그인 3000·게임 9999이며 현재 설정을 우선한다.
- 시작 순서: DB → Redis → 로그인 → 게임. 실행 중인 올바른 서비스는 재사용한다. PID·실행 경로와 건강 상태를 확인하며 모르는 포트 점유 프로세스를 죽이지 않는다.
- 로그인은 기존 `Login/` 작업 디렉터리의 시작 명령을 사용한다. 게임은 사용자가 지정한 빌드(현재 후보 `x64/Release/MainServer.exe`)를 사용하고 서버 루트 기준 상대 경로를 유지한다. 재빌드·설정 수정·바이너리 교체는 별도 승인 전 수행하지 않는다.
- 실제 계정 로그인/월드 진입으로 준비 상태를 최종 확인한다. 포트 연결 성공만으로 판정하지 않는다.
- 자동 시작한 프로세스만 PID·실행 경로·시작 시각을 기록한다. background helper는 숨김으로 시작한다. 종료 시 기록을 재확인하고 정상 종료를 우선한다. 사용자 프로세스는 유지한다.
- 실행에 따라 생기는 서버 로그/WAL·정상 게임 데이터 쓰기는 지정된 로컬 테스트 환경의 실행 허용 범위로 구분한다. 소스/설정/DB 스키마 변경, 데이터 초기화와는 별개다.
- 비밀번호·토큰은 사용자 지정 로컬 환경에서만 사용하고 문서/명령 출력에 노출하지 않는다. 서버 실행 스크립트가 필요하면 `UnityClient/` 아래 작은 스크립트 하나로 시작한다.

## 필요한 플러그인만 연결하기

- **Unity:** 공식 플러그인의 지침 로드는 확인됐다. 다음은 로컬 CLI와 실제 Editor 연결 검증이다. 이전 플러그인 검색에서 관련 항목을 찾지 못한 결과를 현재 설치 상태로 해석하지 않는다. 공식 Codex 플러그인·CLI·프로젝트의 Pipeline 패키지 상태를 각각 확인한다.
- **GitHub:** PR/이슈/Actions 확인에 사용한다. 현재 목록상 설치됨이다. 대상 저장소 접근과 필요한 쓰기 권한은 실제 사용 전에 확인한다. 로컬 커밋과 Git 인증, Pages 저장소 설정은 각각 별개다.
- **Slack:** 사용자는 변경 요청과 직접 확인할 사항을 Slack에서 처리할 예정이다. 채널·승인자·공유 범위를 확정한 뒤 발신→회신 확인→Coordinator 재개를 검증한다. 에이전트끼리는 Codex 내부로 협업한다. 기존 연결에 자동 수신/재개가 없다면 [구축 가이드](PIPELINE_GUIDE.md)의 작은 로컬 수신기를 검토한다.
- **아트:** 이미지 생성은 현재 기본 도구를 사용할 수 있다. 3D 모델/리깅 편집이 실제로 필요할 때 Blender 연결 또는 생성 서비스를 검토한다. 이번 플러그인 검색에는 Blender 항목이 없었다. [플러그인 디렉터리](https://chatgpt.com/plugins)와 해당 도구의 실제 설치 경로를 확인한다.
- 현재 GitHub Pages와 저장소 문서로 충분하므로 별도 웹 호스팅, Notion/Linear 같은 중복 기록 도구를 선행 조건으로 넣지 않는다. 추가 연결마다 운영/권한 관리가 늘어나는지 먼저 판단한다.

## 에셋 자동화의 범위

Unity MCP는 에셋 임포트·배치·재질·Scene·테스트 작업을 제어하는 연결이다. 연결 자체가 완성도 높은 3D 모델·리깅·애니메이션을 모두 제공한다는 뜻은 아니다.

처음에는 보유 에셋/사용 가능한 라이선스의 리소스와 간단한 모델로 한 구역을 만든다. 2D 아이콘·텍스처·콘셉트는 이미지 생성 도구를 사용할 수 있다. 3D 생성/리깅 서비스나 별도 모델링 도구 연결이 필요하면 실제 기능·비용·출력 품질을 확인한 뒤 선택한다. 서비스가 없다고 가짜 생성 결과나 준비되지 않은 Prefab을 완료로 보고하지 않는다.
