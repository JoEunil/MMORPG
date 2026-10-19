# C05 — 전투·HP0 처리 / V03 외곽 / Slack 단계 재개

사용자 지시: 전투·HP0 구현과 실제 검증, 필드 외곽 아트 보완. 입력 검증은 구현·리뷰·아트 동결 후 배치로 실행한다. 단계 완료는 Unity Coordinator 앱의 변경별 Slack 스레드에 보고하고 지정 사용자 재개 신호를 수신한다. 현재 승인된 C05 진행을 임의 단계 승인 게이트로 바꾸지 않으며, 보호영역 변경이나 사용자 대기 지시가 있을 때만 해당 의존 작업을 기다린다.

## 계약·범위

C05_CONTRACT.md의 기존 ACTION/ActionResult/Full/Delta와 슬롯0/1/2만 사용한다. 전투 입력 키1/2/3와 실제 클릭 가능한 슬롯 버튼, 서버 확정 HP/MP 및 결과를 표시한다. 위치·dir·스킬을 기존 ViewData lock에서 원자 기록하는 최소 호환 API와 ActionResult lifetime/subscriber snapshot만 Core에 추가한다. 피해 수치·쿨다운 성공·처치·부활을 클라이언트가 임의 확정하지 않는다. 선/원 범위의 실제 서버 방향 차이를 유지하며 조준 방향 표시가 필요하면 실제 서버 의미대로 제공한다. 새 서버/wire/DB/계정/재화 변경은 현재 필요 없다.

본인 HP0는 서버 수치0/클라이언트 이동·전투 입력 중지/연결 해제만 제공한다. 서버 부활 기능이 없으므로 부활 UI를 만들지 않는다. 몬스터 HP0 숨김과 살아있는 Full 재표시는 C04 경계를 재사용한다. 이름조회·타겟 잠금·인벤토리/아이템·새 AI·보상 연출은 제외한다.

## 소유권·단계

- c05_contract: C05_CONTRACT.md만, 완료.
- c05_field_art: Art/V03·C02SceneBuilder.cs·C02.unity·V03_TASK.md 및 관련meta/README. Editor Editmode 단독 소유, Play/입력 금지. V02/C04 모델 보존. Zone1[-5,105] 및 카메라 외곽 지면 노출을 없애며 이동 picking collider/서버 경계는 변경하지 않는다.
- 구현 담당: C05CombatScreen.cs/meta, C02WorldScreen.cs의 최소 확정상태 공개/HP0 이동 차단·기존 이동목표 취소 연결, ClientCore Interfaces/ViewData/ViewModel의 위 최소 변경·DLL, 관련 fixture. Builder/Scene은 아트 동결·명시 인계 후에만 C05 UI/binding 작성. 다른 소유 파일을 덮어쓰지 않는다.
- 총괄: 작업 카드/판단/Slack 발신·수신 도구와 연결 정책. 원본 Game 스크립트/비밀은 읽기만 하며 새 수신기는 이 체크아웃 Tools/SlackReceiver에 둔다.

## 입력 배치 B04

후속 실제 결과로 수정: account1 재로그인에서 fresh raw HP9975/MP10000을 받아 기존HP0 경로가 즉시 중단됐다(추가입력0). DB/HP복구를 실행하지 않았으며 앞선HP0의 재접속 영속 전제가 성립하지 않았다. 아래 '계정2 추가피격 생략'은 잠정판단 이력이다. 원래 승인된 account2 외곽 캡처→정상bossaggro→동일연결 실제HP0/입력차단/disconnect 배치로 복원한다. 재로그인으로 상태를 되돌리는 테스트를 추가 반복하지 않는다.

실제 관측 후 조정: attempt3에서 스킬키1/2/3·버튼/서버MP·Ring피해·동일ID HP0/제거·15.26초 첫alive/활성Visual까지 통과했지만, 재등장 대기 중 account1/5011도 정상 몬스터 피격으로 HP0이 됐다. 계획했던 account2 단독 HP0 유도와 다른 실제 결과이며 계정 보호 미흡을 기록한다. account2까지 추가로 죽이는 시도는 하지 않는다. 이미0인 account1을 실제 재로그인해 fresh rawHP0/UI/native입력차단·disconnect를 검증하고, account2는 생존 상태로 외곽 실제 캡처 후 정상 disconnect한다. DB/HP 복구는 하지 않으며 다음 재개 때 5011 사용 불가 상태를 먼저 확인한다. 이미0 관측을 새 사망 전이로 주장하지 않는다.

목표: 동결된 C05+V03에서 실제 전용 계정 로그인/진입→네이티브1/2/3/슬롯 버튼→서버 ActionResult·MP/몬스터HP 감소→실제 HP0/표시 제거→약15초 후 같은ID 살아있는 Full/재표시. account2/5012만 정상 보스 aggro/피격으로 본인0을 최대120초 시도하고 HP0 UI/입력 중지/disconnect를 확인한다. HP/MP는 실제 진입 응답을 확인하며 사전 주입/DB 수정하지 않는다. 정상 플레이 후 account2 HP0 영속 가능, 부활/DB 복구/reset은 자동 수행하지 않는다. 미관찰은 통과로 바꾸지 않는다.

포함: 관련 fixture·실제 서버 결과/시각 대조·UI 차단·disconnect/늦은 callback 회귀·V03 중앙/Zone 외곽 시인성 PNG·짧은 실제 GIF. 제외: DB 초기화/치트/타인접속/서비스 종료·강제 종료·공개. 비용: 관련 fixture1회, 공통 로그인120초+전투/HP0/재등장/외곽 관찰 총6분+캡처1분. 같은 원인 두 번 수정 실패면 총괄로 원문/재현을 넘기고 축소 진단한다. 통과 후 근거 없는 반복은 하지 않는다.

배치 시작부터 Assets/Packages/ProjectSettings 쓰기 금지, Editor는 테스트 담당만 조작. 이후 수정은 영향 범위 재리뷰/재검증한다. 마지막 정상 disconnect/Playstop/Recorder0/임시 입력 제거/peer0·기존 서비스 유지.

Slack C05: 1792318770.788919. W03 수신 연결:1792318774.250569. 지정 채널C0C9VFZMV43/사용자U0C9QEUHPMK/앱U0C9ZRTQ6F8. 재개 대상은 현재 Unity Client Coordinator 01a14d46-8b23-7590-8ad5-7efc20f8ecd9로 고정하며 이전 준비 수신 상태는 보존한다. 실제 사람 회신 왕복 전에는 자동 재개 검증 완료로 보고하지 않는다.

## 최종 수용

C05/V03 배치를 완료했다. 관련 PlayMode29/29, 실제 스킬/MP/몬스터HP0·15.26초재등장, 계정2 실제rawHP0 UI/버튼/키·지면입력차단/정상disconnect, 실제외곽PNG·전투GIF 검수. HP0 phase 진입시 이미0이라 그구간 새사망전이는 미관찰. 계정1 보호미흡/재접속HP차이는 원인미확인 이력으로 보존, DB복구 없음. 종합은 Captures/c05-batch-final.txt, 최종SHA는 c05-final-freeze.txt. 모든입력정리/서비스유지, 다음재개대기. 위 원래계획과잠정조정은 실제결과와구분한다.
