# 방향키·보간·시각 예측 작업 카드

사용자 재개 요청에 따른 첫 플레이 조작 묶음. 브랜치 codex/classic-mmorpg-motion, 기준8690fd2 이후 계획/운영 문서 포함. 미게시 거래소42dc·catalog848는 보존하며 다른 기능을 끼워 넣지 않는다.

- 소유권: motion_own은 C02WorldScreen/C05CombatScreen/C16OwnMotionTests, motion_remote는 C04ObservedWorld/C16RemoteMotionTests. Coordinator만 root Editor 통합·조작·문서/커밋/게시를 수행한다. motion_contract는 읽기 전용 분석·독립 리뷰. Scene/Prefab/Material/ClientCore DLL·wire 변경 없음.
- 목표: 방향키 지속 입력/해제, 카메라 기준 정규화, ZXC3슬롯; 확인된 좌표 기반 최대1unit/50ms 요청; 별도 자기 화면 위치의 제한된 예측·연속 보정; 다른 개체 수신 시각 보간/외삽 없음. HP0/Zone/재접속/중단 시 상태 정리. 요청 원점·HUD·스킬·Zone 판정을 예측 위치에서 만들지 않는다.
- 검증: 동결/독립 리뷰 후 MotionRevision 직접 영향 자동2개(자기 입력/예측/취소 + 원격 보간/서버 상태/제거), 가능하면 실제 로그인1회에서 방향키→해제→ZXC→UI입력 차단→퇴장을 묶는다. 이전 click-to-walk 전제의 C04 실입력 테스트를 그대로 실행하지 않는다. Play테스트 시작0개는 통과가 아니며 실제 test_status를 확인한다.
- 제외: 맵/Cell/AI 속도와 외형·진입 UI/I가방/B거래소는 후속 작업 카드이며 현재 첫 묶음에서 완료했다고 하지 않는다. 새 리깅은 작업 범위에서 제외한다.
- 종료: 새 결함 없이 최소 검증을 통과하면 반복 회귀를 중단한다. 미실행 실입력은 이유와 남은 배치로 기록한다. PR은 후속 사용자 기능 묶음과 통합하며 사전Slack게시승인을 유지한다.