# 현재 인계 — 첫 사냥터 지형·스폰

- 길안내 PR [#76](https://github.com/JoEunil/MMORPG/pull/76)은 사용자 승인으로 게시했다. 제품 후보 47a7cff, 게시 기준 624d56d. 지도 성곽·통로, 필드 화살표·거리, 북쪽 정렬, 도착·취소가 범위다. 실제 이동에서 거리 감소와 방향을 확인했고 도착·취소는 Unity 씬 검사로 확인했다.
- 후속 정본은 Game-UnityClient/UnityClient/Builds/ClientGrowthHUD/source의 codex/zone2-terrain-spawns다. 아직 병합 전인 PR #76의 브랜치 끝 624d56d에 기반한다. [후속 레벨 디자인](BRIEF.md#후속-레벨-디자인--사냥터-지형과-통로)을 Zone 2부터 적용한다.
- 남쪽 진입로와 안쪽 사냥 공간을 바위 능선으로 나누고 서쪽·동쪽 길을 열었다. 기존 마을 성곽 연결과 마을에서 이어지는 포장길을 유지한다. 실제 씬에서 두 통로, 능선 직진 차단과 양 끝의 빈틈을 검사했다.
- 구역별 스폰 셀을 Unity에서 저작할 수 있다. Zone 2 남쪽 두 행을 초기 스폰에서 비우고 일반 108·강화 12를 안쪽에 배치했다. 다른 구역의 배열과 총수, 보스 배치, 몬스터 AI는 유지한다.
- Unity 통행 격자와 서버 생성물 250,100타일 일치, 모든 점유 스폰 셀의 유효 좌표 확인, 스폰 PlayMode 1건 및 Windows 빌드 통과. 상세 근거는 비공개 Builds/Zone2Terrain에 있다.
- 서버 스폰 통합 검사 `SpawnLayoutTest.StarterLayoutLeavesEntryCellsEmptyAndBuildsAllServerReferences` 1건 통과. 새 지형의 Windows·MainServer 빌드와 최신 스폰 검사 양쪽 1건씩 통과. 별도 검증 런처에서 새 서버·로그인 서버·게임 시작을 확인했다. 기본 런처는 이전 빌드 경로다. 후속 변경으로 사냥터 표식을 추가했다. Unity 실제 씬에서 마을 시작점→동쪽 통로→사냥터 목적지의 7개 경로 점과 필드 화살표 메시를 확인했다. 종합 검수에서 코드·데이터 차단점은 없었고 실제 게임 캡처·통로 확인이 필요하다. 테스트 게임 로그인 대기이며 PR #76 추가 게시 전이다.
