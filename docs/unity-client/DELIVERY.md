# Git·캡처·Pages 전달

승인 정책은 [AGENTS.md](../../AGENTS.md)를 따른다.

## Git

HANDOFF의 정본과 실제 branch/status/diff를 대조하고 필요한 파일만 로컬 커밋한다. Editor 복사본의 기존 변경을 일괄 포함하지 않는다. push 전 fetch·분기를 확인하고 사용자 이력을 reset·강제 push로 교체하지 않는다.

새 PR은 준비된 후보의 Slack 승인을 대조한 뒤 일반 push·생성·현재 채팅 첨부한다. 같은 승인 범위의 리뷰 수정은 기존 PR에 반영한다. 게시·병합·배포는 별도 상태다.

원격 head 자동 삭제는 사용자 설정을 사용하되 병합·기본 브랜치 포함·실제 삭제를 확인한다. fetch --prune 후 로컬 브랜치와 worktree의 고유/미게시 커밋·사용 여부를 별도 확인한다. 열린 PR·현재 체크아웃·사용자 변경은 보존한다.

## 미디어

실제 Game View·HUD 포함을 확인한다. 에디터 예시·샘플값·촬영용 조명은 캡션에 밝히며 실입력/실거래 근거로 쓰지 않는다. 필요한 짧은 GIF/영상 또는 배치 PNG만 만든다. 설치된 Recorder를 먼저 검토하고 새 녹화 체계를 만들지 않는다. 성능 측정과 녹화를 분리한다.

원본은 Assets 밖 Captures/Builds에 두고 선별 공개본만 site/media/releases/<날짜>/에 추가한다. 이전 캡처를 덮어쓰거나 사용자 보존 지시 없이 자동 삭제하지 않는다. 기능 결과와 캡처 생성 결과를 구분한다.

## README 대표 이미지 갱신

작업 묶음의 결과를 정리할 때 현재 상태를 가장 잘 보여주는 검증된 PNG 한 장을 고른다. 최신 파일이라는 이유만으로 고르지 않고 현재 화면·주요 기능·가독성·실제 검증 범위가 맞는지 확인한다. 시각 변화가 없거나 새 캡처가 덜 대표적이면 기존 선택을 유지한다.

SHOWCASE.md의 'README 대표 이미지'에 버전별 원본 PNG를 지정하고 tools/site/build.py를 실행한다. 빌더가 원본을 site/media/ai-agent-preview.png에 복사해 덮어쓰며 README는 이 고정 경로 하나만 표시한다. 선택 구간은 홈페이지에 렌더하지 않는다. 버전별 원본과 화면 이력은 보존하고 고정 경로의 최신 복사본만 교체한다.

README의 설명/alt는 실제 게임·에디터 예시 여부를 선택 이미지와 맞춘다. 복사본·선택 원본·README를 같은 전달 묶음에 포함한다. 원격 README 이미지는 해당 변경이 GitHub에 반영되면 갱신되며 브라우저/GitHub 이미지 캐시로 잠시 늦게 보일 수 있다.

## 공개 원본과 소유권

| 산출물 | 정본 | 책임 |
| --- | --- | --- |
| 홈·워크플로우 도식 | SHOWCASE.md | 지정 기록 담당 |
| 기능 상세·화면 이력 | portfolio/*.md | 지정 원본 편집자 |
| Agent 지표 | AGENT_USAGE.md | 실제 실행을 확인한 담당 |
| 선별 미디어 | site/media/releases/ | 지정 아트/캡처 담당 |
| HTML·링크 | tools/site/build.py | 웹 통합 담당 |
| 게시·배포 확인 | GitHub/Actions·HANDOFF | Coordinator |

공개 기준은 PORTFOLIO_GUIDE.md다. 본문 원본은 각 한 곳이며 생성 HTML을 따로 고치지 않는다. 원본·미디어 변경 후 build.py로 생성/링크를 확인하고 필요한 화면만 브라우저에서 확인한다. 사이트 갱신 때문에 게임 검증을 반복하지 않는다.

PR Actions는 build·ai-agent-site-preview artifact, 정식 Pages는 master 기준이다. artifact·병합·배포 job·실제 URL 확인을 구분한다. 기본 URL은 https://joeunil.github.io/MMORPG/ 이다.

이전 전달·캡처·계약 문구는 [원문 이력](history/2026-10-24-guideline-audit/README.md)에 보존한다.
