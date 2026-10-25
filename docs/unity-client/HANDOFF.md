# 현재 인계 — 2026-10-25

## 현재 작업

마을·필드와 플레이 마감을 새 큰 PR 하나로 준비했다. 기존 선택인 기사 형상·스킬별 모션, 성곽길A, 가방A, 간소화 거래소B를 유지한다. PR52는 master2e0de28에 병합됐고 이전 게시 승인 CLIENTART02는 소비 완료다. PLAYSESSION01의 지정 사용자 Slack 승인을 대조했고 이 묶음의 일반 push·PR 게시만 진행한다.

- 완료: 이동이 미소비 시전 스냅샷을 덮지 않도록 보존, 거래소 수령 중 닫기/시간초과 후 가방 갱신, 가방 앞순서 보완. 독립 읽기 리뷰, 관련 자동3개, ClientCore·Unity 컴파일 통과. 이미 통과한 전체 테스트는 반복하지 않았다.
- 완료: 길·식생 재질 조정, 기존 메시2개로 마을 포장을 사냥터 진입 경계까지 연결. 확장 전 북/동 원경2개가 실제 길을 가리던 문제를 숨김으로 해결했다. 통행·충돌·스폰·서버·프로토콜·경제·DB·사용자 몬스터 AI는 변경하지 않았다. 레벨 동선은 [BRIEF](BRIEF.md), 상세 근거는 [RESULTS](history/2026-10-24-client-playability/RESULTS.txt).
- 실제 확인: 로그인/캐릭터 선택/필드, 입력 시스템 키 이벤트로 이동+C 준비·타격/MP20 감소/정지 후 드리프트0, 버튼으로 빈 가방·거래소 목록. I/B native 이벤트는 이번 배치에서 열리지 않아 원인 미확인·미검증이다. 가방 앞순서 수정 후 실입력 재열기는 하지 않았다. 재화--/빈 가방이므로 실거래 성공·차감, 연장 길 전체 실제 왕복, 최신 사용자 AI 평가는 미확인이다.
- 입력 허용 유지. 새로운 방향·서버 기능을 임의로 늘리지 않는다. 게시 승인 대기 중 독립 로컬 작업은 가능하지만 이미 통과한 검증을 반복하지 않는다.

## 정본과 실행

- Git: C:/Users/joeun/Desktop/workspace/Game-UnityClient/UnityClient/Builds/C10-Delivery/source, codex/client-play-session-polish. origin/master2e0de28에서 분기.
- Editor: C:/Users/joeun/Desktop/workspace/Game-UnityClient/UnityClient, PID38396/6000.0.47f1/Pipeline7800. 마지막 확인 Edit/clean/backgroundfalse/컴파일 오류0. CAND ClientCore DLL과 두 plugin 동일, ROOT의 오래된 ClientCore 원본은 수정하지 않았다.
- 이번에 시작한 MainServer는 소유 PTY에서 q 종료/exit0, Game/Login 서비스는 소유 PTY Ctrl+C로 종료.3000/9999 리스너 없음, 원래 꺼진 상태로 복원. Redis/MySQL 유지. Play-Economy 서버의 최신 사용자 AI 포함 여부는 미확인이다.
- 실행·촬영 원본: ROOT/UnityClient/Builds/Play-Session. 공개 자료는 CAND Markdown에서 생성, 로컬 미리보기 http://127.0.0.1:8791/MMORPG/ (preview session70047). 웹 브라우저 연결은 기존 오류 탭 URL 정책에 차단돼 다른 브라우저 우회 없이 중단; 사이트 생성·내부 링크 검사는 통과하고 이미지 파일은 별도 시각 확인했다.

## 게시와 Slack

새 묶음 부모1792903229.965669, Unity Coordinator 앱. 수신기 PID39232/listening/고정 Coordinator/등록 스레드 확인. 후보11335f6(제품·자료19파일)의 PLAYSESSION01 게시 승인을 요청했다. 요청1792904955.846829, 부모1792903229.965669, bot U0C9ZRTQ6F8를 읽기로 확인했다. 지정 사용자 U0C9QEUHPMK의 회신1792909726.009609(승인 PLAYSESSION01)을 같은 부모에서 직접 읽어 요청·후보·현재 대기·기소비 없음과 대조, 1회 적용해 게시 진행 중이다. 제품 후보11335f6 이후는 인계/승인 메타데이터만 바뀌었다. 병합·Pages 배포는 승인 범위에 포함되지 않는다. 이전 승인이나 단순 수신확인을 새 승인으로 사용하지 않는다.
