# 현재 인계 — 마을·사냥터 지도와 필드 길안내

- 정본은 Game-UnityClient/UnityClient/Builds/ClientGrowthHUD/source의 codex/town-route-map이다. 기준은 PR #75를 병합한 origin/master 2e57b7f다.
- 전체 지도에 실제 외곽 성곽·남쪽 성문·동쪽 포장길을 표시한다. 마을 시작점과 첫 사냥터 진입을 누르면 기존 통행 격자로 경로를 계산하며, 지도를 닫으면 필드에 가까운 화살표와 목적지·남은 거리를 표시한다. 이동 중 서버 좌표로 안내를 갱신한다. 필드 카메라의 기존 기울기와 거리를 유지하면서 북쪽을 위로 맞췄고, 같은 길에서 남은 화살표 위치는 목적지 기준으로 고정한다. 목표 구역에서 도달 가능한 목적지 1m 안으로 들어오면 화살표·거리·지도 선택을 함께 지운다. 필드 HUD의 안내 끄기도 같은 상태를 지운다.
- 실제 씬에서 성벽을 막힌 것으로 판정하고 우회 경로의 전 구간, 필드 화살표·HUD 표시, 위치 갱신과 안내 숨김·재개·해제를 검사했다. Windows 빌드와 사이트 생성도 통과했다. 새 Windows 게임의 실서버 마을에서 첫 사냥터 선택 후 지도를 닫아 바닥 화살표와 목적지·135 m 표시를 확인했다. 도착 자동 종료는 실제 씬의 좌표 입력 검사로 확인했으며, 실서버에서 직접 목적지까지 걷는 검증은 미확인이다.
- 다음 로컬 런처 실행은 Builds/RouteCancel/Player를 사용한다. Unity 씬에서 취소·경로 재등장 방지·다른 목표 재선택과 Windows 빌드 통과. 근거는 Builds/RouteCancel/scene-check.log·windows-build.json이다. 이 취소 버튼의 독립 실행 실입력은 미확인이다. 코드 리뷰 필수 수정 없음. 도착 자동 종료의 씬 검사와 Windows 빌드 근거는 Builds/RouteArrival/scene-check.log·windows-build.json이다. 북쪽 투영과 같은 경로 위 화살표 고정 PlayMode 2건 및 Windows 빌드 통과. 실제 입력으로 지도·필드 방향이 일치하고 남은 거리가 87m에서 70m로 감소했음을 확인했다. 실제 화면만으로 개별 화살표의 좌표를 대조하거나 도착까지 검증하지는 않았다. 근거는 Builds/NorthUpFieldRoute/test-results.xml·windows-build.json·live-verification.json과 Builds/FieldRouteGuide/scene-check.log다.
- 적용 범위는 마을과 첫 사냥터 목표다. 서버·씬·통행 원본·몬스터 AI 변경은 없다. 2026-11-09 사용자 대화에서 성곽·통로 표시와 필드 길안내, 도착·취소를 한 PR로 게시하도록 승인했다. 제품 후보는 47a7cff다.
- 후속 계획: Zone 2부터 [사냥터 지형과 통로](BRIEF.md#후속-레벨-디자인--사냥터-지형과-통로)를 설계한다. 이번에는 계획만 반영했다.
