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
