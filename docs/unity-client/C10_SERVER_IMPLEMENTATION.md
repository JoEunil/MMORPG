# C10 서버 최소 후보 — 내 거래소 목록

기준: `codex/unity-c10-my-list`, 부모 `0e2adcf`. 이 문서는 후보 구현 및 컴파일 인계이며 독립 리뷰·기능 시험·실서버 적용·게시 완료를 뜻하지 않는다.

바꾼 동작은 기존 query11 SELECT를 인증된 현재 필드 캐릭터의 MARKET 내 매물 요청에 연결하는 것이다. 최소 위치는 PacketTypes/MessageTypes, NonZoneHandler/StateManager, MessageQueueHandler/PacketWriter, ClientContext, Cache Handler와 query11 CASE 및 전용 DB parser다. 기존 C09 검색과 P28 write-once 귀속 코드는 유지한다.

## 계약과 오류 처리

- TCP 요청28: 공통 packed header8 + requestID:u32, 전체12바이트. flags0과 0이 아닌 requestID만 수용한다. 클라이언트 characterID는 받지 않는다.
- 응답29: header8 + requestID:u32/status:u8/count:u16 + 행39×count, 전체15~1185바이트. 행은 listingID:u64/sellerCharacterID:u64/itemID:u32/quantity:u16/price:u64/registeredAt:u64/status:u8다. 응답 body는 별도 `BazaarMyListResponseBody`이고 동일 필드 순서의 기존 packed `BazaarSearchListing`만 공유한다. 자연 정렬 내부 행을 memcpy하지 않는다.
- 기존 내부18/19는 requestID와 내부 owner characterID를 보존한다. `GetFieldCharacterID`는 기존 session shard shared lock 한 번으로 authCompleted/authenticated/userID/characterID/field zone을 검증하고 캐릭터를 얻는다. 늦은 응답은 현재 필드 캐릭터와 내부 owner가 일치할 때만 송신한다.
- query11은 seller 필터·CANCELLED 제외·LIMIT30을 유지하고 CASE만 TRADING0/SOLD1/CLAIMED2/unknown255로 구분한다. SOLD를 수령 가능 또는 수령 완료로 판정하지 않는다. 정렬·페이지·가격·재화·쓰기 경로·schema/procedure/outbox 변경은 없다.
- 정상0행은 성공이다. NULL 결과, SQL/worker 예외, 부분 read/NULL 열, itemID/quantity/status 축소 범위 초과, 0 listingID, 다른 seller, 예상 밖31행은 전체 실패로 초기화하고 requestID/owner만 보존한다. 31행 거절은 기존 SQL LIMIT30 계약 위반에 한정된다.
- DB 비동기 진입 전 요청을 복사한다. DB enqueue 거절·작업 실패는 명시적인 상관 ID/owner 실패 응답을 만들며 단일 Return 소유권을 유지한다. Core 요청 queue 거절도15바이트 실패 응답을 보낸다. 응답 queue 거절은 로그와 반환으로 정리한다. Core/Cache 양쪽2048 메시지 용량에 새 응답 sizeof static_assert를 둔다.

## 검증 인계

추가 fixture11개는 DB parser4개(0/1/30행, NULL/부분 read/범위/owner/31행, stopped worker), Core3개(인증·field·길이·flags·ID, queue 실패/늦은 owner·disconnect, wire값/상태/잘못된 행), TCP4개(분할수신·simulation·짧은/긴 요청)다. 기존 C09 Search/P28 fixture는 삭제하거나 완화하지 않았다.

기능 시험은 모두 미실행이다. 독립 리뷰와 작업 묶음 동결 뒤 Coordinator의 단일 배치에서 기존 검색/P28 영향 회귀와 함께 실행한다. 실DB SELECT·전용 계정 관찰·Unity 입력·서비스 교체는 이 담당이 수행하지 않았다. `TEST_BAZAAR`/CRASH_POINT와 거래·DB 쓰기도 실행하지 않았다.

컴파일은 별도 C10-Delivery/bin 출력과 기존 동일 버전 로컬 의존성 읽기 복사만 사용한다. 최초 IntDir 폴더 누락 C1083 기록은 보존하며, 중단된 재시도는 성공으로 취급하지 않는다. 최종 컴파일 결과와 실제 빌드 산출물은 아래 기록을 따른다.

## 파일 소유권과 복구

제품: CacheLib/Config.h, Handler.h, Handler_Bazaar.cpp, 신규 BazaarMyListResult.h; CoreLib/PacketTypes.h, MessageTypes.h, StateManager.h, NonZoneHandler.h/.cpp, MessageQueueHandler.h/.cpp, PacketWriter.h/.cpp; NetLibrary/ClientContext.cpp. Fixture: UnitTests/CacheLib/CacheStorageTest.cpp, CoreLib/NonZoneThreadPool.cpp, NetLib/NetworkRegression.cpp. 이 문서 외 다른 문서·ClientCore·Unity·site·정책은 편집하지 않았다. 기존 파일의 UTF-8 BOM 여부는 부모 Git blob 기준으로 보존했다.

클라이언트/서버 동시 후보 검증이 필요한 새 TCP28/29 계약이다. 기존 Search 클라이언트는 해당 opcode를 보내지 않으며 기존 계약은 유지된다. 실패 시 정상 종료와 로그/WAL 보존 뒤 기존 C09 서버·DLL 쌍으로 복구한다. 이 SELECT 기능의 rollback에는 DB 행 수정·삭제가 필요 없다. commit/push/PR/Slack은 이 담당이 수행하지 않는다.

## 최종 컴파일 기록

2026-10-19 Release x64의 MainServer와 UnitTests 빌드 모두 exit0이다. 최종 로그는 C10-Delivery/c10-main-build.txt 및 c10-unit-build-e.txt이고, 산출물은 C10-Delivery/bin에 있다. 실행 시험은0회다.

- MainServer.exe SHA256 `3D1011A6B7C8E2FE6A0ECC3C3B739E65EBBC340F4F80B0DF8E213073CAAB402E`.
- UnitTests.exe SHA256 `E59EF6FA904557743E79BCFA1AB5FFA868BB29CE2EDD7FEB3C9B350398D87EE0`.

첫 C1083(IntDir)와 중단된 b, c의 gtest include 부재, d의 LNK1101(MSPDB140 버전) 실패 로그를 보존했다. 이전 C09의 동일 로컬 gtest include/lib 경로를 후보 artifact 설정에 연결하고 /Z7 및 Link debug 정보 생성 비활성으로 최종 e 빌드를 완료했다. 코드 또는 시스템 설정을 변경해 빌드 도구를 우회하지 않았다. 원본 Game은 의존성을 읽기만 했다. 기존 C4244/C4267, UnitTests의 /arch:SSE 무시 D9002 및 NetTimer 정렬 C4744 경고는 남아 있다.

새 fixture11개 및 기존 C09/P28 회귀, TCP 실제 송수신, DB SELECT, 두 계정 실관찰은 모두 이 담당 기준 미실행이다. 독립 리뷰 후 Coordinator가 계획한 배치에서 검증해야 한다. 필수 런타임 DLL은 후보 bin에 읽기 복사/기존 postbuild로 준비했지만 이를 실행 성공 근거로 취급하지 않는다.
