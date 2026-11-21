# 현재 인계 — 공용 CSV 연결, 로컬 완료·게시 준비

## 현재 위치
- ROOT: C:/Users/joeun/Desktop/workspace/Game-UnityClient
- Git 정본: ROOT/UnityClient/Builds/ClientGrowthHUD/source, 브랜치 codex/shared-csv-content.
- origin/master ff446b3을 병합한 2093bec 위에서 작업했다. 이전 클라이언트·웹 후보 86a056c도 보존한다. push·PR·배포는 아직 하지 않았다.
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
- 관련 변경을 하나로 커밋하고 Slack CSVSHARE01 게시 승인을 요청한다. 승인 전 push·새 PR은 보류한다.
- 미등록 기존 아이템 33은 DB 정리나 지급 정책을 임의 변경하지 않는다.
- unrelated site/media/releases/2026-11-13/ 미추적 자료는 보존하고 이번 커밋에서 제외한다.

## 게시 승인 대기 — CSVSHARE01
- 로컬 구현 커밋 12e885c. Unity Coordinator 앱으로 채널 C0C9VFZMV43의 새 부모 1795275182.579429에 게시 승인 요청을 보냈고 threads.txt 등록을 확인했다.
- 요청 링크: https://aiagent-vqu1541.slack.com/archives/C0C9VFZMV43/p1795275182579429
- 승인 회신은 아직 수신·적용하지 않았다. push·PR은 보류한다.
- 수신 health는 2026-11-14 값으로 오래됐다. 기존 Start-SlackReceiver.ps1 재시작이 Windows 실행 정책 UnauthorizedAccess로 차단됐다. 정책 변경·우회는 하지 않았다. 발신은 정상 성공했다.
- 자동 수신을 정상 복구하거나 사용자가 이 채팅에서 이번 게시를 승인한 뒤에만 게시한다. 복구 시 정확한 지정 사용자·요청·스레드·회신 ID를 대조하고 기소비 여부를 기록한다.
