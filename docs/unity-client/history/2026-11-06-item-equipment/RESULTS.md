# 아이템 데이터 공유·기본 무기 장착 — 최종 후보

## 현재 결과 — 2026-11-06
사용자의 추천 규칙 전체 승인 후 구현했다. 아래 과거 기록의 ‘답변 대기/미실행/서버 변경 없음’은 해당 시점에만 유효하며 현재 상태는 이 절이 기준이다. codex/item-equipment, 기반 f205bf16, metadata 커밋 d016407 이후 장착 후보. 신규 push·PR은 아직 없다.

- 서버 itemMap → JSON format4 items → 기존 Unity importer/가방·거래소 표시. 이름·분류·회복·쿨타임·weaponAttackBonus 정본 공유, 기존 아이콘/GUID 유지. 구 format3 입력은 거부하고 마지막 유효 에셋 보존.
- 보유 철검3 한 칸, 공격+2, 가방 수량 유지, 장착 중 같은ID 모든 스택 판매·소비 차단, 해제 후 판매 허용. 자동 지급 없음. 기본 attack은 보존하고 플레이어 각 타격 phase에서 EffectiveAttack을 계산한다. 몬스터 AI/공격 경로는 바꾸지 않았다.
- 기존 inventory shard가 장착/차감을 함께 소유하며 WAL 성공 전 확정하지 않는다. 월드 진입 전에 서버가 저장 상태를 적재한다. UI 조회로 공격 보너스를 켜지 않는다.
- ClientCore는 GET/SET 요청·epoch·수명을 대조한다. mutation 자동 재시도 없음, 결과 불확실 시 판매/재시도를 차단하고 재접속 안내. 가방을 닫아도 장착 확정 결과를 반영한다.

## 검증 근거
ROOT/UnityClient/Builds/ClientGrowthHUD/verification/item-equipment 아래 파일:

| 확인 | 실제 결과 | 근거 |
| --- | --- | --- |
| ClientCore·MainServer·Unity 컴파일 | 성공, Unity errors0/warnings0 | server-build-final.log 및 컴파일 출력 |
| Windows Player | 성공, errors0/기존warnings5 | player-build.json |
| 서버 직접 회귀4개 | 통과 | server-equipment-tests.xml |
| ClientCore/UI 장착2개 | 통과 | unity-equipment-tests.json |
| 데이터 import2개 | 통과, 실제5아이템/철검bonus2 | content-tests.json, server-content-v4.json |
| 기존 DB 읽기 호환 | 5,002행 전부 해석, 거부0 | inventory-codec-readonly-final.json |
| 정상 구매·수령 | 철검1, 다이아750→650, 골드100020 유지 | receipt-ready.jpg 및 실제 UI 응답 |
| 장착·판매 차단 | 수량1, 공격11+2=13, 해제 전 판매버튼 차단 | equipment-equipped.jpg, equipment-sale-blocked.jpg |
| 재접속·해제 | 재접속13 유지, 해제11 복귀, 수량1 | equipment-reconnected.jpg, equipment-unequipped.jpg |
| 종료 | 게임·MainServer·Login 정상 종료 | launcher stop 성공 출력 |

실제 계정은 Lv4/base11이라 11→13이며, 기본10→12는 초기 정책과 자동 fixture 기준이다. 수령1 응답 뒤 refresh에서 ready0/processing0, 가방 철검1을 확인했다. 직접 DB 삽입/초기화/재시딩/스키마 변경은 하지 않았다. 구매한 기존 매물의 실제 listingID는 별도 확정하지 않았다.

독립 코드 리뷰는 차단0. 발견된 두 문제(기존 동일ID 다중스택 거부, 클라이언트 u16 포화검사 불일치)는 원담당자가 수정하고 직접 검사에 포함했다. Astra가 실제5장과 구현·검증 근거를 검수해 제품 통과, 최종 문서의 오래된 상태를 보정하도록 요청했고 이 절과 HANDOFF/공개 설명에 반영했다.

### 검증하지 않은 범위
실제 구역 왕복·전투 피해 수치 측정·서버 완전 재시작 후 장착 복원·구형 형식 변환 복구는 이번 실전 배치에서 하지 않았다. 해당 코드/fixture 검토와 실제 게임 확인을 혼동하지 않는다. 기존 거래 성공 안내의 두 줄이 Search 부근과 겹치는 선택 개선은 후속 UI 과제로 남긴다.

## 저장·패킷 계약과 복구 한계
- DB 테이블은 유지한다. 구 inventory prefix360 뒤 tag IEQ1/u32와 weaponID/u32를 붙여368byte로 쓴다. 명시된 legacy84/352/360 및 전체zero2404만 읽고 비영2404/손상값은 오류로 보고한다. 동일 아이템ID가 여러 슬롯에 있는 정상 기존 가방은 그대로 보존한다.
- WAL type1 payload368(charID8+구360)과 type2 payload376(charID8+새368)를 명시 해석한다. 타입은 uint16 그대로 검사하며 잘라서 허용하지 않는다. DB load/flush/WAL 복구가 같은 codec을 쓴다.
- wire53 GET:24byte,54 SET:28byte,55 RES:36byte(모두 flags0). 요청ID·ownershipEpoch·operation·status·weaponID·base/bonus/effective를 검증한다. 서버·ClientCore·Dummy enum을 함께 갱신했다.
- 현재 runtime은 Equipment Server/Player 쌍으로 고정한다. MainServer SHA256 8B2B9063B41593CBEADB466283039EFAED61E76A8712885501BD32AD37EB0869. settings-before.json에 이전 경로를 보존했지만 설정만 되돌려 구 서버를 실행하면 안 된다.

### 구 서버 복귀 절차 — 미실행
1. 새 서버에서 장착 해제·정상 종료·DB flush를 완료하고 DB와 WAL을 백업한다. 신규 거래·장착·보상 유입이 없어야 한다.
2. 새 type2 WAL을 재생·flush해 미반영 새 WAL이 남지 않았음을 확인한다. 파일을 무작정 지우거나 구 서버로 type2를 읽히지 않는다.
3. 승인받은 오프라인 변환에서 DecodeInventory로 읽고, 장착0을 확인한 뒤 EncodeLegacyInventory로360 형식을 만든다. 이 함수는 장착된 상태를 거부한다. 출력·행수·아이템/수량·LSN을 대조하고 적용·복원은 별도 승인 범위다.
4. 변환 DB와 호환 WAL 상태가 확인된 뒤에만 구 서버/클라이언트 쌍으로 복귀한다. 이 작업에서는 변환·직접 DB 업데이트·롤백 성공을 수행하거나 주장하지 않았다.

## 도입 당시 기록 — 현재 명령 아님

# 아이템 데이터 통합·기본 무기 장착 — 진행 기록

## 현재확인
PR69원격병합 f205bf16,새branch codex/item-equipment. 서버ItemData5개,현재장착상태없음. CharacterState.attack은기본값이며종료DB저장되므로장착보너스를직접누적하지않는다. InventoryData360byte raw DBblob/WAL sizeof고정. slot/padding숨기기/기본attack덮기안은제외.

## 기술비교와Astra자문
A추천: inventory360prefix보존+명시equipment format tag/equipID8trailer=368,구WALtype1(charID+360)/새type2(charID+368)모두복구. 기존loader는더짧은pre-ring blob도허용해실제길이분포읽기확인필수. 잘못된형식을빈가방으로위장금지. Binventory행별도장착열은스키마와WAL모두변경, Ccharacters열만두면가방판매와권위분리로비추천. 구형직접rollback불가,정상flush/신규WAL해결/변환복구준비필요. sameinventory shard에서장착과PartialUpdate/ChangeItemLocked음수차감모두배타. 신규장착은WAL성공후확정. baseattack유지/유효공격은넓게더해u16포화,player피해경로만적용. 기존phase피해는매타격때현재상태사용. 장착은아이템ID단위라동일철검스택전체판매잠금으로설명. 월드진입자율hydration필수.

## 독립구현
item_catalog_share소유5파일완료: exporter/ServerContent/importer/ItemPresentation/기존ContentImportTests2개수정. items단일배열로name/type/weight/회복/쿨정본공유,아이콘로컬기존보존. itemUse미사용중복제거. 현재서버5행고정개수검증대신유일양수ID/필수포션1·2/의미검증,추가ID100fixture와없는icon확인준비. 기존처치보상상수삭제후exporter참조남음결함도제거.
총괄UI에Content인자연결/기존OS MalgunFont참조재사용/fixturemetadata명시. exporter빌드및JSON4실제생성성공5items,기존MonsterData size_t→u8경고1. Unitycompile0error0warning. EditorImportValidated실행5items,GUID e20a847d2d4fa094088b01f4c513165b 유지(meta파일hash차이는CRLF차이로GUID동일확인). 실제asset만canonical로복사. 자동검사/실입력/새서버빌드는미실행. 서버장착/DB/WAL/네트워크변경없음.

## 선택·다음행동
현채팅비동기질문: 기본철검+2/1칸/보유품만/가방유지/판매전해제/재접속유지/타격시점기준과저장형식확장·구서버복귀제약을한번에제안,답변대기. DB스키마/실제DB수정승인으로가정하지않음. 코드확정후전체묶음리뷰와변경영향만검증.

## 장착정책 선택과 실제자료 조사
사용자현채팅비동기답변 추천규칙으로장착까지진행 확인. equipment_storage/ equipment_core 신규각1, item_catalog_share후속1 배정. actualREADONLY inventory길이84x20(zero4),352x2,360x5(zero2),2404x4975(전부zero). git6c100f2의ring/lastOp까지만있던352정의와createCharacter.sql55의UNHEX(REPEAT(00,2404))근거확인. 2404는전부zero만명시지원,상위bytes무시/300슬롯추정안함. 지정캐릭터11/5011/5012는모두360철검0. 기존구매가능철검listing13qty1price100조회(사람id/name/비밀출력없음). 정상거래로취득검증가능해실DB초기화/직접지급불필요. DB변경0/서버기동0. 근거 inventory-lengths-before.json/inventory-readonly-preflight.json.

## PR70 병합 후 후속 정리 — 2026-11-06
PR70은 master68c63f3에 병합됐다. GitHub 자동 리뷰 지적0, 사이트 빌드42167872056 성공. 다음 codex/defensive-equipment로 미커밋 후속 변경을 보존해 분기했다.

구매 성공 안내를 짧은 수량·금액으로 줄이고 수령 행동은 상세로 옮겼다. footer는 bottom52/height40으로 Search(top48)·구매 버튼(bottom94) 사이에 둔다. 직접 영향 기존 구매검사1개 통과, 이전8검사는 반복하지 않았다. 실제 서버 거래를 다시 하지 않았다. 에디터 TMP 측정에서는 성공1줄/상세2줄을 확인했다. 임시 카메라 캡처가 UI를 렌더하지 못해 정상 Game View 경로로 전환했다. frames3→10983 진행 후 market-copy-gameview-preview.png에서 버튼 겹침 없음을 직접 보고 Astra도 화면 품질 통과로 판정했다. EDITOR PREVIEW 명시·가상 문구이며 실거래/서버 응답 검증으로 쓰지 않는다. 캡처 도구가 Assets/Builds 아래에 저장해 생성 이미지와 .meta를 확인한 뒤 Assets 밖 검증 폴더로 옮겨 보존했다. 최종 Playoff/backgroundfalse/World씬clean 복원. 공개 화면 추가/교체 없음. 이 후속 문구의 최신 실행파일은 아직 재빌드하지 않았다.

사용자는 별도 비동기 질문에서 방패·갑옷 방향을 선택했다. 이는 새 효과 수치나 서버 저장 확장 승인으로 간주하지 않는다. 읽기 분석·선택안은 ARMOR_POTION_SCOPE.md/NEXT_ITEM_OPTIONS.md, 후속 정책은 ../2026-11-06-defensive-equipment/PLAN.md에 분리한다.
