# 아이템·보상과 시전자 추종 — 2026-11-05

## 결정과 구현
사용자는 MP3000회복/HP1번·MP2번/공유2초쿨,일반·강화몬스터처치HP·MP각1+골드10/확정후잔액갱신을선택했다. 패링쿨은3초(60틱),방어창0.25초(5틱)/특수스킬1회/기본공격제외를유지한다.

서버ItemData정본과기존exporter/importer로MP회복값/공유쿨을Unity에전달한다. 기존아이템소비reqID/epoch/단일pending·공유쿨을재사용했고만충·사망·수량/저장실패를구분한다. Unity는기존MP아이콘·가방사용과숫자1/2두슬롯을연결했다. 별도슬롯서버상태/DB스키마/Manager를추가하지않았다.

처치보상은HP/MP/골드각개별확정이다. 골드는기존내부메시지를Important로처리하고로딩콜백/캐시입금/characterID검증응답을연결했다. 확정상태만새opcode50의9바이트알림으로전달하고,잔액자체를push하지않고기존요청상관wallet조회로갱신한다. 조회중다중알림은flag로합치고timeout·구역전환·disconnect경계를정리했다. 미확정/실패는성공으로표시하거나입금을무조건재시도하지않는다.

DB SELECT성공+빈행만초기잔액0으로취급하고SQL오류/null은실패로구분한다. 기존goldflush QUERY8은UPSERT로정상게임최초통화행을만든다. 스키마변경/테스트DB수동수정없음. 캐시확정은DBflush완료나프로세스장애까지exactly-once보장이아니다. 기존dirty/flush세대보호를유지한다.

스킬은서버가각phase현재시전자좌표를사용하므로차징·예고만표시시전자평행이동을추종하고,조준각은유지한다. 확정Hit/착탄잔광은해당서버phase좌표에남긴다. 시전자제거·사망·구역전환/취소수명정리를유지했다. 사용자몬스터AI·피격판정은수정하지않았다.

## 검토와 자동 검사
기획판단Astra는FX정적범위통과·아이템설계방향적절로판정했다. 독립코드리뷰에서새골드알림malformed상태의Handler예외삼킴1건을발견했다. 원제작자가opcode50재throw와Handler수준잘못된상태/길이/flags검사를보완했다. 다른확정결함은발견되지않았다.

MainServer/UnitTests/ClientCore/DummyClients빌드,export→Unityimport실제asset(패링60/HPMP3000/공유40)확인,WindowsStandalone빌드성공. 기존LicensingClient진단은로그에보존했고설정우회없음. 터미널성공/실행파일/provenance를구분해확인했다.

직접검사서버2개(ItemUseCommitsOnceAndRejectsWithoutConsumption/PendingDepositsCompleteOnceAndPreserveFlush),Unity4개(자신charge/원격·몬스터charge/HPMP슬롯/골드wallet경합)통과. 리뷰보완후골드wallet1개만재실행해통과했다. 이전패링60틱직접검사2개는반복하지않았다. 복수서브시스템의직접영향경로라기본검사예산을확대했지만동일사실반복검사는피했다.

근거 ROOT/UnityClient/Builds/ClientGrowthHUD/verification/items-caster-follow의server-build.log/server-tests.xml/log,clientcore-build.log/clientcore-fix-build.log,dummy-build.log,export.log,import-result.json,unity-tests.xml(4/4),wallet-fix-test.xml(1/1),player-build-result.json/provenance. 이전쿨3초는verification/parry-cooldown3.

## 실행 전달과 남은 확인
바탕화면BAT는ROOT/UnityClient/Builds/LocalGame/ItemsCaster/Player/Server묶음으로연결했다. Server는새exe와필수DLL만배치했으며과거런타임로그/redo를복제하지않았다. 기존Login환경/앱을재사용하고비밀을복제하지않았다. launcher check와실제시작에서MainServer4536/Player24872/Login35140,server준비완료를확인했다.

실행화면에mainserver.exe Windows보안창이떠서입력은중단하고사용자수동처리를요청했다. OS보안앱클릭/설정우회는하지않았다. 현재입력허용은유지하되보안창처리전게임입력금지다. MP실사용·실제처치골드·이동중추종표현은이새묶음에서아직미확인이다. fixture를실전검증으로보고하지않는다. 최종종합검수·게시승인은아직이다.
## 최종 코드·자동 범위 결과
Astra가MP선택시사용버튼의HP라벨잔존을발견했다. 원래UI담당자가노출전HP/MP라벨선택갱신·생성초기generic문구로보완했고기존Inventoryfixture1개만재검증Passed1/1. C++검사는반복하지않았다. 수정반영Player-Fixed 재빌드성공/실행파일·provenance존재확인. Astra재검수는코드·자동검사범위필수0/통과이며실제조작감은미확인으로유지했다.

최신BAT는ItemsCaster/Player-Fixed/MMORPG.exe + ItemsCaster/Server/MainServer.exe,기존Parry/Login앱/보호환경을사용한다. launcher check통과. 보안창뒤현재실행중Player24872는수정전첫Player이며임의재시작하지않았다. 사용자보안창처리후정상종료/재시작으로최신Player를사용해야한다. 정책·효과·MP/골드실전확인은아직미완료다.

실제Agent추가10회(신규3/후속7): 서버분석신규1,서버구현신규1·Handler보완후속1,FX구현신규1,UI구현/라벨보완후속2,코드리뷰후속1,종합검수단계/최종/재검수후속3. 누적625(신규235/후속390),총괄직접작업은제외,토큰/비용N/A.
최신후보baed635의게시승인PLAYITEM01/r1을새부모1793852712.014869에요청하고등록·수신기31992/listening/age8초를확인했다. 이전PARRY01요청은대체했고기소비없음. 보안창실전미완료를요청본문에명시했다. 원격push/새PR게시/병합은아직하지않았다.

## 보안창처리후실전재개
사용자처리완료후기존세션정상종료→최신Player-Fixed/MainServer/Login재기동. Player25520/Server12256/Login34200. test1캐릭터11레벨4/Zone5진입·두포션슬롯·MP보유0확인. 기준Gold99980/HP포션18/MP0/EXP320/HP9870/11500/MP8638/10000,좌표(369.582,287.122). 사용자는일반몬스터1마리처치·R차징중이동추종직접확인요청대기,포션소비는아직안함. 자동화keyhold불가로이동만수동비교하며그동안자동입력/재시작안함. 기존자동검사반복없음.

## 직접 입력 후속·부재 중 보완
사용자가입력완료를회신하고패링성공이펙트/성공모션과비전투발동동작을요청했다. 직접관찰한처치전후는HP포션18→22,MP0→4,EXP320→360,HUD골드99980불변이었다. 마지막관찰HP9570/MP8510/좌표(383.835,338.842). 성공포션각4지급은관찰했으나실제골드입금은수동조회이전에Esc로ComputerUse가중단돼확정하지못했다. 시장클릭결과도미확인,MP사용입력은하지않았다. 이어사용자는약2시간부재중PC권한/입력·실행검증·PR게시보류를지시했다. 경과시간만으로재개하지않는다.

같은원담당이ActorMotion을보강했다. 발동Begin은방패들기/지탱/복귀만표시하며실제서버Success에서만금속섬광/방패반동/국소spark를표시한다. 효과는기존shield shader로생성/재사용·수명정리,루트좌표/판정/쿨/공격·피격시간을유지한다. 몬스터비상호작용에서도발동동작은있지만가짜성공은없다. 제품ActorMotion.cs/기존검사1개만변경했다.

골드표시원인은MarketScreen.WalletReceived의!walletPending이Core자동조회응답을버리는조건이었다. UI요청gate만제거하고Core의requestID/generation검증및모델·월드·구독수명을유지했다. 자동성공응답은초기중복조회·오래된재조회예약을정리하고실패는기존예약/예산을재사용한다. 서버·경제수치·프로토콜추가변경없음. 기존UI검사1개를실제Core→자동query→HUD/이전모델late거절경로로확장했다.

검증: UnityCLI1.0.0-beta.16의두집중PlayMode검사는결과보고전Editor exit199/TEST_RUN_ERROR로종료돼제품통과/실패를판단할수없다. 진단시도--log-file은test명령에없는옵션이라INVALID_COMMAND_ARGS로거절되어실행자체가없었다. 도움말로CLI옵션미지원을확인했다. 이후지원되는Unity인자전달(-logFile)로진단로그를모았고라이선스BIOS조회권한오류/IPC대기를확인해검사완료전우리CLI작업을취소했다. 같은권한의추가시도/권한상향/우회는하지않았다. 새권한을요청하지않고기존Unity생성Mmorpg.Client.csproj와Mmorpg.Client.Tests.csproj를dotnet --no-restore/UseSharedCompilation=false로정적컴파일했다. 두프로젝트모두exit0/오류0(Runtime기존경고6/Test기존경고1). 변경4파일이Compile항목에있고ROOT복사본과정본해시일치확인대상이다. 이는UnityPlayMode검사·에디터렌더·Player빌드·실전확인을대체하지않는다.

독립코드리뷰는ActorMotion/HUD각범위차단0,종합검수는코드·정적컴파일범위필수0/통과다. 실제sprite크기/명도·패링실성공·골드입금·수정HUD실화면·MP소비는미확인이다. 새Player를빌드/교체하지않고기존BAT/현재게임세션을유지했다. 권한요청·PC추가입력·실행서버전환·원본Game/AI/DB쓰기·원격push/PR게시없음.

추가독립작업: 현재소비·보상wire계약과CONTENT_PIPELINE/BRIEF의확정정책을갱신하고과거기록은보존했다. 다음구역성장A/B와포션공급초안은PLAY_FEEDBACK_PLAN끝에미승인으로준비했다. Zone1마을은기존결정으로보존하며경제/레벨수치는임의적용하지않았다. 근거링크를실제파일로확인/보완했고Astra가선택안검수통과·A로설계범위를좁히는추천을했으나사용자선택을대신하지않았다. Agent634(신규236/후속398),토큰·비용N/A. 실제후속검증로그는verification/parry-wallet-followup,직전실전증거live-after-kills.png다.
## 추가 입력 가드·후속 설계 마감
좁은클라이언트입력조사에서TMP의Escape편집종료와2/Space동시입력프레임에월드단축키가통과하는경로를확인했다. 기존GameUiInput.textEscapeFrame을공통단축키검사에도사용했다. 리뷰가TMP자체종료뒤Combat선행순서의잔여경로를지적해원담당이선택TMP+이번프레임Escape도차단했다. 새상태·Manager·강제executionOrder·입력재생은없다. 기존Esc PlayerLoop fixture1개에Combat→Inventory→Combat순서와다음프레임1/2/Space복구를추가했다. 이fixture는기존OSfocus조건이있으며현재미실행이다.

새변경에직접영향받은기존생성프로젝트두개를runtime-static-reviewed.log/tests-static-reviewed.log로다시정적컴파일했고각오류0/기존경고6·1이다. 코드리뷰재검토차단0,종합검수는코드·정적컴파일범위필수0/통과다. Unity검사결과·Player빌드·배포·실전통과로확대하지않는다. 현재실행파일/BAT는기존검증본을유지하고새소스는아직배포하지않았다.

읽기후속은거래수령함계약까지진행했다. 현재구매는outbox보관후명시Receive items,취소는즉시배송시도,CheckOutbox는목록이아닌최대30건배송이다. READY중캐시기지급미flush가있어새목록정합성필요. 일괄/개별수령과반환품범위는선택대기이며새API/DB/배송정책은구현하지않았다. PLAY_FEEDBACK_PLAN의구역성장A/B·포션공급·수령함초안은종합검수통과,사용자결정전미승인이다.

정상Unity인자 -logFile 진단로그 editor-logged.log에서Unity.Licensing.WindowsSystemInformation.get_BIOSIdentifier의System.UnauthorizedAccessException과licenseIPC대기를확인했다. 이환경실패는제품실패가아니며,로그수집뒤우리진단CLI를취소했다. PC입력·권한상향·BIOS/라이선스설정변경·다른실행경로우회는없다. 사용자부재중새권한질문/실행검증/원격push/PR게시를하지않는다. Git후속정리는복귀후진행하며추가변경은미커밋diff로보존한다.

Agent최종누적641(신규238/후속403),이번마감추가7회내역은AGENT_USAGE참조. 토큰·비용N/A. 사용자복귀신호후에만보류검증과최신후보게시준비를재개한다.
## 사용자 복귀 후 실행 검증 재개

최신 요청 ‘Unity 실행검증하고 PR 올리자’로 이전 부재 보류를 해제했다. 최초 권한 심사는 과거 보류를 적용해 거절했으나, 최신 명시 요청을 설명한 재심사는 허용됐다. Unity CLI beta.16은 최신이다. 실제 사용자 세션에서 집중 PlayMode 검사 3개를 실행해 패링·잔액 콜백 2개 통과, Escape 검사는 batch Editor 포커스 전제 실패였다. 열린 Editor에서 Escape만 다시 실행한 시도는 첫 Escape 후 input.isFocused가 true여서 실패했다. 당시 다른 게임이 화면 앞에 있고 사용자 입력이 감지돼 UI 조작을 멈췄으며, 포커스/제품 원인은 아직 확정하지 않았다. 실패를 통과나 assertion 완화로 바꾸지 않았다.

동일 소스 Player-Feedback/MMORPG.exe 빌드는 Succeeded, 오류0·경고1(Pipeline Runtime 설정 없음)이다. 기존 Player-Fixed/서버/로그인은 변경하지 않았다. 최신 빌드는 실제 로그인·MP 소비·잔액·패링 시각 검증 전이다. 근거 resumed-tests.xml/resumed-tests.log/focused-test-attempt.json/resumed-build.json은 verification/parry-wallet-followup에 보존했다.

## 포커스 확보 후 최종 확인

사용자가 ‘지금 Unity 검증 진행’을 답한 뒤 Editor를 앞으로 가져와 Escape fixture만 재검증했다. Passed1/Failed0(0.4초)이며 이전 실패는 포커스 확보 후 재현되지 않았다. 이미 통과한 패링·잔액 fixture2개는 반복하지 않았다. 제품·테스트 assertion 수정 없이 통과했다.

이전 세션을 정상 종료하고 ClientExe만 Player-Feedback으로 바꿔 같은 서버·로그인과 시작했다. test1 캐릭터11/Lv4/Zone5 진입 성공. 실제 HUD Gold100020으로 이전99980 대비 처치4회 보상40을 확인했고, MP포션 클릭으로 수량4→3/MP8510→10000·‘MP potion used. Cooldown 2.0s.’를 관찰했다. HP포션22/HP9570은 유지됐다. 패링 클릭의 서버 accepted·방패 들기 자세·잔여쿨2.7초를 관찰했다. 실제 특수 공격 패링 Success 순간은 이번 세션 미재현이며 그 연출은 fixture 검증으로 한정한다. 서버 AI·DB 스키마·수동 DB 데이터 변경은 없었다.

최신 실행본 session6d92276f592f46c883486b1a8f5ccfd7은 검증 후 게임·MainServer·Login 모두 정상 종료했다. 다음 BAT는 새 Player-Feedback을 사용하고 이전 Player-Fixed는 복구용으로 보존했다. 근거 focused-retry.json 및 live-wallet-before-mp.jpg/live-after-mp.jpg/live-parry-begin-mp3.jpg. 최신 후보 종합 검수 재요청 후 게시한다.

종합 게시 검수 최종 통과·필수0. Escape 포커스 재검증과 실제 잔액/MP소비/패링발동 근거를 대조했으며, 특수공격 방어 Success는 이번 실전미재현이라고 PR에 명시하는 조건이다. 사이트 생성·내부 링크 검사도 통과했다.

게시: c75b823, PR67 https://github.com/JoEunil/MMORPG/pull/67 (98파일 통합), 사용자 직접 게시 요청으로 진행. Unity Coordinator 앱 U0C9ZRTQ6F8가 기존스레드1793852712.014869에 결과1793867860.792759 발신. 병합·Pages배포는 수행하지 않았다.
