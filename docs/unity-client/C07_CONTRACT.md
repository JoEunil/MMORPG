# C07 인벤토리 조회 계약

2026-10-19, 서버 분석 담당. 소스 기준 `7c83226fadb369de5a55e13d79a3b7479eba8be4`; 조사 시 `git diff --stat -- CoreLib CacheLib ClientCore` 출력 없음. 제품 소스·DB·실행 서버·Editor 변경 및 테스트 실행 없음. HANDOFF의 실행 서버 후보 `01C626…`와 이번 조사 소스/향후 후보가 같다고 가정하지 않는다.

최신 사용자/AGENTS 정책을 적용한다. 승인된 목표 안의 후보 구현·독립 리뷰·로컬 배치 검증은 진행하고, 보호 변경을 포함한 최종 후보의 push/PR 생성 직전에 Coordinator가 한 번만 Slack 승인을 요청한다. 이 분석 문서는 승인이나 실행 완료 증거가 아니다.

## 범위

현재 서버의 자기 캐릭터 인벤토리 조회, 슬롯 0~9·아이템 ID·수량 표시만 연결한다. 장착·소비·드래그·거래·재화·아이템 지급·DB 행 생성은 포함하지 않는다. 실제 데이터가 없거나 조회에 실패했으면 그 상태를 그대로 표시한다.

## 실제 wire

`CoreLib/PacketTypes.h`는 `#pragma pack(push, 1)` 안에 헤더와 인벤토리 구조를 선언한다. 현재 Windows 실행 경로는 별도 htons/htonl 변환 없이 구조체 필드를 쓰므로 little endian이다.

| 메시지 | 내용 |
| --- | --- |
| 공통 헤더 | 8바이트: magic uint16 `0xABCD`, 전체 length uint32, opcode uint8, flags uint8 |
| INVENTORY_REQ | opcode 20, flags 0, 전체 8바이트, body 없음; 캐릭터 ID를 클라이언트가 보내지 않음 |
| INVENTORY_RES | opcode 21, flags 0, 전체 **81바이트 고정** |
| 응답 body | status uint8 (0 실패/1 성공), itemCount uint16, 고정 InventoryItem[10] |
| InventoryItem | 7바이트: itemID uint32, quantity uint16, slot uint8 |

응답 오프셋: status 8, count 9, 첫 item 11. item i의 ID `11+7i`, 수량 `15+7i`, 슬롯 `17+7i`. 사용되는 항목은 선두 itemCount개이며 배열 인덱스는 슬롯 번호가 아니다. `CoreLib/Config.h::MAX_INVENTORY_ITEMS`와 `CacheLib/Config.h::MAX_INVENTORY` 모두 10. 서버 슬롯 생성 경로는 0부터 시작한다 (`CacheStorageInventory.cpp`, FindStackOrEmptySlot 및 itemSlot.slot 대입).

`PacketWriter::WriteInventoryResponse`는 현재 항상 전체 81바이트를 송신한다. 짧은 가변 길이 응답을 기대하면 안 된다. 전체 프레임 길이·실제 버퍼 길이·magic·opcode·flags·status·count를 확인하고, 성공 항목의 슬롯 범위/중복·itemID!=0·quantity>0을 검증한 뒤 완전한 목록을 한 번에 적용한다. 마지막 조건들은 정상 인벤토리 모델의 방어 기준이며 서버가 손상된 blob까지 정규화해 주는 보장은 아니다. 사용하지 않는 후미 항목은 표시/검증하지 않는다. 실패 body는 빈 성공으로 해석하지 않는다.

## 실행·소유권 경로

1. `NetLibrary/NetPacketFilter.cpp::TryDispatch`: 기존 세션, 인증, cheat 상태 확인. 미인증 인벤토리는 통과하지 않는다.
2. `CoreLib/PacketDispatcher.cpp::Process`: flags 0을 NonZoneThreadPool에 전달.
3. `NonZoneHandler.cpp::Process` → `GetInventory`: GetCharacterID(session) > 0만 허용하고, 내부 MSG_INVENTORY_REQ(8)에 세션에서 얻은 캐릭터 ID를 넣는다. ID 0, message pool 부족, queue 거절은 현재 명시 실패 패킷 없이 끝날 수 있다.
4. `CacheLib/Handler.cpp` → `Handler_Inventory.cpp::InventoryRequest`: cache_inventory.Getter(characterID). AVAILABLE이면 itemID!=0인 항목을 선두로 압축하고 슬롯 값은 보존, status1/count n 송신. 나머지 cache 상태는 status0.
5. Cache → `CoreLib/MessageQueueHandler.cpp::InventoryResponse` → PacketWriter → `iocp->SendDataUnique(sessionID, ...)`.

인벤토리 요청 자체에 타인 ID 선택 필드가 없다. 캐릭터 진입은 `Handler.cpp::CharacterStateRequest`의 QUERY_3가 char_id/user_id/channel_id/deleted_at 조건으로 소유권을 검증한다 (`CacheLib/Config.h`). 이 **성공한 서버 응답**의 charID만 세션에 연결해야 한다. 클라이언트가 ENTER 요청으로 보낸 ID를 바로 세션에 기록하면 안 된다.

## 현재 서버의 차단 결함과 최소 후보

### A. 세션 characterID가 끝까지 0

`StateManager.h::CompleteAuth`는 characterID=0으로 초기화한다. 전체 CoreLib 검색에서 `SetCharacterID`는 정의만 있고 호출이 없다. 정상 진입은 `MessageQueueHandler::CharacterStateResponse` → `LobbyZone::ImmigrateChar` → EmigrateChar → 필드 진입이지만 어느 곳도 세션 characterID를 설정하지 않는다. 따라서 현재 소스대로는 정상 월드 진입 뒤에도 INVENTORY_REQ가 GetInventory의 조기 return에 걸린다. 이는 정적 분석 결론이며 실제 신규 요청 실행 결과로 기록하지 않는다.

최소 후보: `CoreLib/LobbyZone.h::ImmigrateChar`가 기존 shard→lobby 락 아래에서 인증된 zone0 세션 확인·중복 로비 캐릭터 거절·m_chars 삽입을 성공시킨 직후, **같은 기존 세션 iterator**의 characterID를 `c.characterID`로 기록한다. 이미 보유한 shard 락에서 SetCharacterID를 재호출하면 동일 shared_mutex 재진입이 되므로 피한다. body->charID!=0과 실패/늦은 응답의 무변경도 확인한다. 새 세션을 생성하지 않는다. 기존 R01의 늦은 ENTER/Disconnect 방어와 락 순서를 유지하며 별도 큐/락을 추가하지 않는다.

### B. 실패 응답 count 미초기화와 writer 경계 없음

`CacheLib/Handler_Inventory.cpp`의 실패 분기는 status=0만 쓰며 count/items를 초기화하지 않는다. `CoreLib/Message.h`의 new[]와 양쪽 MessagePool은 payload를 0으로 지우지 않는다. `PacketWriter::WriteInventoryResponse`는 실패여도 body->itemCount만큼 순회한다. `IPacket::Clear`도 zone/count 메타만 지우며 송신 payload는 그대로다. 그 결과 실패 count가 잔여 값이면 범위 밖 읽기/쓰기, 전체 고정 응답의 미사용 tail은 잔여 데이터 송신 위험이 있다. 현재 정상 조회를 열기 전에 보완해야 한다.

최소 후보:

- `CacheLib/Handler_Inventory.cpp`: Getter 호출 전에 characterID를 지역 값으로 확보하고, 요청/응답이 같은 buffer라는 점을 고려해 Getter 이후 응답 body를 값 초기화한다. 실패는 status0/count0/items0, 성공만 유효 항목을 채운다.
- `CoreLib/PacketWriter.cpp::WriteInventoryResponse`: 송신 body를 0초기화하고 status가 성공인지와 count<=10을 확인한 경우에만 항목을 복사한다. 실패/잘못된 내부 count는 status0/count0으로 정규화하는 등 명시된 안전 실패 정책을 사용한다. 외부 wire/고정 길이는 보존한다.

기존 status0/1·81바이트 계약과 DB/보상 규칙은 유지한다. 후보 검증 후 기존 R01 빌드로 되돌리면 이 세 파일의 C07 diff를 제거할 수 있다. DB 변경은 없고 기존 candidate rollback/build 경로를 재사용한다. 실제 서버 정상 전환은 Coordinator의 로컬 운영 절차로 수행하며 본 분석 역할은 실행하지 않는다.

## 조회 실패와 빈 목록의 차이

`CacheStorageInventory.cpp::LoadFromDB`는 QUERY_5의 characters_inventory 행을 읽는다. 차가운 캐시는 DB_READING을 즉시 반환하므로 **첫 조회가 실패하고 비동기로 로드되는 동작이 가능**하다. DB 로드가 끝나도 그 요청에 자동 성공 패킷을 다시 보내지 않는다. 이후 사용자의 새 조회로 확인한다. DB 행이 없으면 SetEmpty 후 끝나며 자동 생성하지 않는다. 그러므로 신규 테스트 캐릭터의 inventory 행 존재를 가정하지 않는다.

성공/빈 목록은 AVAILABLE cache의 itemID!=0 항목이 0개인 경우(status1/count0). DB 누락·읽는 중·에러는 status0이며 클라이언트가 모두 빈 가방으로 표시하면 안 된다. 프로토콜에 세부 실패 원인은 없으므로 UI는 `조회하지 못했습니다. 잠시 후 다시 조회해 주세요.` 수준으로 표시한다. 원인 확정은 서버 로그/읽기 전용 관찰로 별도 수행한다. 자동 재시딩이나 무한 polling으로 성공을 만들지 않는다.

## 현재 ClientCore 부족한 연결

- `PacketBuilder.CreateInventoryRequestPacket()`은 올바른 헤더 전용 요청을 이미 만들지만 INetworkService/NetworkService와 IViewModelUI/ViewModel의 요청 진입점이 없다.
- Handler의 일반 Deserialize<InventoryResBody>는 최소 body 길이만 검사한다. itemCount를 ViewModel에 넘기지 않고 items[10] 전부 전달해 0/잔여 tail까지 목록에 들어간다. inventory 전용 엄격 parser로 count만큼 반환해야 한다. C06와 같이 simulation routing 전에 검증하면 flags1이 파서를 우회하지 않는다.
- Handler catch는 현재 opcode21의 파싱 실패를 로그로 삼킨다. malformed 정책은 기존 socket disconnect 경계에 전달하는 방향으로 명시하고 관련 회귀를 추가한다.
- `ViewModel.InventoryReceived`는 수명/연결/요청 여부 확인 없이 네트워크 스레드에서 SetInventory 후 UI 이벤트만 dispatcher에 올린다. 다른 응답과 같은 수명·worldReady·single pending 확인을 dispatcher 안에서 한 뒤 목록 갱신과 이벤트를 함께 적용해야 한다.
- `ViewData.GetInventory()`는 lock을 풀고 내부 mutable List를 그대로 반환한다. 작은 snapshot 복사로 반환하고 기존 한 저장소를 재사용한다. Dispose/실패/새 세션에서 다른 캐릭터의 과거 성공 목록을 표시하지 않도록 정리한다.

## 늦은 응답·요청 제한

wire에 requestID·characterID·generation이 없다. 한 연결에서 동시 요청은 하나만 유지한다. 연결 종료/Dispose/월드 준비 전/요청 없는 응답은 적용하지 않는다. timeout 후 요청을 새로 보내면 이전 요청의 늦은 응답과 새 응답을 구분할 수 없다. 이 한계를 세대 변수만으로 해결했다고 보고하지 않는다.

최소 선택지는 timeout 뒤 해당 연결에서 새 요청을 열지 않고 연결 재수립 안내를 하거나, 기존 응답을 수신해 outstanding을 소진하기 전까지 새 요청을 막는 것이다. 완료된 status0 응답 뒤 새 수동 요청은 가능하다. UI 패널 닫기/다시 열기만으로 미완료 요청을 폐기하고 겹친 새 요청을 보내지 않는다. 고유 요청 ID 추가는 C07 조회에 필수인 변경으로 확대하지 않는다.

## 완료 후 한 배치에 넣을 검증

- 엄격 wire: request8/response81, count0/10, 슬롯 비연속, 잘못된 길이·magic·flags·status·count11·중복/범위 밖 슬롯 거절, 실패 응답은 항목 미노출.
- 서버 회귀: 성공한 소유 캐릭터만 세션 결합, 실패/중복/Disconnect 뒤 늦은 ENTER 무변경; inventory cache 실패 응답 count0과 고정 송신 tail0, invalid 내부 count 안전 실패. 기존 R01 관련 회귀 유지.
- ViewModel: 단일 outstanding, worldReady 이전·Dispose/연결 실패 뒤 응답 무변경, dispatcher 전 목록 무변경, empty와 failed 구분, 반환 snapshot 외부 수정이 저장 목록에 영향 없음.
- 실제 UI: 정상 로그인/진입→조회→실서버 응답과 ID/slot/quantity 일치, 빈 목록이면 실제 status1/count0 확인. status0면 실패 안내/수동 재조회, 패널 클릭으로 이동/스킬 오작동 없음, 정상 종료/재접속 후 과거 목록 노출 없음. 실제 비어있거나 실패한 계정에 항목을 주입해 성공을 연출하지 않는다. 채워진 인벤토리는 허용된 기존 계정/행이 확인된 경우에만 실검증하며 그렇지 않으면 packet regression과 실환경 미검증을 구분한다.

아이템 wire에는 이름/아이콘이 없다. 확인된 정적 명칭은 `CoreLib/ItemData.h`: 1 HP포션, 2 MP포션, 3 철검, 4 철방패, 5 가죽갑옷. 알 수 없는 ID는 실제 ID를 표시한다. 정적 명칭 매핑이 장비/소비 기능 지원을 뜻하지 않는다.
