# 가방 읽기 실패 후속 분석

조사 대상은 `C:/Users/joeun/Desktop/workspace/Game-UnityClient/UnityClient/Builds/C10-Delivery/source`의 `codex/inventory-read-followup` 후보이며, 소스 분석은 `HEAD d217f0a` 기준이다. 분석 담당은 DB·Editor·서버를 조작하지 않았다. 아래 메타데이터 관찰은 이후 Coordinator가 기존 읽기 전용 도구로 확인한 결과다.

## 실제 읽기 확인과 구현 범위

기존 `Inspect-C07Inventory.cjs`를 한 번 실행했다. `START TRANSACTION READ ONLY` 안의 SELECT에서 5011·5012 모두 inventory 행 present/BLOB360 bytes를 확인하고 rollback으로 끝냈다. 새 계정·행 생성·내용 읽기/변경·DB 초기화는 없다. 실제 첫 요청 시각의 캐시 상태 자체는 미측정이며 cold-cache를 환경 원인으로 확정하지 않는다. 계정 귀속 JOIN은 실행하지 않았다.

다음 로컬 후보는 기존 가방 UI가 명시적인 실패 응답을 받았을 때 열린 화면에서 GET만 짧은 지연 후 한 번 다시 읽는 방식이다. 실패와 재시도를 문구로 구분하고, 두 번째 실패는 Refresh 안내로 남긴다. 성공한 실제 응답 전에는 No items나 조회 성공을 표시하지 않는다. timeout/전송 예외/쓰기 요청은 자동 재시도하지 않는다. 닫기·모델 교체·Zone 전환·disable에서 예약을 취소하고 기존 pending 및 수령 후 fresh-read 계약을 유지한다. 서버·ClientCore·프로토콜은 변경하지 않는다. 기존 코드와 독립 리뷰·직접 최소 검증 후에만 구현 완료로 기록한다.

## 확인된 계약과 원인 후보

클라이언트의 `ClientCore/ViewModel.RequestInventory()`는 월드 진입 상태에서 단일 요청을 보내고, `InventoryReceived()`는 서버 `resStatus == 1`일 때만 성공으로 알린다. `UnityClient/Assets/C02/C07InventoryScreen.cs`는 실패를 “Select Refresh to try again”으로 표시하고, 성공한 0개 목록만 “No items.”로 표시한다. 따라서 현재 UI는 빈 인벤토리를 실패로 위장하지 않는다.

서버 `CoreLib/NonZoneHandler.cpp::GetInventory`는 세션의 캐릭터 ID를 읽어 `MSG_INVENTORY_REQ`에 넣는다. 실제 비동기 진입 테스트 `UnitTests/CoreLib/NonZoneThreadPool.cpp::PendingEnterCompletionCannotRebindEmigratedCharacterOrInventory`는 캐릭터 5011/5012 진입 완료가 엇갈려도 인벤토리 요청에 현재 바인딩된 5011을 넣으며, 늦은 진입 응답이 이 바인딩을 바꾸지 않는 계약을 고정한다. 현재 보고된 계정02/캐릭터5012 로그인·월드 진입 성공은 결합 자체의 증거지만, 소스만으로 실제 요청이 5012를 운반했는지는 확인되지 않는다.

캐시의 `CacheLib/CacheStorageInventory.cpp::Getter`는 캐시 미스에서 DB 적재를 비동기로 예약하고 즉시 `DB_READING`을 돌려준다. `CacheLib/Handler_Inventory.cpp::InventoryRequest`는 `AVAILABLE`만 성공 응답으로 만들며 `DB_READING`도 즉시 실패 응답으로 바꾼다. 적재가 끝난 다음 요청은 성공할 수 있다. 반면 DB에 캐릭터 행이 없으면 콜백이 `SetEmpty`로 되돌리고 실패를 유지한다. 그러므로 **유효한 인벤토리 행이 있는 캐릭터의 차가운 캐시 첫 요청은 실패할 수 있는 서버 경로가 코드상 확인된다.** 이 동작은 계정01의 앞선 “첫 요청 실패 후 새로고침 성공” 관찰과 부합하며, 계정02에서도 가능한 선도 원인이다. 계정02에서 캐시가 실제로 차가웠는지 또는 DB 행이 존재하는지는 아직 미확인이다.

빈 가방은 `AVAILABLE` 상태의 유효한 `InventoryData`에서 슬롯이 모두 0인 경우 성공 응답(`resStatus=1`, `itemCount=0`)으로 직렬화된다. `Resources/DB/init.sql`은 `characters_inventory.char_id`를 기본 키로 정의하고, `Resources/DB/createCharacter.sql`은 새 캐릭터 생성 시 해당 행을 만든다. 캐시 쿼리(`CacheLib/Config.h::QUERY_5`)는 `char_id`로 인벤토리 행을 읽는다. DB 행 부재, 조회/큐 실패, 정상 빈 목록은 서로 다른 상태이며, 클라이언트에서 실패를 빈 목록 성공으로 바꾸면 안 된다.

## 최소 후속 확인과 변경 판단

환경에서 한 번의 승인된 **읽기 전용 SELECT**로 `char_id=5012`의 캐릭터가 지정 계정02 사용자 ID에 귀속되는지와 `characters_inventory` 행의 존재 및 BLOB 길이만 확인한다. 로그인 ID는 값으로 출력하지 않고 조건 파라미터로만 사용한다.

```sql
SELECT c.char_id, c.channel_id,
       (u.user_id IS NOT NULL) AS belongs_to_account,
       (i.char_id IS NOT NULL) AS inventory_row_exists,
       OCTET_LENGTH(i.inventory) AS inventory_blob_bytes
FROM game.characters AS c
JOIN login.users AS u ON u.user_id = c.user_id AND u.login_id = ?
LEFT JOIN game.characters_inventory AS i ON i.char_id = c.char_id
WHERE c.char_id = 5012 AND c.deleted_at IS NULL;
```

결과가 소유 캐릭터와 인벤토리 행을 확인하고 BLOB 길이가 유효하다면, 기존 서버 로그에서 같은 요청 시각의 `inventory not found in DB`/DB 작업 오류가 없고 첫 실패 뒤 캐시 적재가 끝난 뒤의 재요청이 성공하는지만 확인하면 cold-cache 후보를 가를 수 있다. 이 환경 사실은 아직 관찰되지 않았다. 행이 없거나 소유자가 다르면 우선 데이터/선택 결합 문제를 조사해야 하며 빈 목록 성공으로 보정하지 않는다. 행이 있고 반복 요청도 실패하면 서버 로그의 DB 조회 실패·적재 큐 실패를 먼저 확인한다.

## 수정 후보와 최소 검증

DB 행이 정상이고 cold-cache 응답이 원인으로 확인될 때만 서버 응답 경로를 최소 수정한다. `DB_READING`을 정상 빈 가방으로 응답하지 말고, 적재 완료 후 같은 세션/캐릭터 요청에 성공 결과를 전달하거나 클라이언트가 제한된 1회 재요청을 하도록 한다. 범용 캐시 대기 구조나 스키마 변경은 필요하지 않다. 기존 캐릭터 바인딩과 BG/P28 수정을 유지하고, 지연 응답이 다른 캐릭터 선택/세션에 적용되지 않도록 요청 시점의 세션-캐릭터 결합을 확인해야 한다. 수정 필요성은 위 SELECT와 요청 로그가 확인되기 전에는 확정하지 않는다.

수정한다면 최소 확인은 (1) 정상 DB 행이 있는 cold-cache 첫 요청이 성공한 실제 빈/내용 목록으로 마무리되고, (2) DB 행 부재 또는 조회 실패가 실패로 남으며 빈 목록 성공으로 보이지 않는지다. 이번 분석에서는 테스트나 서버 조작을 실행하지 않았다.
