# 현재 인계 — 공용 CSV 연결, PR #87 게시 완료

## 현재 위치
- ROOT: C:/Users/joeun/Desktop/workspace/Game-UnityClient
- Git 정본: ROOT/UnityClient/Builds/ClientGrowthHUD/source, 브랜치 codex/shared-csv-content.
- origin/master ff446b3을 병합한 2093bec 위에서 작업했다. 이전 클라이언트·웹 후보 86a056c도 보존한다. 작업 브랜치 push·PR #87 게시를 완료했다. 배포·병합은 하지 않았다.
- 과거 인계: history/2026-11-21-shared-csv/HANDOFF-before-csv.md. 과거 승인을 이번 CSV 범위에 재사용하지 않는다.

## 완료 범위
- 서버와 Unity가 최상위 Data/의 동일한 스킬·몬스터·성장·아이템 CSV 15개를 읽는다. 전체 검증 후 기존 ServerContent를 갱신한다.
- 숫자 콘텐츠 C++ exporter·JSON·Unity importer를 제거했다. 에디터 재로드와 데스크톱 빌드의 원본 CSV 복사를 연결했다.
- CSV monsterType·equipmentSlot을 사용해 임의 ID 몬스터 표시·지도 대표 선택·장착·설명·판매 제한을 맞췄다. ClientCore 고정 장비 ID 제한은 제거하고 응답 상관성 검증은 유지했다.
- upstream 로비 복귀 처리를 확인했다. 사용자 몬스터 AI·서버 게임 로직은 추가 수정하지 않았다.
- 맵·스폰은 서버가 CSV로 읽지 않아 기존 Unity 베이크를 유지한다. 패링은 서버의 고정 5틱·60틱·1회 계약을 유지한다.

## 실제 검증
- CLI 1.0.1, ClientCore·MainServer·Windows 플레이어 빌드 통과. CSV 관련 핵심 회귀 7개 통과. 통과한 검사는 반복하지 않았다.
- 원본·서버 실행 폴더·Windows 플레이어의 CSV 15개 해시 일치, 배포 ClientCore DLL 일치.
- 독립 종합 검수 통과. 공개 사이트 생성 및 미디어 필수 검사 통과.
- 최신 서버와 실제 로그인→Zone 6 진입→가방 조회 통과(읽기만 수행). 등록된 아이템 7종 확인. 기존 DB의 미등록 ID 33은 알 수 없는 항목으로 보존하며 장착하지 않는다.
- 최초 라이브 검사에서 모든 DB 아이템이 현재 카탈로그에 있어야 한다는 검사 가정을 바로잡고 해당 경로만 다시 통과했다.
- 독립 실행판의 직접 키보드 조작은 이번에 수행하지 않았다. 라이브 확인은 Unity PlayMode와 실제 서버 연결이다.
- 근거: ROOT/UnityClient/Builds/CsvClientIntegration/{player-build.json,csv-manifest.json,live-final.xml,live-result.txt,csv-focused.xml,fixed-tests.xml,clientcore-build.log,server-serial-build.log}.
- 관리 런처가 게임·MainServer·LoginServer를 모두 정상 종료했음을 status Phase=stopped로 확인했다. 임시 라이브 검사 파일은 프로젝트에서 제거해 근거 폴더로 보존했다.

## 다음 행동·게시 경계
- 사용자의 현재 채팅 승인을 적용해 PR #87로 게시했다. 병합은 사용자 검토에 맡긴다.
- 미등록 기존 아이템 33은 DB 정리나 지급 정책을 임의 변경하지 않는다.
- unrelated site/media/releases/2026-11-13/ 미추적 자료는 보존하고 이번 커밋에서 제외한다.

## 게시 승인 적용 — CSVSHARE01
- 로컬 구현 커밋 12e885c. Unity Coordinator 앱으로 채널 C0C9VFZMV43의 새 부모 1795275182.579429에 게시 승인 요청을 보냈고 threads.txt 등록을 확인했다.
- 요청 링크: https://aiagent-vqu1541.slack.com/archives/C0C9VFZMV43/p1795275182579429
- 사용자가 현재 채팅에서 공용 CSV와 맵 exporter 유지 범위를 확인한 뒤 “확인 PR 올려줘”로 게시를 명시 승인했다. 이 승인으로 현재 묶음의 push·PR 게시를 진행하며 병합은 사용자 검토에 맡긴다.
- 수신 health는 2026-11-14 값으로 오래됐다. 기존 Start-SlackReceiver.ps1 재시작이 Windows 실행 정책 UnauthorizedAccess로 차단됐다. 정책 변경·우회는 하지 않았다. 발신은 정상 성공했다.
- Slack 자동 수신은 여전히 미복구 상태다. 이번 게시 승인은 현재 채팅에서 직접 확인했으며 중복 적용하지 않는다.

- 게시 직전 origin/master의 f600d70(서버 리팩터링 설명 문서만 변경)을 병합했다. 검증 대상 런타임 코드는 바뀌지 않아 통과 검사는 반복하지 않는다.
- 게시 결과: https://github.com/JoEunil/MMORPG/pull/87 (open, master 대상). 다음 행동은 사용자 PR 검토와 해당 리뷰 대응이다.

## 최종 마감 요청 — 2026-11-22
- 사용자는 마지막 조작 확인만 수행하고 마감하도록 요청했다. 확률 드롭·보스 시간표를 현재·향후 계획에서 제외하며 새 기획을 추가하지 않는다. 현재 기획 정본은 BRIEF의 현재 작업 범위 확정이다. 과거 기록은 삭제하지 않는다.
- 같은 CSV Windows 빌드와 서버를 기동했다. 기존 테스트 포트 10099는 Windows TCP 제외 범위 10068~10167에 들어 bind 실패했다. Windows 설정은 변경하지 않고 격리 런처 GamePort를 19999로 변경했으며 MainServer·LoginServer·게임이 정상 시작됐다.
- 최신 런처 설정: ROOT/UnityClient/Builds/CsvClientIntegration/Live/tools/local-game/settings.json. 실행 상태: 같은 Live/UnityClient/Builds/LocalGame/Run/state.json.
- 실제 게임 로그인 화면을 확인했고 사용자에게 test1 직접 로그인을 요청한 상태다. 입력 도구의 인증 화면 자동 조작 금지를 준수한다. 로그인 전 직접 조작 검증을 완료로 기록하지 않는다.
- 남은 한 배치: 독립 실행판 가방·장비·거래소 화면/상태, 가능한 이동·전투·포션 조작 확인, 종료. 이미 통과한 빌드·자동 검사·실서버 조회를 반복하지 않는다.
- 최종 검증 후 기본 BAT 설정 최신화 여부를 결정하고 이번 문서·결과를 PR #87에 반영한다. 아직 마감 완료가 아니다.
