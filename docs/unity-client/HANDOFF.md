# 현재 인계 — CSV 클라이언트 마감 완료

## 정본·게시
- ROOT: C:/Users/joeun/Desktop/workspace/Game-UnityClient
- Git 정본: ROOT/UnityClient/Builds/ClientGrowthHUD/source, 브랜치 codex/shared-csv-content.
- PR #87: https://github.com/JoEunil/MMORPG/pull/87 — 게시 완료, 병합은 사용자 검토에 맡긴다.
- CSVSHARE01은 현재 채팅의 “확인 PR 올려줘” 승인으로 게시했다. Slack 부모 1795275182.579429, 채널 C0C9VFZMV43. 같은 승인을 중복 적용하지 않는다.

## 완료
- 서버·Unity가 같은 CSV 15개를 직접 읽으며 숫자 데이터 exporter/importer를 제거했다. 맵·충돌·구역·스폰 배치 exporter는 유지한다.
- 임의 ID의 몬스터·장비 표시 및 ClientCore 호환을 맞췄다. 서버 코드·사용자 몬스터 AI를 직접 수정하지 않았다.
- ClientCore·MainServer·Windows 빌드, 관련 회귀 7개, 배포 CSV·DLL 일치, 실서버 조회, 독립 검수와 사이트 생성 통과. 통과 검사는 반복하지 않았다.
- 최신 Windows 실행판에서 가방·철검 장착/해제·HP/MP 회복과 수량·거래소 목록/수수료·R 예고/차징/MP 차감을 관찰했다. 사용자는 거래소·스킬·아이템 사용·몬스터 상호작용을 모두 직접 확인했다고 보고했다.
- 상세 수치·관찰/사용자 확인 구분·제한: [최종 결과](history/2026-11-21-shared-csv/FINAL_RESULTS.md). 이전 인계는 같은 이력 폴더에 보존한다.

## 실행·범위
- 게임·MainServer·LoginServer는 모두 정상 종료했다. 테스트 상태 Live/UnityClient/Builds/LocalGame/Run/state.json의 Phase=stopped.
- 기본 바탕화면 BAT의 ROOT/tools/local-game/settings.json은 검증된 CsvClientIntegration/Player와 ClientGrowthHUD/source/x64/Release/MainServer.exe를 가리킨다. Windows가 기존 10099를 예약해 테스트 포트만 19999로 변경했다. 경로·Node 사전 확인 통과.
- 테스트 포션 HP/MP 각 1개를 정상 소비했다. 장착 상태는 원래대로 해제했다. 미등록 기존 아이템 33은 그대로 보존한다. DB 초기화·임의 보충 없음.
- 확률 드롭·보스 시간표는 현재·향후 계획에서 제외한다. 새 기획을 추가하지 않으며 이번 개발·검증 묶음은 마감한다.
- Slack 발신은 정상이다. 자동 수신기 재시작은 Windows 실행 정책에 차단돼 미복구다. 정책 우회 없이 남겨둔 운영 제한이며 완료된 게임 검증과 구분한다.
- unrelated site/media/releases/2026-11-13/ 미추적 자료는 보존·커밋 제외한다. 다음 행동은 필요한 PR 리뷰 대응이며 게임 개발을 자동 재개하지 않는다.
