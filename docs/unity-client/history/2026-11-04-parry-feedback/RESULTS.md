# 패링·전투 피드백과 창 모드 — 2026-11-04

사용자는 Space 패링의 쉬운 안(방향 제한 없음)을 선택하고 피격·예고 표현 및 쿨타임을 요청했다. 방어 창0.25초/재사용1.5초/공격1회/실패해도쿨/MP재화·몬스터경직없음으로 확정했다. 이후 실제 입력은 보류하고 빌드·자동 검사까지만 하도록 명시했다.

## 구현 범위

- 서버 정본 SkillData의 parryWindowTicks5/parryCooldownTicks30/charges1 → 기존 Exporter의parry블록 → UnityServerContent/Importer로 공유. 실제asset에5/30/1저장확인.
- PARRY48/PARRY_RES49, 요청24/응답33바이트. Zone simulation queue의틱순서에서수락하고명시적상태·남은틱을응답한다. 중복reqID재활성화/연장금지,낮은ID·오래된epoch거절.
- 실제양수피해인몬스터공격1회만방어하고같은틱다음공격은정상피해. 사망·구역전환·귀환에서방어창종료,쿨은유지. DB저장·스키마·사용자몬스터AI/스턴변경없음.
- ACK는HUD쿨상태만, 기존ActionResult의예약slot254/idUINTMAX/Begin·Success는자신·주변의모션과효과만담당. 클라입력만으로방어성공/무적을만들지않는다. 로컬timeout255와서버Unavailable6분리,자동재요청없음,새수동시도가능.
- 기존QER왼쪽Space방패슬롯·쿨오버레이. 패링입력은마우스조준없이기존텍스트/모달/포커스가드를사용한다.
- 방어성공금속색방패펄스·짧은방어자세,실제피해의빠른반응/부드러운복귀. 루트좌표·시전시간·진행중공격은보존한다. 몬스터특수공격의채워진예고대비/진행색만보강하고기본공격예고는추가하지않았다.
- 창모드1280×720/창크기조절,48px밝은커서와짙은외곽선/hotspot5,4. Unity설정API/실제importer로연결했다. ROOT로컬도우미ClientArguments는ArgumentList로전달한다.

## 검토·자동 검사

A(기존Action예약요청)/B(별도요청·응답)/C(SkillData방어스킬추가)를서버분석·Astra가비교해B를선정했다. 검수필수조건을같은계획담당에돌려반영했고ROOT미러에잘못쓴문서는같은담당자가정본에해당구간만통합했다. 독립코드리뷰차단0,최종Astra는코드·자동검사범위통과/필수0으로판정했다.

MainServer/UnitTests/ClientCore/DummyClients빌드,서버콘텐츠export·Unityimport,WindowsStandalone빌드성공. Unity headless run/test/build를사용했고실제Unity입력은하지않았다. 독립빌드는Unity의성공종료와산출물을확인했으며기존LicensingClient signature validation/ignoring진단도로그에보존했다. 라이선스/OS설정을바꾸지않았다.

직접검사총6개: 서버2(ParryCombat.OneChargeBlocksOnePositiveHitAndExpiresAtFiveTicks / NonZoneThreadPoolFixture.ParryRequestsAreCorrelatedAndTransfersClearOnlyWindow),Unity4(HUD응답/전환/timeout,방어오버레이,피격복귀/권위좌표보존,몬스터예고진행/기본제외). 모두통과. 네트워크판정·수명·세표현경로가직접영향범위라기본2~3개예산을확대했으며기존통과한사냥/거래/자유각도검사는반복하지않았다.

증거 ROOT/UnityClient/Builds/ClientGrowthHUD/verification/parry의server-tests.log/xml,unity-tests.xml(4/4),import-result.json,각build.log와release/player-build-result.json/provenance. 창모드/커서설정은verification/windowed-cursor/configure-result.json,에셋비교는verification/cursor-asset/cursor-scale-preview.png.

## 전달·미확인

바탕화면BAT는다음실행부터ROOT/UnityClient/Builds/LocalGame/Parry의동기화된Player/Server/Login묶음을사용한다. 새경로check통과,서버파일해시일치확인. 현재실행중인프로세스나기존배포폴더는교체/종료하지않았고새묶음을자동실행하지않았다. 로그인비밀은복제하지않고기존Game/Login환경만읽는다.

사용자지시에따라실제Space입력·실서버패링성공·게임HUD겹침·모션/예고가독성·실제OS커서조작감은미확인이다. fixture의핸들러호출은실입력근거가아니다. 시작/성공방송에시각·requestID가없어이미만료된후늦게온Begin의시각적구별에는한계가있지만권위방어창이나쿨을재활성화하지않는다. 실입력보류해제후한배치에서만후속확인한다. PR게시승인은입력재개승인이아니다.

Agent추가9회: 아트신규1,서버분석신규1·후속2(계획보완/정본경로교정),구현후속2,리뷰후속1,종합검수후속2. 누적614(신규232/후속382),총괄직접빌드/검사/기록은제외,토큰·비용N/A.

## 실입력 재개 — 2026-11-05

사용자 재개 요청으로 Parry 묶음 Player14512/MainServer13120/Login22912를 시작했다. 지정 test1의 정상 로그인·캐릭터11/Zone5 필드진입, 창1280×720과가시적인커서, Space슬롯/QER/포션/HP·MP의비중첩을실제화면으로확인했다. 패링버튼클릭→서버수락안내 및추정쿨~1.3표시→준비상태복귀를관찰했다. Q버튼시전의공격자세와MP8840→8839도확인했다. Player.log에실행예외는관찰되지않았다.

자동화Space키는같은반응을관찰하지못해물리키통과로판정하지않았다. 사용자에게WASD/Space/QER와몬스터실제공격대응을한배치로요청했다. 해당답변대기중에는자동입력이나게임재시작을하지않는다. 실제피해차단/피격/특수공격예고평가는아직대기다. 이구간에기존자동검사를반복하지않았다.
## 기본 공격 패링 제외 — 2026-11-05

사용자가 실제 WASD/Space/QER 입력 묶음을 완료하고 패링 표현은 보인다고 회신했다. 기본 공격은 준비 모션이 없어 패링 불가로 바꾸기를 요청했다. 이 회신은 패링 표현 관찰 근거이며 모든 모션 품질이나 실제 피해 차단 수치의 검증 완료로 확대하지 않는다.

ZoneState_Skills의 몬스터 피해 판정에서 skillId0만 패링 대상에서 제외했다. 기본 공격은 정상 피해/기존 사망 처리를 유지하고, 살아 있다면 방어창과 횟수는 소모하지 않는다. 특수 스킬만 기존0.25초창/1회/쿨1.5초로 막는다. 사용자 Monster AI·패킷·Unity 코드 변경 없음. 리뷰 담당 후속1회 차단0.

MainServer/UnitTests Release x64 빌드 및 수정된 기존 ParryCombat.OneChargeBlocksOnePositiveHitAndExpiresAtFiveTicks 1개 통과. 기본 공격 피해/창유지/성공방송없음→특수 스킬2회 중1회방어→무피해미소비/시간만료/쿨유지/사망정리를 확인했다. 기존통과검사는반복하지않았다. 처음 제한된 빌드의 작업자 연결이 지연돼 정상 취소했고 일반 실행 환경의 단일 작업자 빌드로 완료했다. 증거 ROOT/UnityClient/Builds/ClientGrowthHUD/verification/parry-basic-exclusion/build-serial.log,test.log/xml.

배포 후보는 ROOT/UnityClient/Builds/LocalGame/Parry/Server-SkillOnly에 새exe와필수DLL만 배치했다. 실행중서버의잠긴redo/backpressure복사는실패했고후보에포함하지않았으며복제된로그도제거했다. 새exe/의존DLL해시일치·launcher check통과. BAT는다음시작부터새서버사용,현재Player14512/MainServer13120/Login22912세션은자동종료/교체하지않았다. 새정책의실전재입력은미실행이다.

공개 HUD 설명과 현재정책을 특수 스킬만 방어로 갱신했고 사이트빌드통과. Agent615(신규232/후속383),직접구현·검사는활성화제외,토큰/비용N/A.