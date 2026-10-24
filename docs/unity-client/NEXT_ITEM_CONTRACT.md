# 다음 아이템·캐릭터 화면 묶음 — 인벤토리 계약 분석

2026-10-19. 분석 담당 소유 문서. 제품 코드·DB·서비스·빌드·Unity는 변경하거나 실행하지 않았다.

## 결론

현재 전용 캐릭터의 가방 조회 실패는 **아이템이 0개인 정상 인벤토리를 UI가 오인한 것**으로 확인되지 않았다. 과거 READ ONLY 증거는 `characters_inventory`의 **행 자체가 없다**는 결과다. 서버는 행 없는 경우와 비동기 로딩 중인 경우를 모두 기존 실패 응답으로 보낸다. 정상 행의 BLOB이 비어 있는 경우에는 이미 성공·0개 응답이 가능하다.

따라서 클라이언트에서 status0를 빈 가방 성공으로 바꾸거나, DB 행이 없을 때 공유 캐시에 빈 인벤토리를 넣어 성공을 만들면 안 된다. 실제 아이템 카드 조회가 성공한 것으로 공개 설명이나 검증 기록을 바꿀 근거도 없다.

## 조사 기준과 증거

- 서버 기준은 `master d535b0df77ad17fb26cab3dbc2ee6ef1e97c7f53`, 조사 경로는 `UnityClient/Builds/C10-Delivery/source`다. 조사 시작 당시 작업 폴더 HEAD는 설명 후속 `ef03b18`, 조사 중 총괄의 다음 묶음 준비로 `3f751bdb46042d1b5a294b823b801f6ee016a310`이 됐다. 아래 핵심 inventory 서버/Core 파일은 d535b0d와 diff가 없음을 확인했다. HEAD 이동을 코드 구현 완료로 해석하지 않는다.
- `UnityClient/Captures/c07-inventory-db-metadata.txt`: 5011·5012 모두 `row=missing`. 기존 도구의 READ ONLY 결과를 재사용했으며 이번 분석에서는 새 DB 조회를 실행하지 않았다. 현재 DB 상태를 새로 확인한 증거는 아니다.
- `docs/unity-client/history/2026-10-24-work-records/C02_DB_REVIEW.md`: 전용 캐릭터 준비는 profile+characters 총 4행이었다. inventory/currency/diamond는 쓰기 대상에서 명시적으로 제외됐다. 누락은 당시 제한된 준비 범위와 일치한다.
- `Resources/DB/createCharacter.sql:54`: 기존 대량 개발 캐릭터 생성 절차는 characters 다음에 별도 inventory 행을 INSERT한다. `Resources/DB/init.sql:54`의 inventory 테이블은 별도 PK/BLOB 테이블이며 이 DDL 자체는 자동 행 생성 기능이 아니다. 대량 생성 SQL은 실행하면 안 된다.

## 실제 요청 → SQL → 화면

1. `ClientCore/ViewModel.cs::RequestInventory`: InWorld와 단일 pending을 확인하고 기존 목록을 지운 뒤 요청한다. 요청에는 사용자가 고를 수 있는 캐릭터 ID가 없다.
2. `CoreLib/NonZoneHandler.cpp:177::GetInventory`: `StateManager::GetCharacterID(sessionID)`로 서버에 바인딩된 characterID를 가져와 `MSG_INVENTORY_REQ`에 담는다. `StateManager.h:147`의 getter는 세션 바인딩을 반환한다. 현재 이 경로는 MyList의 `GetFieldCharacterID` 검증보다 약하지만, 두 전용 계정의 이번 실패가 오귀속 때문에 발생했다는 증거는 없다. R01 write-once 바인딩 수정이 반영된 기준에서 조사했다.
3. `CacheLib/Handler_Inventory.cpp:12::InventoryRequest`: `cache_inventory->Getter(characterID)`가 AVAILABLE일 때만 status1로 아이템을 복사한다. AVAILABLE인데 모든 itemID가 0이면 itemCount0·성공이다. 나머지 캐시 상태는 전부 status0·0개다.
4. `CacheLib/CacheStorageInventory.cpp:75::LoadFromDB`: 첫 조회는 DBWorker 작업을 넣고 즉시 DB_READING을 반환한다. SQL은 `CacheLib/Config.h`의 QUERY_5, `SELECT char_id, inventory FROM characters_inventory WHERE char_id = ?`다.
5. 비동기 SQL 결과가 `!res || !res->next()`이면 `SetEmpty` 후 종료한다. `CacheStorage.inl::SetEmpty`는 READING 엔트리를 지운다. DB 행이 없으면 다음 Refresh도 같은 실패/로드/삭제 흐름을 반복한다. SQL 예외·연결 고갈은 DBWorker failure callback으로 같은 SetEmpty 처리를 하며 성공으로 바뀌지 않는다.
6. 행이 있으면 `EMPTY_INVENTORY`로 선초기화한 데이터에 BLOB을 읽고 Insert해 AVAILABLE이 된다. **처음 요청의 성공 응답을 나중에 재발송하지 않는다.** 정상 행도 차가운 캐시의 첫 요청은 실패하고 이후 명시 Refresh에서 성공할 수 있다.
7. `CoreLib/MessageQueueHandler.cpp:109::InventoryResponse` → `PacketWriter.cpp:149::WriteInventoryResponse`: 기존 opcode21·81바이트, status 0/1, 최대10행 계약이다. 실패는 count0, 성공 count0도 허용된다.
8. `ClientCore/PacketHelper/Packet.cs::DeserializeInventory`는 성공·0개를 정상으로 파싱한다. ViewModel은 status1만 성공으로 전달한다. `C07InventoryScreen.InventoryReceived`는 성공0개에 `No items.`, 실패에는 Refresh 안내를 표시한다. 이 분기는 현재 계약과 일치한다.

## 최소 변경 판단

### 이번 묶음에서 바로 가능한 범위

- 캐릭터 상태 화면과 가방의 **실제 수치·결과·상태 설명**을 개선하되 status0/timeout/성공0개를 유지한다. 정상 빈 가방 표시를 별도 신규 서버 기능으로 만들 필요는 없다.
- 서버 실제 바인딩과 유효한 field 세션만 조회/응답하는 보강이 필요하면 대상은 `CoreLib/NonZoneHandler.cpp::GetInventory`, `CoreLib/MessageQueueHandler.cpp::InventoryResponse`, `CoreLib/MessageTypes.h::MsgInventoryResBody` 및 관련 단위 fixture다. 응답 내부 characterID를 추가하고 현재 field 바인딩과 대조하는 방식은 wire 변경 없이 가능하지만, 이는 **행 누락을 해결하는 수정은 아니다**. 기존 zone 전환 중 요청 허용 범위와 함께 검토해야 한다.
- cold cache 때문에 정상 행도 첫 요청에 실패하는 문제를 고친다면 대상은 `CacheLib/Handler_Inventory.cpp`와 `CacheStorageInventory.cpp/.h`의 조회 완료 경로다. 한 요청당 최종 응답 한 번, DB 작업 실패·큐 거절 응답, 세션 바인딩 재확인을 포함해야 한다. 공유 상태·대기열을 크게 추가하기보다 기존 DBWorker 완료 경로를 활용하는 좁은 구현을 검토한다. DB row-missing은 끝까지 실패로 유지한다. 자동 무한 재시도나 UI 가짜 성공은 대안이 아니다.

### 이번 분석만으로 확정할 수 없는 범위

- **없는 inventory 행을 정상 빈 인벤토리로 정의하는 정책 변경은 근거가 부족하다.** 기존 생성 절차는 행을 별도 생성하고 flush QUERY_6은 UPDATE-only다. `LoadFromDB`의 missing 분기에서 빈 데이터를 Insert하면 PartialUpdate/DeliverItem도 같은 캐시를 사용하여 메모리상 아이템을 추가할 수 있지만, 실제 DB 행이 없어 flush가 지속 실패할 수 있다. 조회 수정처럼 보여도 지급·배송·영속성에 영향을 준다.
- 테스트 성공을 위해 5011/5012에 빈 행·아이템을 추가하는 것은 분석 배정 밖이다. 기존 승인된 C02 4행 범위에 포함됐다고 해석하지 않는다. 특히 `generate_characters()` 실행, 스키마 변경, 기존 행 덮어쓰기·재시딩은 제안하지 않는다.
- 신규 캐릭터 생성 흐름의 누락 보완과 기존 계정 데이터 복구를 진행하려면 **실제 생성 경로와 실행 DB의 행 생성 정책**을 별도 확인해야 한다. 현재 QUERY_2 선언만으로 런타임 생성 API가 쓰이고 있다고 단정하지 않는다.

## 호환성·검증·복구 계획

- 표시 개선만이면 wire·DB·거래 정책 변경이 없다. 성공/실패 파싱과 기존 로그아웃/Zone 수명 차단은 유지한다.
- 조회 완료 경로 후보를 구현한다면 기존 wire81바이트/status/최대10행은 유지한다. 내부 응답 characterID는 외부 패킷에 추가하지 않는다. 공통 CacheStorage, 락, 큐, WAL, flush 정책은 이번 수정 대상으로 넓히지 않는다.
- 단위 검증: row-present/all-zero → 성공0개, row-present/item → 실제 값, cold→completion → 최종응답 1개, row-missing/SELECT null/SQL 예외/큐 거절 → 실패0개, 응답 전 로그아웃·다른 바인딩 → 폐기, 다른 계정 데이터 혼입0. 기존 실패를 성공으로 바꾸는 assertion 완화는 금지한다.
- 실입력 배치: 동결·독립 리뷰 후 두 전용 계정 로그인→가방→Refresh→시장 전환→정상 로그아웃을 한 번 묶는다. 실DB가 여전히 missing이면 실패가 진짜 결과이며, row-present 빈 성공·양수 카드 실증은 미실행으로 남긴다. 양수 카드 fixture 통과를 실제 계정 성공으로 바꾸지 않는다.
- 캐릭터 상태 검증은 서버에서 받은 level/EXP/HP/MP/최대치/공격력과 실제 HUD를 대조한다. 계산식이 없는 수치·아이템·장비 효과·가짜 전투력을 추가하지 않는다.
- 복구는 후보 코드·DLL/Scene의 작업 전 사본으로 정상 교체한다. 조회 전용 변경은 DB 복원 작업이 필요 없어야 한다. 운영 강제 종료·DB 삭제·데이터 초기화는 포함하지 않는다.

## 전용 계정의 빈 inventory 2행 보완 후보 — 실행 금지

Coordinator 추가 배정에 따라 **사람 승인 요청을 위한 후보**만 정리한다. 기존 C02 승인을 확대 해석하지 않으며, 이 문서만으로 DB 쓰기는 허용되지 않는다. 정상 빈 가방을 위해 서버의 missing-row 정책을 바꾸는 대신, 이미 검증한 두 테스트 캐릭터의 누락된 부속 행만 보완하는 별도 데이터 준비다. SQL·도구 후보의 독립 리뷰와 dry-run 결과를 준비한 후 Coordinator가 Unity Coordinator 앱 Slack 스레드에서 지정 사람에게 `승인 BAG` 같은 짧은 명령으로 승인받아야 한다.

### 스키마·BLOB 규격

- 기존 `game.characters_inventory`: `char_id BIGINT UNSIGNED NOT NULL PRIMARY KEY`, `inventory BLOB`(NULL 허용), InnoDB. DB BLOB 자체에는 길이 제약이 없다. 승인 전 실제 INFORMATION_SCHEMA로 일치·비생성 컬럼·트리거 부재를 확인하고 차이가 있으면 중단한다. 이 문서의 DDL은 저장소 소스 기준이며 실DB metadata를 이번에 확인한 것은 아니다.
- DB BLOB은 외부 81바이트 응답이나 packed7바이트 슬롯 배열이 아니라 `Cache::InventoryData`의 native 메모리 이미지다. `GetFlushCommand`가 `sizeof(InventoryData)`를 그대로 복사한다. `Core::MsgInventoryItem`은 uint32 ID/uint16 수량/uint8 slot과 자연정렬 padding을 가진다.
- 현재 헤더와 MSVC x64 기본 pack8 기준으로 **소스에서 계산한 예상**: MsgInventoryItem 크기8, InventoryData 크기360. offset은 count0, items4(10×8), recentEventIds88(32×8), head344, tail345, lastOp346, lastLsn352다. `EMPTY_INVENTORY`는 count/모든 item ID·수량·slot/ring/head/tail/lastOp/lastLsn 전부 0이다. padding도0인 360바이트는 이 빈 값과 일치한다.
- **실행 전 필수 확정:** 실제 후보 서버와 동일 컴파일러/설정에서 `static_assert(sizeof(Core::MsgInventoryItem)==8)`, `static_assert(sizeof(Cache::InventoryData)==360)`와 위 offsetof를 확인한다. 별도 codec fixture는 `InventoryData{}`의 의미 필드가0이며 SQL용 all-zero360바이트를 memcpy한 결과도 모든 의미 필드가0임을 확인한다. 분석 담당은 이를 빌드·실행하지 않았다. 결과가 다르면 360 후보를 실행하지 말고 새 규격으로 독립 검토한다.
- 기존 대량 시드의 `UNHEX(REPEAT('00',2404))`는 현재 구조체 크기를 보장하지 않는다. 같은 값으로 복사하거나 BLOB 크기를 wire frame81로 정하면 안 된다. 일반 아이템 serialization/가변 배열 codec을 새로 만들 필요는 없다. 이번 후보는 고정된 빈 native 이미지뿐이다.

### 고정 대상·실행 전 조건

| login | user_id | char_id | profile_id | profile.name |
| --- | ---: | ---: | ---: | --- |
| unity_ai_01 | 6981 | 5011 | 8192 | unity_ai_01_c02 |
| unity_ai_02 | 6982 | 5012 | 8193 | unity_ai_02_c02 |

각 대상이 login→user→characters→profile JOIN으로 정확히 1개씩 일치하고 channel_id1, deleted_at NULL인지 다시 확인한다. 이름에서 ID를 추측하거나 다른 캐릭터로 대체하지 않는다. 기존 characters/profile/account는 SELECT만 한다. 기존 부속 행이 **한 개라도 있으면 성공 재사용·덮어쓰기·추가 INSERT 없이 중단**한다. reconnect UI를 멈추고 지정 계정의 활성 게임 연결이 없음을 확인하며, 동시 게임 테스트 중에는 실행하지 않는다. 외부 환경 서비스가 실행 중이라는 이유로 강제 종료하지 않는다.

### 최소 SQL 후보와 원자성

아래는 사람이 검토할 SQL 형태다. **복사 실행용 독립 스크립트가 아니며**, 고정 SELECT 결과 assertion·메타데이터/연결 확인·오류 ROLLBACK을 수행할 driver가 필요하다. 일반 무인 실행은 dry-run/ROLLBACK만 하고, 승인 확인 후 정확한 단일 commit 모드에서만 commit하도록 분리한다. 비밀번호·DB 연결 설정은 인자·로그·문서에 넣지 않는다.

```sql
SET TRANSACTION ISOLATION LEVEL REPEATABLE READ;
START TRANSACTION;

-- driver: 기대한 두 캐릭터·소유자·profile·channel·미삭제 조건을 각각 확인.
SELECT c.char_id, c.user_id, c.profile_id, c.channel_id, c.deleted_at, p.name
FROM game.characters AS c
JOIN game.profile AS p ON p.profile_id = c.profile_id
WHERE c.char_id IN (5011, 5012)
ORDER BY c.char_id FOR UPDATE;

-- 반드시 0행. 한 행이라도 존재하면 즉시 ROLLBACK 후 종료.
SELECT char_id, OCTET_LENGTH(inventory) AS blob_bytes
FROM game.characters_inventory
WHERE char_id IN (5011, 5012)
ORDER BY char_id FOR UPDATE;

-- 위 ABI/codec 증거가 360임을 확정한 후보에서만 사용.
-- INSERT IGNORE, REPLACE, ON DUPLICATE KEY UPDATE를 사용하지 않는다.
INSERT INTO game.characters_inventory (char_id, inventory)
VALUES (5011, UNHEX(REPEAT('00', 360))),
       (5012, UNHEX(REPEAT('00', 360)));

-- driver: 직전 affectedRows==2와 두 결과 모두 길이360/all_zero1을 확인.
SELECT char_id, OCTET_LENGTH(inventory) AS blob_bytes,
       HEX(inventory) = REPEAT('00', 360) AS all_zero
FROM game.characters_inventory
WHERE char_id IN (5011, 5012)
ORDER BY char_id;

-- dry-run 또는 assertion/SQL/연결 실패: ROLLBACK;
-- 지정 사람의 승인·commit 모드·최종 assertion이 모두 유효할 때만 COMMIT;
```

- 두 캐릭터의 고정 row lock과 없는 inventory PK 범위 잠금을 동일 트랜잭션에서 얻는다. PK 제약을 그대로 사용하므로 경합 삽입·중복 오류·deadlock도 실패/전체 ROLLBACK이며 재시도 후 자동 커밋하지 않는다. DB isolation/트리거/engine이 다르면 중단한다.
- 결과 로그는 char_id/row 존재/바이트 수/all-zero/트랜잭션 결과만 남긴다. 승인 전 드라이버의 metadata·고정 SELECT dry-run은 읽기 확인으로 별도 기록한다. 쓰기를 포함한 롤백 시뮬레이션까지 승인 전 수행해야 한다고 해석하지 않는다.
- 승인 범위는 이 두 신규 inventory 행·아이템0개뿐이다. 재화/currency/diamond, 계정/profile/characters 수치, 스키마, 다른 캐릭터 데이터, 서버 정책은 변경하지 않는다.

### commit 후 확인·cold cache·복구 경계

- commit 뒤 새 READ ONLY 연결에서 두 행 길이/all-zero를 다시 확인한다. commit 응답이 끊기면 삽입을 재실행하지 않고 SELECT로 결과를 판별한다. 결과가 불명확하면 중단하고 Coordinator에게 인계한다.
- 캐시 missing은 SetEmpty로 엔트리가 제거되어 있으므로, 새 행 삽입만으로 기존 서버 재시작이 필수인 것은 아니다. 다음 실제 가방 조회가 cold cache를 로드하며 **첫 응답은 여전히 status0일 수 있다**. DBWorker 완료 후 사용자의 새 Refresh는 AVAILABLE/all-zero를 읽어 status1/count0가 된다. 이때만 빈 가방 실서버 성공으로 기록한다. Refresh에도 실패하면 DB 작업 로그/고정 SELECT를 확인하고 무한 재시도나 아이템 삽입으로 해결하지 않는다.
- 사전 실패는 전체 ROLLBACK으로 원상 유지한다. commit 뒤에는 정상 조회가 읽기뿐이어도 이미 캐시 또는 정상 게임 경로가 접근했을 수 있으므로 **자동 DELETE/되돌리기 금지**다. 원복이 정말 필요하면 두 행이 여전히 승인한 빈 이미지이고 사용·변경되지 않았는지 확인한 뒤 별도의 사람 승인으로 처리한다. 후보 코드·UI 복구와 DB행 삭제를 같은 작업으로 묶지 않는다.
- 승인 전 완료 조건은 코드/ABI·codec 근거, 대상 metadata/SELECT, driver의 중복 중단·실패 rollback 검토다. 실제 commit 및 Play 입력 검증은 미실행 상태로 구분한다.

이번 문서는 분석 완료이며 구현·독립 리뷰·테스트 성공 근거는 아니다. 최종 구현 배정과 PR 통합은 Coordinator가 결정한다.

## 최신 SELECT 사전검사 — 2026-10-19

root/Captures/c12-inventory-preflight.txt에서5011/5012모두행누락을다시확인했다. 드라이버SELECT-only사전검사는inventory columns에서안전중단했다. 실제INFORMATION_SCHEMA의char_id는저장소DDL의unsigned와달리signed BIGINT이고,PRIMARY KEY/NOT NULL/EXTRA빈값/컬럼2개/inventory BLOB NULL허용이다(Captures/c12-db-columns.txt). 스키마를고치지않고드라이버가이확인된기존signed BIGINT를정확히검사하도록후보를맞췄다. 두고정양수ID는범위안이며INSERT값/대상확대없음. 다른타입은계속중단한다. metadata진단에는컬럼정보만출력했고계정비밀은출력하지않았다.

## 커밋 실행 조건 강화

코드리뷰에서 Established0만으로 dirty/queued/WAL 부재를 보장하지 못하는 경계를 지적했다. SELECT-only 검사는 기존 실행서버가 무접속인 상태에서 가능하지만 --commit은 기존 정상 q 종료·drain 증거를 확보하고 MainServer 프로세스 전체0, 포트9999 Listen/Established0, 원래 작업루트의 모든 numeric redo/backpressure 파일0바이트가 확인된 동안에만 가능하다. 이 조건을 연결전/INSERT전/COMMIT전에검사한다. 기존영속로그는삭제/초기화하지않는다. 파일누락·비어있지않음·정상종료불명확시DB반영없이중단한다. 서버정상종료뒤남은WAL/dirty를임의로삭제하지않는다. 실제재기동은동일검토C10바이너리의기존start-only사전검사를따르며DB승인과혼동하지않는다. 스키마검사는inventory COLUMN_KEY빈값까지확인해char_id단독PK를보호한다.
