# Unity Coordinator Slack 운영

승인 정책은 [AGENTS.md](../../AGENTS.md), 현재 요청·소비 상태는 [HANDOFF](HANDOFF.md)가 정본이다.

## 발신

작업 채널 C0C9VFZMV43, 완료 채널 C0CA7BSKN3B를 사용한다. 현재 실행 도구는 HANDOFF의 ROOT/UnityClient/Tools 경로다. Git 정본의 도구와 버전이 다를 수 있으므로 임의로 교체하지 않는다. 이 경로의 Send-CoordinatorSlack.ps1의 기존 DPAPI BotToken·auth.test로 workspace T0C9ECC3GS3와 bot U0C9ZRTQ6F8을 확인한다. 사용자 계정 커넥터로 발신하지 않는다.

새 묶음은 -NewThread -MessageFile로 부모를 만들고 반환 ts·링크와 SlackReceiver/threads.txt 자동 등록을 확인한다. 같은 묶음은 -ThreadTs에 정확한 문자열 ts를 지정한다. 게시 후 등록 실패는 반환된 ts를 복구하며 중복 부모를 만들지 않는다. 결과 불명확 시 읽기로 확인한다.

완료 요약은 완료 채널의 새 메시지로 남긴다. 승인 수신 대상을 완료 채널로 옮기지 않는다.

## 수신 상태

실제 프로세스, receiver-active-client-health.json의 phase=listening·30초 이내 updatedAt, 고정 Coordinator 01a14d46-8b23-7590-8ad5-7efc20f8ecd9·등록 스레드를 대조한다. sandbox LOCALAPPDATA는 실제 사용자 경로와 다를 수 있다. UTC 문자열·파일 mtime으로 대조하고 시각 파싱 오차를 장애로 오인하지 않는다.

꺼졌으면 기존 stop 마커·대상·설치 codex queue 경로를 확인하고 Start-SlackReceiver.ps1을 숨김 실행해 SOCKET_READY·최근 health를 확인한다. 토큰·권한·정책·대상 채팅을 바꾸지 않는다. 재부팅 자동 시작은 현재 구성하지 않았다.

## 승인 처리

1. 지정 사람 U0C9QEUHPMK·채널·등록 스레드·정확한 요청을 대조한다.
2. 회신 ID·범위/개정·현재 대기·기소비를 확인한다.
3. 유효한 승인을 한 번만 적용하고 같은 앱 스레드에 결과 또는 불일치 사유를 남긴다.
4. 요청·회신·처리 결과를 HANDOFF에 기록한다.

짧은 명령은 승인 <ID>, 기존 !coord 형식도 실제 코드가 허용하는 범위에서 사용한다. 수신확인은 전달 점검이며 개발/게시 승인이 아니다. 문서만 바꿔 수신 문법이 추가됐다고 보고하지 않는다.

코드/필터, Socket 연결, 큐 접수, 실제 사람 답장 전달, 승인 적용을 구분한다. queued는 적용 완료가 아니다. 과거 승인을 재사용하거나 미전달 답장을 일괄 재생하지 않는다.

본문은 한 일·문제와 해결(있을 때만)·검증·남은 일을 3~5줄로 쓰고 승인 요청에는 구체적 효과·위험·짧은 복사 명령 하나만 담는다.

과거 스레드·준비 기록은 [이력](history/2026-10-24-guideline-audit/README.md)에 보존한다.
