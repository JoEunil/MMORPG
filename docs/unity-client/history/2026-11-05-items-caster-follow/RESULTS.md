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
