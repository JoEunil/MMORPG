# W03 Slack 재개 수신기 독립 리뷰

**최신 판정: workspace envelope 필터 수정본 기준 코드상 차단 문제 없음.** 초기 지적과 해시 이력은 보존한다. live 수신/현재 Coordinator 전달 성공을 뜻하지 않는다.

2026-10-18 KST. **초기 판정: 차단 문제 있음 — 지정 workspace 이벤트 필터 P2 1건.** Tools/SlackReceiver 다섯 파일과 HANDOFF·실행 근거를 읽었다. 이 문서만 작성했으며 receiver/테스트/CLI queue/Slack 발신·수신·프로세스·Unity를 실행하지 않았다. 기존 원본 Game scripts/상태를 수정하지 않았다.

## P2 — bot 인증과 Socket 이벤트 workspace 경계가 분리됨

`slack-receiver.mjs:86–92`는 envelope.payload.event만 acceptReply에 넘겨 payload.team_id 및 envelope.type을 확인하지 않는다. 시작 시 auth.test는 botToken의 workspace/bot을 검증하지만 SocketMode는 별도 app-level token으로 연결된다. [Slack token 문서](https://docs.slack.dev/authentication/tokens/)에서 app-level token은 workspace 단일 bot 인증과 다른 범위를 갖고, [Socket Mode 문서](https://docs.slack.dev/apis/events-api/using-socket-mode/)의 이벤트 envelope는 payload에 team_id를 담는다. 따라서 bot auth가 모든 이벤트의 workspace를 고정한다는 검증을 대신하지 못한다.

고정 workspace T0C9ECC3GS3만 수신한다는 현재 계약에 맞게 `events_api` 및 payload.team_id 일치를 확인한 뒤 기존 event 필터를 적용한다. wrong/missing team 또는 다른 envelope 유형은 ack하고 ignore하는 최소 수정과 해당 회귀 한 개면 충분하다. 기존 channel/human/thread/!coord 조건은 유지한다. 실제 다른 workspace 오배달을 관찰했다는 뜻은 아니며 명시한 입력 경계의 누락이다. live 시작 전 Coordinator에 보고했다.

## 그 밖에 확인한 경계

- Coordinator `01a14d46-8b23-7590-8ad5-7efc20f8ecd9`를 launcher와 receiver 양쪽에서 고정한다. CLI는 shell:false·명시적 `queue --thread` argv를 사용하고 `--last`나 별도 실행기 복제를 사용하지 않는다. 수신 본문은 JSON 문자열로 구분하고 보호 영역/플랫폼 승인 경계를 유지하라는 안내를 포함한다.
- 지정 channel C0C9VFZMV43/human U0C9QEUHPMK만 허용하고 bot/app/subtype 메시지·무접두사·미등록 thread를 무시한다. threads.txt에는 현재 배정한 C05 1792318770.788919 및 W03 1792318774.250569가 있다. 과거 C04/이전 준비 스레드나 등록하지 않은 새 변경 스레드를 자동 감시하지 않는다. 신규 변경 스레드는 등록 전 자동 수신된다고 주장하면 안 된다.
- 회신 ts별 pending 예약을 먼저 저장하고 ack/CLI delivery 후 queued 또는 needs_review를 저장한다. 이미 pending/queued/needs_review면 다시 queue하지 않는다. 중간 예외·불명확한 outcome은 수동 검토 대상이며 실패 자동 재전송이 없다. 동일 본문의 별도 ts는 별도 인간 회신으로 처리한다.
- 별도 receiver-active-client-state.json에 coordinator·reply ts·상태·thread만 기록하고 원문은 저장하지 않는다. 기존 receiver-state를 덮는 경로가 아니다. state 저장은 temp→rename 방식이다. state/allowlist 오류는 조용히 다른 대상으로 보내지 않고 이벤트 오류로 연결을 닫는다.
- DPAPI token은 launcher 메모리/자식 환경에서 읽고 receiver가 자기 환경에서 즉시 제거해 Codex queue 자식에 상속하지 않는다. receiver는 token/Socket URL/transport 예외·원문을 로그로 출력하지 않고 launcher는 finally에서 환경·SecureString을 정리한다. auth.test의 bot/team 불일치는 시작을 거부한다.
- 단일 Windows-session mutex는 launcher 중복 실행을 거부하며 해제는 finally에서 수행한다. stop marker가 있으면 시작을 거부하고 연결 중 주기적으로 확인해 닫는다. 연결 오류/서버 disconnect에 재접속 backoff 2–30초를 적용한다. sync CLI 최대20초 또는 재접속 sleep 중에는 stop 반영이 즉시 되지 않을 수 있으며 즉시 강제 종료 장치로 표현하지 않는다.
- 테스트는 필터·중복/재시작·pending 예약·실패/예외 no-auto-retry·별도 회신·스레드 출처/원문 비저장을 검증한다. expectation을 낮추거나 실사용자 원문/비밀을 출력하는 경로는 발견하지 않았다. receiver transport/진짜 queue/인간 회신 왕복을 대신하는 테스트는 아니다.

## 초기 동결 SHA256 및 검증 범위

| Tools/SlackReceiver 파일 | SHA256 |
| --- | --- |
| slack-receiver.mjs | `F231C1452D5EC46AECAFD5D5FD3E73A736AE3839F046EFCBDA90F7643786B536` |
| slack-delivery.mjs | `1774A12C5917D9C9C670ACD6C811BBDA720614DE7A0446A5A032223A22B1F233` |
| slack-delivery.test.mjs | `8F1B2DA0069691BE152BBC543877725D0EAB120BF875EC09F59D7C57BD125935` |
| Start-SlackReceiver.ps1 | `8A54087A2C19573655829C84665B2882815231BC9CBEAFC39E8FD80389932695` |
| threads.txt | `614A505100A07292BBF28F079989E46069D15F8193BAC7A431C4FCB71D34E219` |

위 해시를 직접 확인했다. Coordinator는 node check0 및 실제 Windows 사용자 회귀6/6을 보고했다. 초기 sandbox rename EPERM 실패 이력은 보존한다. Captures/w03-receiver.txt의 초기5/6·재실행 필요 표기는 초기 시점 기록이며 최신6/6 근거와 구분한다. reviewer는 테스트를 실행하지 않았다. live Socket 시작·재접속 실측·허용 인간의 실제 회신→현재 Coordinator queue 왕복은 아직 미검증이다. workspace 필터 수정 후 그 delta만 재리뷰한다.

## workspace envelope delta 재리뷰

acceptEnvelope가 `events_api`·payload `event_callback`·지정 team T0C9ECC3GS3를 확인한 후 기존 acceptReply로 전달한다. 다른/누락 team과 다른 유형은 ack하고 ignore한다. 실제 receiver도 event만 넘기던 경로에서 이 envelope 경계로 바뀌었으며 고정 Coordinator/channel/human/thread·pending 예약/no-auto-retry·원문 미저장·token 정리는 유지됐다. **초기 P2 해소, 코드상 차단 문제 없음.** 새 외부 경계 회귀는 잘못된 두 유형/wrong·missing team에서 ack 및 deliver 금지를, 정상 envelope에서1회 전달을 확인한다. 기존 기대를 낮추지 않았다.

직접 확인한 최신 SHA256:

| 파일 | SHA256 |
| --- | --- |
| slack-receiver.mjs | `BB8D2AA40E8E3B51F5A4A934891422279FB87969F0BA2B9F454CDB4D088631D5` |
| slack-delivery.mjs | `0B279A01FA6DE8418D9B45DE060ADD6C37646D1130ECB89DE66CEFBAE8522B10` |
| slack-delivery.test.mjs | `B969F4446D52F9CB0D4E47FB155A609B3458F5B0894C48A97DF825953F644FA9` |

Start-SlackReceiver.ps1 `8A54087A…` 및 threads.txt `614A5051…`는 초기 동결과 동일하다. Coordinator는 수정본 회귀7/7·node check0을 보고했다. 이 재리뷰에서 어떤 소스도 실행하지 않았다. Socket 연결/재접속·stop 실제 반영·지정 인간 회신→정확한 현재 Coordinator 전달은 이후 실제 검증 대상이며 정적 판정만으로 완료하지 않는다. 원래 준비 대상/상태를 보존하고 이 새 receiver를 사용하는 운영 범위와 플랫폼 자동 검토는 별도로 유지한다.

정적 리뷰 역할은 여기서 완료했다. 이후 Coordinator가 수행한 live 시작/재접속 근거는 MCP_SETUP·W03 실행 기록에서 별도로 관리하며 리뷰어 직접 실행으로 합치지 않는다. 지정 인간의 실제 회신→정확한 Coordinator queue 왕복 성공 여부도 해당 실제 기록으로 판단한다.
