# 현재 인계 — 2026-10-25

## 현재 작업

현재 요청의 남은 패널 조작과 마을–사냥터 길 왕복을 실제 서버 세션에서 완료했다. 새 제품 코드 변경은 없고 기존 로컬 UI 묶음의 실서버 근거와 공개 화면을 보완했다.

- 패널: 정상 PlayerLoop QueueStateEvent 합성 키로 I 가방 열기 → B 거래소 전환/가방 닫힘 → Esc 모두 닫힘, 서버 위치 불변. 실제 빈 가방·매물30개 표시. native 키 조작으로 보고하지 않는다. 보유 아이템/재화가 없어 성공 거래 차감은 미검증이다.
- 왕복: 마을(10.61,9.66) → 기존 길 끝(99.68,9.66) → 이음(151.92,9.66) → 사냥터(209.96,9.66) → 기존 사냥 구간(280.14,9.66) → 마을(9.89,9.66). Zone1→2→1 및 각 정지점 drift0, HP7875 유지. 전체6구역/고지연/최신 사용자AI 검증은 아니다.
- 첫 러너는 Camera.main을 사용해 잘못된 방향으로 경계에 닿았다. 실제 입력 기준 world.ViewCamera로 러너만 고쳐 길 구간만 재검증했고 통과한 패널은 반복하지 않았다. 실패 근거 보존. 제품 결함/지도 수정으로 처리하지 않는다.
- 실제 로그인/선택/진입/로그아웃은 마우스 클릭, 보호된 자격증명은 기존 GameView 텍스트 helper 사용. 이동/패널은 합성 키. 위치·HP·모델 주입/DB 수정/서버AI 수정 없음.
- owned MainServer PTY49355 ROOT cwd에서 기존 WAL/log를 사용, q 정상 종료 exit0. Login PTY98654 Game/Login에서 실행 후 Ctrl+C 종료 exit1. 포트3000/9999 미수신, Unity Edit/clean/compilingfalse/backgroundfalse 복원. 임시 입력 해제, Redis/MySQL 변경 없음.
- 공개 인벤토리/거래소/필드에 실제 캡처3개와 검증 범위를 추가했다. 기존 이미지는 보존한다. 상세 근거 ROOT/UnityClient/Builds/Play-Session/panel-route-details.txt 및 route-followup-details.txt/run.txt.
- 남은 것은 실제 보유 데이터 조건의 거래 차감과 더 넓은 플레이/아트 평가다. 기존 범위 안의 별도 구현 결함은 확인하지 못했고 새 선택 대기는 없다. DB를 채워 완료시키지 않는다. CLIENTUI01 게시 승인 요청을 보냈으며 수신·push는 아직 없다.

### 보존한 클라이언트 묶음

사용자가 PR53을 직접 검토하는 동안 다음 클라이언트 묶음을 진행하라고 지시했다. 새 로컬 브랜치 codex/client-ui-interaction에 가방·거래소 Esc 닫기와 채팅 기록 스크롤 보존을 구현했다. 기존 기사·성곽길A·가방A·간소화 거래소B 선택을 유지하며 새로운 취향 선택은 필요하지 않았다. 앞으로 필요해지면 관련 샘플을 한 번에 묶는다.

- 완료: 열린 가방/거래소에서 Esc는 기존 ClosePanel 경로를 사용한다. 기존 대기 거래 보존·불확실한 수령 후 가방 갱신·자동 재전송 금지는 유지한다. 채팅은 이전 기록을 읽을 때 스크롤 비율을 보존하고 최신 위치/자기 echo에는 아래를 따라간다. 같은 문장 고정 기능은 아니다.
- 검증: 독립 코드 리뷰 차단 없음, Unity 컴파일 통과, 새 영향 PlayMode 자동3사례만 통과(채팅2/Esc1). 입력은 InputSystem/가짜 네트워크 fixture이며 native 실입력·실거래 성공으로 보고하지 않는다. 이미 통과한 게임 테스트는 반복하지 않았다.
- 기존 미확인: I/B native 입력 도달 문제는 코드상 결함이 없어 임의 수정하지 않았다. 빈 가방/미확인 재화 때문에 성공 거래 차감·아이템 보유 실화면 미검증, 연장 길 전체 왕복은 위 실서버 합성키 배치에서 완료했다.
- 금지: 사용자 몬스터 AI·서버·패킷·경제·DB·충돌·스폰 변경. 이번 작업은 기존 UI 안의 동작만 바꿨다. 입력 허용 유지.

## 정본과 실행

- Git: C:/Users/joeun/Desktop/workspace/Game-UnityClient/UnityClient/Builds/C10-Delivery/source, codex/client-ui-interaction. PR53 head552c5fe 기반으로 다음 작업만 로컬 누적한다. origin/master7289a73에서 PR53 병합 및 head552c5fe 포함 확인. 이번 후보595f822는 이후 변경29파일만 포함한다.
- Editor: C:/Users/joeun/Desktop/workspace/Game-UnityClient/UnityClient, PID38396/6000.0.47f1/Pipeline7800 확인. 현재 Edit/clean/backgroundfalse/compilingfalse 복원 확인. Scene·Prefab·Material·ClientCore DLL 변경 없음.
- 이번 배치에서 기존 서버/로그인 서비스를 정상 시작·종료했으며 사용자 AI는 수정하지 않았다. 공개 자료는 CAND Markdown에서 생성하고 실제 캡처3개를 추가했다.
- 상세 근거·과거 결과: [RESULTS](history/2026-10-24-client-playability/RESULTS.txt). 목표·기획/아트 선택: [BRIEF](BRIEF.md).

## Slack·게시·다음 행동

CLIENTUI01 승인 확인: 채널C0C9VFZMV43/부모1792915030.606889/요청1792918308.079019/지정 사용자U0C9QEUHPMK/회신1792918532.280879의 원문을 앱으로 대조했다. 후보595f822와 이후 기록만 포함한 큰 PR 하나의 정상 push·게시 승인을 이번에 소비한다. 병합·배포는 제외한다. 기존 PLAYSESSION01 승인을 재사용하지 않는다.

PR53은 master7289a73에 병합됐다. 이번 codex/client-ui-interaction은 그 이후29파일의 UI·워크플로우·결과 자료 묶음이며 승인 이후 제품 변경은 없다. 다음 행동은 게시와 사이트 CI 확인이다.

후속 읽기 전용 조사에서 새 확정 클라이언트 결함/필수 누락은 없었다. 실제 보유 데이터 조건의 거래 차감은 미검증이며 DB를 채우지 않는다. 새 기획·아트 선택 대기는 없다. 입력 허용은 유지하며 통과한 테스트는 반복하지 않는다.
