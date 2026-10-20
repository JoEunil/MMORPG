# 거래소 판매대금 수령 — 실제 계약과 최소 연결안

2026-10-20 서버 분석. 제품 코드·DB·서버 실행 변경 없음. 분석 기준은 `codex/market-proceeds`, master `0f9b96dee074127d3f6e12a34ce15d44d391bf5e`. 분석 착수 때의 이전 HEAD848f8b는 별도 콘텐츠 후보이며, Coordinator가 master 기반으로 전환한 뒤 새 브랜치를 기준으로 재확인했다. 현재 실행 서버가 이 소스와 같다는 주장은 하지 않는다.

## 결론

판매대금 지급과 중복 수령 방지는 기존 DB에 이미 있다. 새 거래 정책이나 스키마 없이 TCP·Core/Cache 응답·ClientCore·Unity 표시만 연결할 수 있다. 다만 기존 내부 Claim handler는 실패 응답 상관관계와 초기화가 부족하고, 지급액을 DB의 u64에서 u32로 읽는다. 이 경로를 함께 최소 보완해야 한다.

## 확인한 실행 경로

- `CoreLib/PacketTypes.h:10,45`: 외부 TCP는 현재 검색26/27, 내매물28/29까지. 30/31을 사용한 외부 기능은 없다. `MAX_DEFINED_OPCODE`도31로 갱신해야 한다.
- `CoreLib/NonZoneHandler.cpp:196`, `MessageQueueHandler.cpp:92`: MyList는 요청에서 `GetFieldCharacterID`를 사용하고, 응답도 같은 bound character인지 재확인한다. Claim도 이 경계를 그대로 따른다.
- `CoreLib/MessageTypes.h:38,259`: 내부 `MSG_BAZAAR_CLAIM=28`/응답29는 TCP번호와 별개다. 기존 요청은 characterID+listingID, 응답은 status+listingID+u32 diamondClaimed이며 requestID/응답characterID가 없다.
- `CacheLib/Handler.cpp:57` → `Handler_Bazaar.cpp:220` → `Config.h:93` query17 → `Resources/DB/procedure.sql:82` `sp_bazaar_claim(listing,seller)`가 실제 지급 경로다.
- 외부 NonZone Claim 분기, Core Claim 응답 분기, PacketWriter, ClientCore wire/dispatcher/UI 연결은 아직 없다. `MainServer/IntegrationTestBazaar.h:493,586`은 내부 메시지 시험이며 외부 TCP 구현의 증거가 아니다.

## DB의 확정 의미

`Resources/DB/procedure.sql:82-137`:

1. listingID+sellerID+`bazaar.status='SOLD'`가 맞는지 확인한다. 없거나 다른 판매자면 result2.
2. 같은 트랜잭션에서 `bazaar_claim`의 `READY`를 `CLAIMED`로 조건부 UPDATE한다. 변경0행이면 result4이며 지급하지 않는다.
3. 성공 때 `characters_diamond`에 가격만큼 diamond·total_earned를 증가시킨 뒤 COMMIT, result1+지급액을 반환한다. SQL 예외는 ROLLBACK 후 result3+0이다.

`bazaar_claim.listing_id`는 PK이고 InnoDB UPDATE가 동시 요청을 직렬화한다. 먼저 성공한 요청만 지급하며, 다음 요청은 result4를 받는다. Claim은 `bazaar.status`를 CLAIMED로 바꾸지 않는다. MyList의 SOLD행은 수령 후에도 남을 수 있으므로 SOLD만 보고 미수령/수령가능 표시를 합성하면 안 된다. 이 조회 의미를 바꾸는 JOIN·스키마·정산 정책 변경은 이번 최소안에 필요 없다.

DB price·diamond는 BIGINT UNSIGNED다(`Resources/DB/init.sql:81-99`). 지급액은 신규 외부 wire도 **u64**로 연결한다. 내부 Claim 응답의 diamondClaimed도 u64, DB 읽기도 `getUInt64("diamond")`로 좁게 보완한다. u32 그대로면 DB COMMIT 뒤 응답 금액이 절삭되거나 변환 예외가 날 수 있다. 기존 전체 잔액 조회나 구매 메시지 타입까지 확대 수정하지 않는다.

## 권장 외부 wire 후보

현재 외부 헤더는 packed8바이트: magic:u16/length:u32/opcode:u8/flags:u8, 현재 Windows/C# 경로와 같은 little endian. 신규 Claim은 simulation flags0.

| 프레임 | opcode | body | 전체 길이 |
| --- | --- | --- | --- |
| 판매대금 요청 | 30 | requestID:u32, listingID:u64 | 20 |
| 판매대금 응답 | 31 | requestID:u32, listingID:u64, status:u8, diamondClaimed:u64 | 29 |

요청·응답 requestID/listingID는0거부. 요청 header.length·실제길이·flags를 정확히 검사한다. 응답 status는0~4만 허용하고 실패 지급액은 반드시0. 성공 금액0은 기존 가격 정책상 가능하므로 근거 없이 거부하지 않는다. 잘린/추가 바이트, 다른 헤더/flags, 성공 외 지급액은 파서에서 거부한다. 내부 메시지는 외부 packed 형식을 memcpy하는 대신 별도 기존 MsgStruct로 다룬다.

내부 요청에는 기존 필드 뒤 requestID를 추가해 기존 named 초기화 호환성을 유지한다. 응답에는 requestID와 characterID를 추가하고 모든 필드를 value-initialize한다. 내부 legacy fixture의 requestID0 허용과 외부 요청0거부는 분리한다. 내부 ABI가 바뀌므로 MainServer/CoreLib/CacheLib는 같은 소스로 함께 빌드한다.

## 서버 최소 변경 범위와 소유권

- `PacketTypes.h`: opcode30/31, 최대값31, packed 요청/응답·크기 assertion.
- `NonZoneHandler.h/.cpp`: 정확길이/flags/0-ID 검사 → `GetFieldCharacterID(session)`만 seller로 사용. 클라이언트 characterID를 받지 않는다. Acquire/Cache queue 거절 때 같은 requestID/listingID 실패0 응답을 보내고, 직전 bound character 재확인. 기존 queue는 메시지 복사형이므로 원본 Return은 기존 MyList 방식 그대로다.
- `MessageTypes.h`: 앞서 정리한 상관 필드와 u64 지급액. `Handler.cpp` Claim 분기에는 정확한 내부 메시지 길이검사를 둔다.
- `Handler_Bazaar.cpp`: 요청 body를 값으로 복사하고 원본 Message를 한 비동기 작업이 소유한다. 성공/DB0행/SQL예외/DB연결부족/Enqueue 거절을 같은 initialized finish 경로로 합친다. DBWorker에 `onFailure`를 제공하고 거절 때 호출자에서 실패finish, `msg=nullptr` 처리로 바깥 Process의 Return과 중복되지 않게 한다. 기존 Search/MyList의 좁은 패턴을 따른다. 요청 body 포인터를 응답 버퍼 덮어쓰기 뒤 참조하지 않는다.
- `MessageQueueHandler.h/.cpp`: 정확 내부 응답 크기 검사, 응답characterID가 현재 `GetFieldCharacterID(session)`와 일치할 때만 송신. 응답 queue는 메시지를 복사하므로 Cache 원본은 enqueue 성공/거절 모두 정확히1회 Return. 거절은 로그로 남고 클라이언트는 타임아웃으로 처리한다.
- `PacketWriter.h/.cpp`: 상관 ID/status 검증과 실패금액0 정규화, 고정29바이트 응답. 유효하지 않은 내부 결과를 성공으로 표시하지 않는다.

`DBWorker.h`는 work 예외 때 onFailure를 호출하며 rejection 때는 호출자가 처리해야 한다. 기존 `EnqueueMessageWork`는 예외 때 Return만 하고 외부 실패 응답을 만들지 않으므로 Claim 연결에는 그대로 쓰지 않는다. queue/worker 자체 재설계·새 lock/pool·DB 프로시저 수정은 필요 없다.

DB COMMIT 후 연결/응답이 유실되면 첫 요청이 실패했다고 단정할 수 없다. UI에 결과 미확인과 재조회/재요청 가능성을 표시한다. 자동 반복 지급 요청은 만들지 않는다. 수동 재요청이 result4를 받더라도 원래 수령액을 추정하거나 성공 결과를 합성하지 않는다. 그때 DB CAS가 중복지급을 막는다.

## ClientCore·화면 연결

기존 `ClientCore/PacketHelper/Packet.cs`, `Network/Handler.cs`, `Services/NetworkService.cs`, `Interfaces.cs`, `ViewModel.cs`에 필요한 method/event만 추가한다. 원본 ClientCore 한 곳을 수정한 뒤 기존 DLL 경로를 사용한다.

현재 ViewModel의 `BazaarRequestKind`/증가 requestID/단일pending/`_sessionGate`/dispatcher 모델을 확장한다. 선택 listingID를 pending에 함께 보존하고 응답 requestID+kind+listingID+현재 model lifetime+connection/world상태를 dispatcher 안에서 검사한다. 검색/MyList 응답과 Claim 응답이 서로 pending을 해제하면 안 된다. 취소·패널닫기·연결종료가 late response를 무효화해야 하며 성공 UI가 새 캐릭터나 다음 요청에 반영되면 안 된다.

Coordinator와 확정한 최소 API:

- `IViewModelUI.RequestBazaarClaim(ulong listingID)`
- `IViewModelUI.OnBazaarClaimReceived`: `event Action<ulong, byte, ulong>` (listingID, status, diamondClaimed)
- `INetworkService.RequestBazaarClaim(uint requestID, ulong listingID)`
- `IViewModelNet.BazaarClaimReceived(uint requestID, ulong listingID, byte status, ulong diamondClaimed)`
- 기존 `CancelBazaar()`는 Claim pending kind/listing도 무효화한다. 이는 응답 표시 취소이며 이미 큐에 들어간 DB 거래를 취소하지 않는다.

UI는 Claim 요청 시 자기 rows를 보존해 기존 Begin의 행 clearing 영향을 흡수해도 된다. 성공/이미수령4 뒤 같은 UI 세션에서 해당 listingID의 재클릭을 잠근다. 이는 새 claim상태를 서버가 조회해 준다는 뜻이 아니며 재접속 후 미수령 판정을 합성하지 않는다. timeout은 결과 미확인으로 표시하고 자동재요청하지 않는다.

기존 BeginBazaarRequest는 행을 비우므로 Claim에 그대로 쓰면 선택/매물이 사라지는 점을 구현자가 주의한다. 행 보존 여부를 필요한 부분만 분기하고 새 범용 상태 프레임워크는 만들지 않는다. MyList의 본인 SOLD 선택에서만 수령을 요청하며 pending 중 중복클릭을 막는다. 수령상태는 MyList가 제공하지 않으므로 '정산 가능'·예상 잔액을 표시하지 않는다. 성공이면 서버가 반환한 이번 지급액만, 실패2/3/4는 각각 대상없음/처리오류/이미수령또는정산대기없음처럼 확인 가능한 의미만 표시한다.

## 데이터·최소 검증과 복구

2026-10-20 분석 담당의 read-only 접속은 sandbox EACCES로 차단됐다. 이후 Coordinator가 기존 `Inspect-C10MyListings.cjs`를 정식 require_escalated 경로로 실행해 non-cancelled 매물이 **5011=0행, 5012=0행**임을 보고했다. DB쓰기0. 그러므로 두 전용 캐릭터에는 현재 정상 수령 가능한 SOLD+READY 매물이 없다. 분석 담당은 차단을 우회하거나 비밀을 출력하지 않았다.

실제 정산 성공 검증에는 본인 SOLD+READY 및 원래 잔액의 읽기 근거가 필요하다. 없으면 임의 seed·재화주입하지 않고 정확한 테스트데이터 범위를 별도 결정한다. 기존 `IntegrationTestBazaar.h`는 char50~52와 등록/구매/seed·cleanup이 섞여 있으므로 이번 두 전용 계정 테스트 대체로 통째 실행하지 않는다.

동결·독립리뷰 후 직접 영향2~3 시나리오에 합친다: (1) wire·세션소유권·Cache의 정상/예외/queue거절 응답과1회Return, u64 보존 (2) ClientCore 다른listing/kind/취소/늦은응답·중복클릭 경계 (3) 데이터가 있으면 같은 실제 입력 로그인에서1회수령+같은매물재요청을 묶어 UI·DB지급1회·중복result4를 확인. 현재 본인매물0행이므로 실서버에서는 빈목록·거절 결과를 확인하고 성공 경로는 자동fixture/mock 근거로 분리한다. 자동검증을 실제 DB 지급 성공이라고 부르지 않는다. 데이터 부재로 로컬 구현을 중단하지 않는다. 기존 전체 게임회귀는 반복하지 않는다.

새 동작은 DB migration 없이 기존 지급을 노출하므로 rollback은 정상서버종료 후 이전 전체 빌드 복구+이전 ClientCore DLL/클라이언트 복원이다. 이미 정상 COMMIT된 정산은 복구를 이유로 되돌리거나 DB를 초기화하지 않는다. 사용자 PR게시 직전 최종 Slack 승인 경계는 Coordinator가 처리한다.
