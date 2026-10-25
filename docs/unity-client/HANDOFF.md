# 현재 인계 — 2026-10-25

## 목표·정본
기존 서버·사용자 몬스터 AI를 보존하고 클라이언트 후속을 큰 기능 묶음으로 모은다. Git 정본 C:/Users/joeun/Desktop/workspace/Game-UnityClient/UnityClient/Builds/C10-Delivery/source, 현재 codex/nearby-player-feedback. Editor ROOT/UnityClient, Unity6000.0.47f1/Pipeline7800. ROOT=C:/Users/joeun/Desktop/workspace/Game-UnityClient. 제품 수정은 두 경로 동기화한다.

## 게시 완료
사용자 현재 채팅의 '게시해줘'로 HUDWALLET01 승인·소비 완료. PR54 codex/client-ui-interaction head bac95593adaa30b984386134cc849d4d5ad9bb82로 정상 push·제목/설명 갱신. Actions40182347847 success(사이트 PR 빌드, 정식배포 아님). 병합 안 함. 요청 부모1792920540.953299/요청1792921603.305979, 앱 처리회신1792921902.113769. Slack 사람 회신을 받은 것으로 기록하지 않는다. 이전 HUNTFEEDBACK01 회신1792920766.249229는 이미 소비 완료.

PR54에는 패널/채팅 조작, 스킬 안내/몬스터 쓰러짐, HP·레벨·EXP 강조, 제한된 잔액 조회 복구, 공개 실제캡처가 포함된다. 직접영향 자동검증·실제서버 합성입력 근거는 RESULTS에 보존. 성공거래/실제재화차감은 보유데이터 부족으로 미검증; 재시딩하지 않는다. 기존 테스트 반복 금지.

## 현재 미게시 후속
주변 플레이어 체력 표시: C04 기존 PlayerObservation에 C13EntityVitals 재사용. 최초Full 기준표시만, 기존Full/Delta HP감소만 강조, HP0에서도 플레이어 유지. 기존 AOI/구역/종료 수명 유지. 서버·AI·패킷·DLL·C13/에셋 변경 없음.
독립리뷰 차단없음, 컴파일0오류/경고, 신규NearbyPlayerVitals1사례 통과(.087s), ROOT/CAND2파일 동기화. 임시PreviewScene 실제기존prefab 1280x720 배치/피격색 확인. 예시HP·촬영조명이며 실제 다중접속 검증 아님. 원본 ROOT/UnityClient/Builds/Play-Session/nearby-player-vitals-preview.png. 실제 다중세션은 미실행.

## 입력·다음 행동
실제 입력 허용 유지. 현재 Editor Edit/clean/backgroundfalse; 직전 owned Main q종료0/LoginCtrlC종료1, DB/Redis 유지. 후속은 미게시 로컬 누적하며 이 작은 변경만 새 PR/승인으로 나누지 않는다. 관련 클라이언트 묶음과 실제 관찰 조건을 더 모아 게시 직전 Slack 승인 요청. 현재 새 승인/취향 선택 대기 없음. 후속 앱 부모1792922124.815219(자동 등록 완료). 새 작업 묶음은 새 앱 부모, 같은 묶음은 기존 스레드.
로그인/진입 후속조사는 현재 계약 내 정당한 단독변경 없음. 빈목록/조회실패 모호성을 임의 재시도로 감추지 않는다.

상세 [RESULTS](history/2026-10-24-client-playability/RESULTS.txt), 범위 [BRIEF](BRIEF.md).
