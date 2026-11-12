# 현재 인계 — 월드 가독성

- 이전 월드 작업은 [PR #76](https://github.com/JoEunil/MMORPG/pull/76)으로 병합됐다. 기본 BAT의 Unity·MainServer를 병합된 WorldZoneLayout 빌드로 전환했고, 실제 Zone 2→3 이동에서 안내 유지·도착 종료와 정상 종료를 확인했다. 근거는 내부 Builds/WorldZoneLayout에 있다.
- 새 후보는 정본의 `codex/world-readability`에 있다. 사냥터 목적지 라벨을 핀 왼쪽으로 정렬했고, 기존 석재 재질을 사용해 구역 통로 7곳과 내부 통로 10곳에 표주 24개를 배치했다. 통행 격자와 서버 생성물은 변경하지 않았다.
- 실제 Unity 씬의 지도 배치와 라벨·핀 선택을 1280×720, 960×540 에디터 검증으로 확인했다. Windows 빌드와 공개 사이트 빌드도 통과했다. 새 후보의 실서버 플레이를 반복하지 않았다. 에디터 캡처와 검증 로그는 내부 Builds/WorldReadability에 있다.
- 게임과 같은 한글 폰트로 지도 캡처를 렌더링해 목적지 5개 가독성을 확인했고 종합 검수 통과. 새 PR 게시 승인 요청을 준비한다.
