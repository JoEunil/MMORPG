# 현재 인계 — 2026-10-25

## 현재 작업

현재 요청: I/B 전달 원인 확인·해결, 남은 클라이언트 검증 계속. 제품 코드 결함은 아직 확인하지 못했다. 자동화 도구 전달 경로의 구체적 차이를 확인했다.

- 현재 sky.press_key i/Shift_L+i/Tab을 Unity InputEventTrace로 비교: 각각 TEXT가 관찰됐고 I 눌림 STAT은 없음. 장치는 Keyboard RawInput id1, DynamicUpdate, GameView 포커스/프레임 진행 정상. 이전 wasPressedThisFrame 0의 관찰과 일치한다. 도구 내부 OS 구현이나 물리 키 실패로 일반화하지 않는다.
- 사용자 물리 입력 완료 답변 후 I=1/B=1, frame171937 확인. 자동화 TEXT 전달과 달리 물리 키 상태는 정상이다. 로그인 화면의 입력 전달만 확인했으며 월드 패널 성공까지 주장하지 않는다. 관찰기/SessionState 제거, Edit/clean/compilingfalse/backgroundfalse 복원 완료.
- LOCAL_PREPARATION에 TEXT와 키 상태 구분 및 기존 정상 PlayerLoop QueueStateEvent 재사용 절차를 기록했다. 합성 키는 native 성공으로 보고하지 않는다. 제품에 문자 단축키 fallback을 추가하지 않았다. 서버/AI/DB/설정 변경 없음.
- next_batch_readiness가 길 전체 왕복 5관찰점과 현재 서버가 필요한 증거를 ROOT/UnityClient/Builds/Decision-Next/readiness-r2.txt에 준비했다. 새 선택 질문/독립 제품 수정 근거 없음. 물리 입력 비교 뒤 기존 실제 계정/서버 조건을 확인하고 길 왕복과 패널 조작을 묶는다. 기존 통과 테스트 반복 없음.

### 보존한 클라이언트 묶음

사용자가 PR53을 직접 검토하는 동안 다음 클라이언트 묶음을 진행하라고 지시했다. 새 로컬 브랜치 codex/client-ui-interaction에 가방·거래소 Esc 닫기와 채팅 기록 스크롤 보존을 구현했다. 기존 기사·성곽길A·가방A·간소화 거래소B 선택을 유지하며 새로운 취향 선택은 필요하지 않았다. 앞으로 필요해지면 관련 샘플을 한 번에 묶는다.

- 완료: 열린 가방/거래소에서 Esc는 기존 ClosePanel 경로를 사용한다. 기존 대기 거래 보존·불확실한 수령 후 가방 갱신·자동 재전송 금지는 유지한다. 채팅은 이전 기록을 읽을 때 스크롤 비율을 보존하고 최신 위치/자기 echo에는 아래를 따라간다. 같은 문장 고정 기능은 아니다.
- 검증: 독립 코드 리뷰 차단 없음, Unity 컴파일 통과, 새 영향 PlayMode 자동3사례만 통과(채팅2/Esc1). 입력은 InputSystem/가짜 네트워크 fixture이며 native 실입력·실거래 성공으로 보고하지 않는다. 이미 통과한 게임 테스트는 반복하지 않았다.
- 기존 미확인: I/B native 입력 도달 문제는 코드상 결함이 없어 임의 수정하지 않았다. 빈 가방/미확인 재화 때문에 성공 거래 차감·아이템 보유 실화면 미검증, 연장 길 전체 실제 왕복도 미검증이다.
- 금지: 사용자 몬스터 AI·서버·패킷·경제·DB·충돌·스폰 변경. 이번 작업은 기존 UI 안의 동작만 바꿨다. 입력 허용 유지.

## 정본과 실행

- Git: C:/Users/joeun/Desktop/workspace/Game-UnityClient/UnityClient/Builds/C10-Delivery/source, codex/client-ui-interaction. PR53 head552c5fe 기반으로 다음 작업만 로컬 누적한다. master는2e0de28, PR53은 현재 검토 중이므로 이전 브랜치/이력을 정리하거나 PR53에 새 범위를 섞지 않는다.
- Editor: C:/Users/joeun/Desktop/workspace/Game-UnityClient/UnityClient, PID38396/6000.0.47f1/Pipeline7800 확인. 현재 Edit/clean/backgroundfalse/compilingfalse 복원 확인. Scene·Prefab·Material·ClientCore DLL 변경 없음.
- 이번에 서버/로그인 서비스를 시작하지 않았다. 기존 서비스 상태와 사용자 AI를 유지했다. 공개 자료는 CAND Markdown에서 생성하며 레이아웃 변화가 없어 새 이미지/게임 촬영은 생략했다.
- 상세 근거·과거 결과: [RESULTS](history/2026-10-24-client-playability/RESULTS.txt). 목표·기획/아트 선택: [BRIEF](BRIEF.md).

## Slack·게시·다음 행동

모델·선택 입출력 정비 결과는 Unity Coordinator 앱 부모1792916173.802999와 완료 채널1792916174.647139에 남겼다. 새 게시 승인 대기는 없다.

보존한 UI 묶음은 부모1792915030.606889. 새 게시 승인은 요청/수신하지 않았다. PR53의 PLAYSESSION01 회신1792909726.009609는 PR53 게시에 소비 완료이며 이 묶음에 재사용하지 않는다.

[PR53](https://github.com/JoEunil/MMORPG/pull/53)은 사용자 검토용으로 유지한다. head552c5fe 사이트 Actions40169324854 성공/미리보기 artifact12589192115를 확인했으며 병합·정식 Pages 배포는 하지 않았다. 이번 작은 마감은 다음 큰 PR에 누적하며 별도 작은 PR을 만들지 않는다. 다음은 기존 미확인 입력 경로·아이템/거래 실데이터 조건을 필요한 범위에서 확인하고, 같은 기능 묶음의 남은 부분을 고른다. 데이터 변경·미지원 기능·새 미술 방향은 임의로 추가하지 않는다.
