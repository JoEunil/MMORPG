# 현재 인계 — 전체 월드 지도

- 정본은 Game-UnityClient/UnityClient/Builds/ClientGrowthHUD/source의 codex/world-map-overview다. 기준은 PR #74를 병합한 origin/master 4b68006, 검수된 제품 후보는 6c75eb6이다.
- 미니맵 클릭으로 북쪽이 위인 전체 지도를 연다. 기존 지형과 구역 자료로 여섯 구역, 권장 레벨, 현재 구역과 내 위치를 표시한다. 서버·ClientCore·맵 충돌·몬스터 AI 변경은 없다.
- PlayMode 1건, Windows 빌드와 사이트 생성 통과. 로컬 서버의 마을에서 클릭으로 열기, 현재 위치, 닫기, 재열기를 확인했다. Esc와 지도 위 키보드 이동 차단은 실제 키로 미확인이다. 종합 검수 6c75eb6 통과, 필수 수정 없음.
- 기본 로컬 런처는 Builds/WorldMapOverview/Player를 실행한다. 기존 게임·서버는 정상 종료하고 새 빌드를 시작했으며 검증 마지막 관찰은 마을에 접속해 지도를 열어둔 상태다. 상세 근거는 비공개 Builds/WorldMapOverview/live-verification.json·test-results.xml·windows-build.json이다.
- PR #74 미니맵 방향과 마을 환경 작업은 병합됐다. 이전 결과는 [지도 작업 이력](history/2026-11-08-map-presentation/README.md)에 있다. 포션 공급·드롭과 사용자 몬스터 AI는 기존 보류·보호 범위를 따른다.
- 2026-11-09 사용자 대화에서 MAPOVERVIEW01 게시 승인 확인. 제품 후보 6c75eb6 및 관련 기록을 단일 PR로 게시한다. Slack 요청 부모 1794207563.949579.
