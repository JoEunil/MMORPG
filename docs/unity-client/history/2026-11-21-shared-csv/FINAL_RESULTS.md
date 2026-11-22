# CSV 클라이언트 최종 확인 — 2026-11-22

대상: PR #87의 CSV Windows 플레이어와 같은 리팩터링 MainServer, test1 캐릭터 11 / Zone 6. 최종 후속 변경은 문서뿐이며 실행 코드는 변경하지 않았다.

| 확인 | 실제 결과 |
| --- | --- |
| 가방·카탈로그 | 8개 행 표시, CSV 품목과 기존 미등록 ID 33 구분 |
| 철검 장착·해제 | 수량 1 유지, 공격력 11→13→11, 원래 미장착 상태 복원 |
| HP 포션 | HP 9,210→10,710, 수량 20→19, 6초 재사용 표시 |
| MP 포션 | MP 9,360→9,860, 수량 2→1, 6초 재사용 표시 |
| 거래소 | 판매 목록 30개 조회, HP 포션 선택 시 판매가 100 다이아·골드 수수료 10 표시 |
| R 스킬 | 채워진 전방 예고·차징 표시, MP 9,860→9,700 |
| 사용자 실제 확인 | 거래소·스킬·아이템 사용·몬스터 상호작용 모두 확인했다고 현재 채팅에서 보고 |
| 종료 | 관리 런처로 Unity·MainServer·LoginServer 정상 종료, Phase=stopped |

거래 등록·구매를 자동 재실행하거나 DB를 초기화하지 않았다. 포션 각 1개는 정상 게임 사용으로 소비됐으며 임의 보충하지 않았다. 자동 press_key d로는 좌표 변화가 없어 자동 이동 성공으로 기록하지 않는다. 사용자가 확인한 범위를 자동 도구 결과로 바꾸지 않는다.

Windows의 TCP 예약 범위에 기존 포트 10099가 들어 서버 bind가 실패했다. Windows 설정 변경 없이 테스트 포트를 19999로 바꿔 실행했다. 바탕화면 BAT가 호출하는 ROOT/tools/local-game/settings.json을 같은 검증 플레이어·서버와 포트로 갱신했고 check 명령으로 경로·Node를 확인했다. 이전 설정은 ROOT/UnityClient/Builds/CsvClientIntegration/default-settings-before-final.json에 보존했다.

로컬 상세 근거: ROOT/UnityClient/Builds/CsvClientIntegration/final-player-results.txt 및 Live/UnityClient/Builds/LocalGame/Run/state.json. 기존 빌드·CSV 15개 해시·관련 회귀 검사는 통과 결과를 재사용했고 반복하지 않았다.

확률 드롭·보스 시간표는 현재·향후 작업 계획에서 제외한다. 새 기획은 추가하지 않으며 현재 요청의 구현·검증 작업을 마감한다. PR 병합은 사용자 검토에 맡긴다. Slack 수신기는 이전 Windows 정책 차단으로 미복구이며 게임 검증 실패와 구분한다.
