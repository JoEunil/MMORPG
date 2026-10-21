## 최신 후보 보정 — 2026-10-21

이하 본문은 2026-10-18의 과거 실행 버전 분석이다. 현재 후보는 서버 Rectangle의 수직 방향을 이동 규약 0=+Y(North)/1=-Y(South)에 맞게 교정했다. 수평 2=-X/3=+X, 길이·폭·피해·wire는 불변이다. 현재 계약은 [LINE_AIM_TASK.md](LINE_AIM_TASK.md), [LINE_AIM_RESULTS.txt](LINE_AIM_RESULTS.txt)와 후보 소스 `CoreLib/SkillData_Range.cpp`를 기준으로 한다. ROOT의 기존 서버 소스나 과거 본문의 0=-Y/1=+Y를 최신 계약으로 재사용하지 않는다.

네 방향 직접 자동 검증과 HUD 검증을 통과했다. 초기 Windows 보안 창 차단은 사용자가 직접 처리한 뒤 해소됐으며, 같은 날 후속 실제 접속에서 C키 1회로 서버 방향 0(+Y), 12×3 범위 윤곽, 동일 대상 HP 1500→500(피해 1000)을 확인했다. 실입력 검증은 완료됐으며, 초기 차단 이력을 이유로 반복하지 않는다. 처치·EXP 증가·재접속 영속성은 이번 검증 범위가 아니다. 상세 근거와 정상 종료 상태는 LINE_AIM_RESULTS.txt의 ‘후속 — 서버 확정 범위 윤곽 및 실제 C 검증 완료’를 따른다.

# C05 — 기존 전투·HP0 계약

2026-10-18 조사. 소유권은 이 문서뿐이다. 서버·ClientCore·Unity 제품·설정·DB·바이너리를 수정하거나 빌드/서비스/Editor를 조작하지 않았다. Coordinator와 다른 작업자의 변경을 보존한다.

## 기준과 실행 버전

- HEAD `1d5a3d69074a1de6bafde30f1ec4dfe5cac1a951`. `git status --short -- CoreLib`는 비었으며 전투 소스는 HEAD와 같다. ClientCore와 CacheLib에는 C01~C04 기존 미커밋 변경이 있다. HEAD만으로 현재 제품 전체를 표현하지 않는다.
- 현재 작업 폴더 `x64/Release/MainServer.exe` SHA256은 `C0F689BD0E58D3A20282143D5DD5C0C15514F42F192A6FF70568C2DDF312E825`로 HANDOFF의 실제 AUTH 빌드와 일치한다. HANDOFF는 game PID20024/9999 및 C04 실제 응답 검증을 기록한다. 이번 역할의 Get-NetTCPConnection 조회는 CIM 접근 불가로 실패했으므로 PID/포트의 현재 생존을 독립 확인했다고 주장하지 않는다. 실행 전 Coordinator/테스터가 확인한다.
- C04의 서버 관측·HP0 fixture 통과와 별개로 C05 실전투는 아직 실행하지 않았다. 새 wire/서버 변경/DB 준비 없이 현재 경로로 구현·검증 가능하다.

## 입력·서버 판정 경로

`IViewDataUI` → `ViewData` → 명목50ms `ClientTick/ViewModel.SendUpdate` → `NetworkService.Action/PacketBuilder.CreateActionPacket` → TCP → flags/current Zone 검증 → `ZoneHandler::ProcessAction` → `Move` → `Skill` → `DirtyCheck`. ACTION은 opcode11/flags1, pack1 little endian 전체18바이트다. 공통헤더8 뒤 `dir:u8,x:f32,y:f32,skillSlot:u8`. targetID/피해량/시각/입력번호/ACK 필드는 없다. 이동좌표와 스킬을 함께 보내며 시전위치는 Move 처리 후 서버좌표다. 이 경로는 타 플레이어에게 피해를 주지 않는다.

`CoreLib/ZoneState.cpp:ImmigrateChar`는 슬롯3개를 skillID0/1/2로 고정한다. 슬롯255는 이동만, 슬롯0도 실제 공격이다. `ZoneState_Actions.cpp:Skill`은 슬롯범위·skillID·서버 cooldown·MP를 검사한다. MP 부족/재사용 중이면 조용히 무시하고 별도 실패응답은 없다. 허용 시 MP를 차감하고 cooldown tick 설정/activeSkill 추가. `ZoneHandler.h:RunTick`은 cooldown감소→입력→ApplySkill→UpdateMonster→snapshot 순서다. client 로컬 쿨다운 완료를 서버 시전 성공으로 표시하지 않는다.

| 슬롯/skillID | MP | cooldown | phase0 / phase1 | 실제 피해 범위 | attack10 기준 피해 |
| --- | --- | --- | --- | --- | --- |
| 0 | 1 | 20tick≈1초 | 시작0tick / 타격6tick≈0.3초 | 시전원점 반경2 포함 | 50 |
| 1 | 20 | 120tick≈6초 | 시작0 / 타격16tick≈0.8초 | 시전원점 반경8 포함 | 1000 |
| 2 | 20 | 80tick≈4초 | 시작0 / 타격10tick≈0.5초 | 길이12·전체폭3 직사각형 | 1000 |

근거 `CoreLib/SkillData.cpp`, `SkillData_Range.cpp`, `ZoneState_Skills.cpp:ApplyHit`. 피해는 캐릭터attack×phase.attack이고 skill0 multiplier5/skill1·2 multiplier100. flags bit0가crit×2, 실제AOE 검사는0x02다(`SkillData.h`의 bit2 주석보다 실행코드 우선). skill1·2 flags2, skill0 flags0. skill0의 비AOE break는 AOI 전체가 아니라 각 cell 내부이므로 UI에 단일 지정대상 공격이라고 약속하지 않는다. 실전 피해는 서버 HP 변화로만 표시한다.

방향은 반드시 서버 실제 범위와 구분한다. 이동/몬스터 관례 dir0=+Y,1=-Y,2=-X,3=+X이나 **직사각형 판정은 dir0=-Y,1=+Y,2=-X,3=+X**다. circle은dir무관. 서버를 고치지 않는 C05 최소안은 slot2 조준/범위표시를 이 실제 규칙에 맞추고 이동 facing과 혼동되는 부분을 명시한다. +X 방향(dir3) 실검증은 불일치 영향을 피한다. 시전phase는 처음 저장한 x/y/dir를 유지하므로 이후 이동한 캐릭터 위치를 타격원점으로 바꾸지 않는다.

## 결과 wire와 표시 경계

ACTION_RESULT opcode12/flags1, 전체 `10+count*21`. 각 항목은 `casterType:u8,casterId:u32,skillSlot:u8,dir:u8,x:f32,y:f32,skillId:u32,skillPhase:u16`. casterType0=플레이어 Zone internalID,1=몬스터 internalID(u16에서u32로확장); monster typeID가 아니다. 시작phase0와 hit phase1도 서버가 broadcast한다. hit phase는 **피해가 실제 특정대상에 들어갔다는 ACK가 아니며 targetID/HP/피해량도 없다**. 범위밖 공격도 phase결과는 나온다. `PacketWriter.cpp:WriteActionField`, `ZoneState_Skills.cpp:ProcessCellActiveSkills`, `ZoneState_Snapshots.cpp:ActionSnapshot` 근거.

기존 `ReceiveTick`은 count/길이검증 parser로 ActionResult를 처리하고 `IViewModelUI.OnActionResultReceived`가 이미 있다. 결과를 확정시전/phase표시에 재사용하되 finite위치·dir0..3·casterkind/ID·지원skill/phase·count검증 및 세션/Zone/disable 정리를 유지한다. 몬스터 skill3은 phase0..3,skill4는0..9이므로 플레이어2phase만을 모든caster에 적용하면 정상몬스터결과를 잘못 판정한다. 최소플레이어표시만 한다면 type0/skill0..2/phase0..1만 명시적으로 처리해도 된다.

Full/Delta/Action에는 ZoneID·sequence·snapshot timestamp가 없고 broadcast worker 순서 복원을 보장하지 않는다. 클라이언트는 도착한 관측을 반영하며 조용히 새로운 서버순서 계약을 만들지 않는다. C04의 Full3초 관측TTL은 삭제판정이 아니다.

## 몬스터 HP0와 재생성

`ZoneState_Skills.cpp`는 alive몬스터만 타격, int HP를0으로 clamp하고 막타캐릭터EXP증가·몬스터HPdirty를 세운다. 일반type0 HP1500/attack5/respawn300tick≈15초/보상10EXP, type1 HP3000/attack8/500tick≈25초/30EXP, type2 HP10000/attack15/6000tick≈300초/300EXP다(`MonsterData.h`).

- MonsterDelta(field0)은 HP0을 보낼 수 있다. FullSnapshotMonster는 HP0 몬스터를 제외한다. 부재만으로 death라 하지 않는다.
- C04는 유효 HP0 관측에 즉시 visual제거, Delta단독부활금지, 유효alive Full로만재생성한다. 이번 실검증에서 이를 사용한다.
- `UpdateMonster`가 죽은몬스터 respawnTick을 감소,0이면`MonsterState::Respawn`으로 같은internalID/maxHP/원래spawn좌표/dir1을 복원한다. 해당함수는HP/x/y dirty를 직접 세우지 않으므로 **다음alive Full**을 기다린다(명목최대1초 추가). death가발생한tick에도카운트감소하므로 약15초이지밀리초정밀15초계약은아니다.
- Respawn은 phase/skillStep/tick을초기화하지 않는다. C05는이를고치거나새AI를제안하지않는다. 실제재생성HP와Full관측만검증한다.

## 본인 HP0의 실제 지원 한계

플레이어 HP/MP는 기존 Full(HPfield) 및 Delta(field0/1)로 내려온다. 몬스터 hit는HP0으로clamp한다. **Move/Skill/플레이어activeSkill처리에hp>0검사는없고, 본인death/despawn/부활/사망귀환wire도 없다.** 따라서0표시·클라이언트입력차단·기존Disconnect는가능하나 새부활버튼/시체규칙/서버가죽은캐릭터행동을거부했다는주장은불가하다. UI가막아도서버보안정책이추가된것은아니다.

`CacheLib/Handler.cpp`는 정상퇴장캐릭터HP/MP/EXP/좌표등을기존캐시/DB경로에저장한다. 재로그인이회복이라는근거가없다. 실전본인HP0는전용테스트계정한개에한정하고0상태의영속가능성을기록하며DB회복/재시딩을암묵수행하지않는다. 앞으로별도부활요구가생기면기존서버기능범위확인을먼저하며이번단계에신규시스템을만들지않는다.

## ClientCore 최소 호환 변경안

서버/wire 변경 없이 다음만 구현담당에게 권장한다. 이 분석 역할은 직접 고치지 않았다.

1. `ClientCore/Interfaces.cs:IViewDataUI` 및 `ViewData.cs`: 현재 확정좌표/dir/slot을 기존`_lockMove`안에서 함께기록하는작은 `UpdateAction(dir,x,y,slot)` 추가. slot0..2/finite/dir검증. `UpdateMove`후`UpdateSkill`별도호출은사이ClientTick에끼어들수있고`UpdateSkill`단독은예전좌표/dir를보낼수있다. 새범용명령큐/Manager는불필요하다. 여러입력은기존tick의마지막하나로합쳐짐을유지한다.
2. `ViewModel.cs:ActionResultReceived`: C04 Full/Delta와같이입구/postedclosure의lifetime검사및enqueue시점eventdelegate snapshot. 현코드는lifetime검사없이drain시현재구독자를조회하므로재진입세대에옛시전결과가유입될수있다. 소비자도model/Zone세대token을확인한다.
3. Unity는기존C02확정Full을HP/MP/dir/좌표의단일기준으로재사용한다. 이동목표중단→확정좌표+선택dir+slot을원자queue하는작은경계면을추가한다. 실제시전전localMP감산/데미지예측/허위cooldown성공은금지한다. 기존UpdateSkill/CancelMoveAPI를관계없는범위로리팩터링하지않는다.
4. fixture는원자queue읽기/slot검증/초기255보존/시전후255복귀/queued결과세대차단/HP0차단·정리·정상Full재생성을검증한다. 파싱count/길이와C03이동회귀를유지한다.

이 범위는 기존 ClientCore의 Unity최소API/수명호환 및 기존protocol대응이며 보호영역새계약이필요없다는추천이다. 원본단일소스에서DLL을빌드하고해시를기록한다.

## 통합 입력 배치 기대와 비용

저작/리뷰후Assets동결·Editor단독조작으로한번의로그인/선택/진입에합친다. 정상전용계정/실제몬스터/실제키입력만사용한다. 본인HP0 구간은 account2 `unity_ai_02` / character5012만 사용하며 account1은 생존 회귀용으로 보존한다. DB·치트·상태주입·서버재시작없이가능하다. 실제HP/MP/attack은이번진입응답/Full로확인하며준비당시10000/10000/10을현재값으로단정하지않는다.

1. B01기존실제로그인/선택/진입회귀. 독립raw Full/Delta/ActionResult collector를시전전에구독하고화면·서버HP/MP를대조한다.
2. 관측된type0를기존click이동으로접근. slot1의반경8 안에 유지한 뒤실제키입력→본인phase0/1·MP20감소·몬스터HP감소를관측한다. 바로같은키를다시눌러도별도성공phase/MP소비가없음을유한대기로확인한다. 성공이아닌요청버튼표시는구분한다.
3. 원점+X측의대상또는범위안으로이동후dir3 slot2를실제입력. attack10이면type0에slot1+2의1000×2로kill가능(최소MP40). 움직이는몹/원점고정/다른몹피격때문에이상적2회가반드시같은대상에맞지는않으므로실제관측ID와HP를추적하고최대60초/정해진MP예산으로종료한다. 대안은slot1두번(재사용6초이후)이며기본공격만이면30회≈30초가필요하다.
4. 특정 ID의 HP0 수신→같은visual제거→약15초+Full/여유5초내같은ID aliveFull→visual복구·실제HP/dir/위치일치. 다음Full의부재를HP0 증거로 대체하지 않는다. type0 수십 개가 AOI에 있으므로 ID를 고정한다.
5. 본인HP감소도관측. 본인 HP0 실제 검증은 account2/5012에서만 수행하며,중앙cell(2,2),Zone1spawn(50,50)의기존boss를기본공격50으로aggro후근접대각위치에서idle하는별도bounded최대120초구간을배치끝에둔다. boss skill3/4 multiplier100×attack15=1500으로초기10000HP는7적중필요. 거리의서버판정은intdx/dy의dist²≤2이며약(1,1)대각은skill3첫방사각30..150°와skill4첫외곽square에들수있다. stepdelay100tick≈5초+phase타이밍·이동상황에따라30~90초이상달라질수있어자동성공을보장하지않는다. aggroTick의증가코드가없으므로원거리boss공격을기대하지않는다. 보스HP10000을죽이는것이목적은아니다. 본인0가되면실제HP0UI/입력중지/정리증거를남기고정상Disconnect한다. 미도달이면미관찰로기록하며DB를수정하지않는다.
6. UI/HUD클릭은시전/이동을누출하지않고TMPfocus/앱focus/연결/Zone/disable/HP0중입력을막는다. 실제world에TMP입력필드가없으면focus는fixture/static근거로분리한다. native입력장치/임시device정리·Play종료·게임TCP연결0·서비스유지확인으로종료한다.

권장전체실행비용은fixture와기존회귀포함약5~8분, 동일실패2회면로그와재현을Coordinator에게돌려보낸다. 실제결과/PNG/GIF와최종제품·DLL·Scene해시를묶으며단순문서작성으로항목을통과처리하지않는다.

## 승인 판단

현재최소안에새서버/DB/인증/경제정책/Zone기반구조/운영변경은필요없다. 따라서별도승인요청을만들지않는다. 필드외곽아트는서버충돌판정이없는기존[-5,105]Zone1열린지면을시각보완하는클라이언트작업이며충돌벽을약속하지않는다. 프로토콜방향통일·서버사망/부활·시험용HP변경·새kill/despawnACK를추가하려면이번최소안과분리한정확한제안/영향/복구를먼저Coordinator에게제출한다.


## 실제 재접속 관찰 — 저장/로드 원인 미확인

C05 실제 전투에서 계정1/5011은 몬스터 재등장 대기 중 HP0·MP9919가 됐고, 정상 native Disconnect 뒤 실제 UI 재로그인에서는 fresh raw Full HP9975/MP10000(ownID12, Zone1,39,39)을 받았다. DB/HP 복구를 실행하지 않았다. 새 계정1 helper는 fresh HP0 전제 불일치로 추가 키·지면 입력 전에 중단했다. 따라서 HP0가 재접속까지 보존됨을 주장하지 않으며, 재로그인 회복 정책/새 부활 wire를 확인했다는 뜻도 아니다.

읽기 확인한 현재 체크아웃 경로: CacheLib/Config.h:53~58 QUERY3는 저장 HP/MP 조회, CacheLib/Handler.cpp:143~146와 CoreLib/MessageQueueHandler.cpp:62~65는 그 값을 진입 상태로 복사, CoreLib/ZoneState.cpp:45~80 Immigrate는 HP/MP 보정 없이 state 사용. 종료는 StateManager.cpp:81~87 Emigrate와121~124 현재 HP/MP 전달, CacheLib/Handler.cpp:160~164 QUERY4 비동기 저장 경로다. 이 경로에서 명시적 HP/MP 회복은 찾지 못했다. 실제 이전 HP0의 저장/로드/실행 빌드 동작 차이 원인은 미확인이다. 보호 영역 변경 없이 계정2 같은 연결의 정상 보스 피격 직후 HP0 입력 차단을 원래 승인 범위로 검증한다.
