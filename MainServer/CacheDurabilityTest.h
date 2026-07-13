#pragma once
#include <iostream>
#include <cassert>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>

#include <CoreLib/IMessageQueue.h>
#include <CoreLib/Message.h>
#include <CoreLib/MessageTypes.h>
#include <CacheLib/Initializer.h>
#include <CacheLib/Config.h>

#include "IntegrationTestCache.h"

// ================================================================
// Cache Durability (WAL) 테스트 — 재빌드 불필요, 환경변수 2단계
//
//   set Durable=1                            (공통: 테스트 모드 진입)
//   1단계: set CRASH_POINT=WAL_DIRTY  -> 뮤테이션 후 flush 전 abort
//   2단계: set CRASH_VERIFY=1 -> 복구 + 멱등성 검증
//
// 주의: WAL 파일(redo.*)을 삭제하지 않는다. WAL과 DB의 lastLsn은 한 몸이라
//       WAL만 지우면 세그먼트 번호가 1로 리셋돼 새 LSN이 DB의 lastLsn보다
//       작아지고, 복구가 조용히 무력화된다 (LSN epoch 위반).
//       잔재 정리는 반드시 정상 뮤테이션 경로로만 수행한다.
// ================================================================

namespace Cache {

	constexpr uint64_t DUR_CHAR_ID = 19;  
	constexpr uint32_t DUR_ITEM_ID = 77;
	constexpr uint16_t DUR_ITEM_QTY = 5;

	inline static int Test_Cache_Durability_Crash() {
		std::cout << "=== Cache Durability 1단계: 크래시 유발 ===\n";

		MockMessageQueue mockMQ;
		Cache::Initializer init;
		init.Initialize();
		init.InjectDependencies(&mockMQ);
		auto* recvMQ = init.GetMessageQueue();

		// 캐시 채우기 + 이전 실행 잔재 정리 (정상 삭제 경로 — WAL에도 기록되어 안전)
		// 테스트에 쓸 ItemID와 일치하면 제거.
		{
			Core::MsgInventoryReqBody body{};
			body.characterID = DUR_CHAR_ID;
			recvMQ->EnqueueMessage(MakeMsg(Core::MSG_INVENTORY_REQ, 1, body));
			auto* res = mockMQ.WaitFor();
			assert(res != nullptr && "Timeout: initial fetch");
			auto* rb = Core::parseMsgBody<Core::MsgInventoryResBody>(res->GetBuffer());

			for (int i = 0; i < rb->itemCount; ++i) {
				if (rb->items[i].itemID == DUR_ITEM_ID) {
					std::cout << "  잔재 발견 (qty=" << rb->items[i].quantity << ") — 정상 경로로 제거\n";
					Core::MsgInventoryUpdateBody del{};
					del.characterID = DUR_CHAR_ID;
					del.itemID = DUR_ITEM_ID;
					del.op = 2;
					del.change = -static_cast<int16_t>(rb->items[i].quantity);
					recvMQ->EnqueueMessage(MakeMsg(Core::MSG_INVENTORY_UPDATE, 1, del));
					assert(mockMQ.WaitFor() != nullptr && "Timeout: pre-cleanup");
					Block();
					break;
				}
			}
		}
		Block();

		// ADD — 캐시 반영 + WAL 기록 (DB flush는 30초 뒤라 일어나지 않음)
		{
			Core::MsgInventoryUpdateBody body{};
			body.characterID = DUR_CHAR_ID;
			body.itemID = DUR_ITEM_ID;
			body.op = 1;
			body.change = DUR_ITEM_QTY;
			recvMQ->EnqueueMessage(MakeMsg(Core::MSG_INVENTORY_UPDATE, 2, body));

			auto* res = mockMQ.WaitFor();
			assert(res != nullptr && "Timeout: ADD");
			auto* rb = Core::parseMsgBody<Core::MsgInventoryUpdateResBody>(res->GetBuffer());
			assert(rb->resStatus == 1 && "ADD failed");
			assert(rb->itemQuantity == DUR_ITEM_QTY);
			std::cout << "  ADD 반영됨 (캐시+WAL). DB flush 전 크래시 유발\n";
		}

		// 필수 대기: fsync 스레드 주기(50ms)가 지나야 WAL 레코드가 CRT 버퍼를
		// 벗어나 OS 페이지 캐시로 내려감. abort()는 CRT 버퍼를 버리므로,
		// 이 대기 없이 abort하면 레코드가 유실되어 테스트가 성립하지 않음.
		Block();

		CrashPoint("WAL_DIRTY"); // abort — flush(30초) 훨씬 전
		std::cout << "  크래시 미발생 — CRASH_POINT=WAL_DIRTY 설정 확인 필요\n";
		return 1;
	}

	inline static int Test_Cache_Durability_Verify() {
		std::cout << "=== Cache Durability 2단계: 복구 검증 ===\n";

		// 1) WAL 복구 확인 — flush 못 한 변경이 살아 있어야 함
		{
			MockMessageQueue mockMQ;
			Cache::Initializer init;
			init.Initialize(); // Restore: WAL last image의 lsn > DB lastLsn -> 캐시 재적용
			init.InjectDependencies(&mockMQ);
			auto* recvMQ = init.GetMessageQueue();

			Core::MsgInventoryReqBody body{};
			body.characterID = DUR_CHAR_ID;
			recvMQ->EnqueueMessage(MakeMsg(Core::MSG_INVENTORY_REQ, 1, body));
			auto* res = mockMQ.WaitFor();
			assert(res != nullptr && "Timeout: restore fetch");
			auto* rb = Core::parseMsgBody<Core::MsgInventoryResBody>(res->GetBuffer());
			assert(rb->resStatus == 1 && "Data not exist");

			bool found = false;
			for (int i = 0; i < rb->itemCount; ++i) {
				if (rb->items[i].itemID == DUR_ITEM_ID) {
					assert(rb->items[i].quantity == DUR_ITEM_QTY && "WAL 복구 수량 불일치");
					found = true;
				}
			}
			assert(found && "WAL 복구 실패 - flush 전 변경이 유실됨");
			std::cout << "  [PASS] WAL 복구: flush 전 변경 생존 (qty=" << DUR_ITEM_QTY << ")\n";
			Block();
			// init 소멸: dirty flush -> DB의 lastLsn이 WAL 레코드 lsn까지 전진
		}
		Block();

		// 2) 복구 멱등성 — WAL에 레코드가 남아 있지만, 이번엔 dbLSN이 따라잡아
		//   Restore가 스킵해야 함 (중복 적용이면 qty가 2배가 됨)
		{
			MockMessageQueue mockMQ;
			Cache::Initializer init;
			init.Initialize(); // Restore 재실행 — lsn > dbLSN 불충족 -> 스킵
			init.InjectDependencies(&mockMQ);
			auto* recvMQ = init.GetMessageQueue();

			Core::MsgInventoryReqBody body{};
			body.characterID = DUR_CHAR_ID;
			recvMQ->EnqueueMessage(MakeMsg(Core::MSG_INVENTORY_REQ, 2, body));
			auto* res = mockMQ.WaitFor();
			assert(res != nullptr && "Timeout: idempotency fetch");
			auto* rb = Core::parseMsgBody<Core::MsgInventoryResBody>(res->GetBuffer());
			for (int i = 0; i < rb->itemCount; ++i) {
				if (rb->items[i].itemID == DUR_ITEM_ID)
					assert(rb->items[i].quantity == DUR_ITEM_QTY && "복구 중복 적용 발생");
			}
			std::cout << "  [PASS] 복구 멱등성: 재복구에도 수량 불변\n";
			Block();

			// 정리 — 다음 실행을 위해 아이템 제거 (정상 경로: WAL 기록 + lsn 전진)
			Core::MsgInventoryUpdateBody del{};
			del.characterID = DUR_CHAR_ID;
			del.itemID = DUR_ITEM_ID;
			del.op = 2;
			del.change = -static_cast<int16_t>(DUR_ITEM_QTY);
			recvMQ->EnqueueMessage(MakeMsg(Core::MSG_INVENTORY_UPDATE, 3, del));
			assert(mockMQ.WaitFor() != nullptr && "Timeout: cleanup");
			Block();
		}

		std::cout << "=== Cache Durability Tests PASSED ===\n";
		return 0;
	}

	inline static int RunCacheDurabilityTest() {
		if (!Cache::GetEnvVar("CRASH_POINT").empty())
			return Test_Cache_Durability_Crash();
		if (!Cache::GetEnvVar("CRASH_VERIFY").empty())
			return Test_Cache_Durability_Verify();
		std::cout << "CRASH_POINT=WAL_DIRTY 또는 CRASH_VERIFY=1 설정 후 실행\n";
		return 1;
	}
}