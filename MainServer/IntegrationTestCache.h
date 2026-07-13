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

// 전제 조건:
//   - 로컬 DB (localhost, root/1234, game) 실행 중
//   - characters 테이블에 char_id=11 존재

namespace Cache {
	void Block() {
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
    // Handler 응답을 캡처하는 Mock 큐
	class MockMessageQueue : public Core::IMessageQueue {
		std::mutex m_mutex;
		std::condition_variable m_cv;
		std::queue<Core::Message*> m_received;
	public:
		void EnqueueMessage(Core::Message * msg) override {
			std::lock_guard lock(m_mutex);
			m_received.push(msg);
			m_cv.notify_one();
				
		}
		    // 타임아웃(ms) 내에 응답 대기
		Core::Message * WaitFor(int ms = 5000) {
			std::unique_lock lock(m_mutex);
			if (m_cv.wait_for(lock, std::chrono::milliseconds(ms), [this] { return !m_received.empty(); })) {
				auto* msg = m_received.front();
				m_received.pop();
				return msg;
			}
			return nullptr;
		}
	};
		// 요청 메시지 생성 헬퍼
	template<typename BodyType>
	static Core::Message * MakeMsg(uint16_t msgType, uint64_t sessionID, const BodyType & body) {
		auto* msg = new Core::Message(MESSAGE_LEN);
		auto* st = reinterpret_cast<Core::MsgStruct<BodyType>*>(msg->GetBuffer());
		st->header.messageType = msgType;
		st->header.sessionID = sessionID;
		st->body = body;
		msg->SetLength(sizeof(Core::MsgStruct<BodyType>));
		return msg;
	}
	
	constexpr uint64_t TEST_CHAR_ID = 16;    // DB에 존재하는 char_id
	constexpr uint32_t TEST_ITEM_ID = 99; // 인벤토리에 없는 아이템 (테스트용)
	
	// ---------------------------------------------------------------
	// 캐시 미스 → DB 조회 → 인벤토리 응답
	// ---------------------------------------------------------------
	inline static int Test_Inventory_DBFetch() {
		MockMessageQueue mockMQ;
		Cache::Initializer init;
		init.Initialize();
		init.InjectDependencies(&mockMQ);
		auto* recvMQ = init.GetMessageQueue();

		Core::MsgInventoryReqBody body{};
		body.characterID = TEST_CHAR_ID;
		recvMQ->EnqueueMessage(MakeMsg(Core::MSG_INVENTORY_REQ, 1, body));
		
		auto* res = mockMQ.WaitFor();
		assert(res != nullptr && "Timeout: DB fetch");
		auto* rb = Core::parseMsgBody<Core::MsgInventoryResBody>(res->GetBuffer());
		assert(rb->resStatus == 1 && "DB fetch failed");

		Block();
		std::cout << "[PASS] Test_Inventory_DBFetch (itemCount=" << rb->itemCount << ")\n\n";

		// init 소멸자: dirty → DB flush (상태 저장)
		return 0;
		
	}
	
	// ---------------------------------------------------------------
	// 두 번째 요청 → 캐시 히트
	// ---------------------------------------------------------------
	inline static int Test_Inventory_CacheHit() {
		MockMessageQueue mockMQ;
		Cache::Initializer init;
		init.Initialize();
		init.InjectDependencies(&mockMQ);
		auto* recvMQ = init.GetMessageQueue();
		
		Core::MsgInventoryReqBody body{};
		body.characterID = TEST_CHAR_ID;
		
			        // 1차: DB fetch로 캐시 채우기
		recvMQ->EnqueueMessage(MakeMsg(Core::MSG_INVENTORY_REQ, 1, body));
		assert(mockMQ.WaitFor() != nullptr && "Timeout: initial fetch");
		Block();
			        // 2차: 캐시 히트
		recvMQ->EnqueueMessage(MakeMsg(Core::MSG_INVENTORY_REQ, 1, body));
		auto* res = mockMQ.WaitFor();
		assert(res != nullptr && "Timeout: cache hit");
		auto* rb = Core::parseMsgBody<Core::MsgInventoryResBody>(res->GetBuffer());
		assert(rb->resStatus == 1 && "Data not exist");
		Block();
		
		std::cout << "[PASS] Test_Inventory_CacheHit\n\n";
		return 0;
		
	}
	
	// ---------------------------------------------------------------
	// ADD → UPDATE → DELETE (상태 복원, 소멸자에서 DB flush)
	// ---------------------------------------------------------------
	inline static int Test_InventoryUpdate_Sequence() {
		MockMessageQueue mockMQ;
		Cache::Initializer init;
		init.Initialize();
		init.InjectDependencies(&mockMQ);
		auto* recvMQ = init.GetMessageQueue();
		// 캐시 채우기
		{
			Core::MsgInventoryReqBody body{};
			body.characterID = TEST_CHAR_ID;
			recvMQ->EnqueueMessage(MakeMsg(Core::MSG_INVENTORY_REQ, 1, body));
			assert(mockMQ.WaitFor() != nullptr && "Timeout: initial fetch");
		}
		Block();
		// ADD item TEST_ITEM_ID, qty=5
		{
			Core::MsgInventoryUpdateBody body{};
			body.characterID = TEST_CHAR_ID;
			body.itemID = TEST_ITEM_ID;
			body.op = 1;
			body.change = 5;
			recvMQ->EnqueueMessage(MakeMsg(Core::MSG_INVENTORY_UPDATE, 2, body));
			
			auto* res = mockMQ.WaitFor();
			assert(res != nullptr && "Timeout: ADD");
			auto* rb = Core::parseMsgBody<Core::MsgInventoryUpdateResBody>(res->GetBuffer());
			assert(rb->resStatus == 1 && "ADD failed");
			assert(rb->itemID == TEST_ITEM_ID);
			assert(rb->itemQuantity == 5);
			Block();
			std::cout << "  ADD PASSED (slot=" << (int)rb->slot << ", qty=" << rb->itemQuantity << ")\n\n";
		}
		Block();
		// UPDATE item TEST_ITEM_ID, change=+3 → qty=8
		{
			Core::MsgInventoryUpdateBody body{};
			body.characterID = TEST_CHAR_ID;
			body.itemID = TEST_ITEM_ID;
			body.op = 2;
			body.change = 3; // update + 3
			recvMQ->EnqueueMessage(MakeMsg(Core::MSG_INVENTORY_UPDATE, 3, body));
			
			auto* res = mockMQ.WaitFor();
			assert(res != nullptr && "Timeout: UPDATE");
			auto* rb = Core::parseMsgBody<Core::MsgInventoryUpdateResBody>(res->GetBuffer());
			assert(rb->resStatus == 1 && "UPDATE failed");
			assert(rb->itemQuantity == 8);
			Block();
			std::cout << "  UPDATE PASSED (qty=" << rb->itemQuantity << ")\n";
		}
		Block();
		// DELETE item TEST_ITEM_ID
		{
			Core::MsgInventoryUpdateBody body{};
			body.characterID = TEST_CHAR_ID;
			body.itemID = TEST_ITEM_ID;
			body.op = 2;
			body.change = -8;
			recvMQ->EnqueueMessage(MakeMsg(Core::MSG_INVENTORY_UPDATE, 4, body));
			
			auto* res = mockMQ.WaitFor();
			assert(res != nullptr && "Timeout: DELETE");
			auto* rb = Core::parseMsgBody<Core::MsgInventoryUpdateResBody>(res->GetBuffer());
			assert(rb->resStatus == 1 && "DELETE failed");
			Block();
			std::cout << "  DELETE PASSED (qty=" << rb->itemQuantity << ")\n";
		}

		Block();
		std::cout << "[PASS] Test_InventoryUpdate_Sequence\n\n";
		return 0;
		
	}
	
	// ---------------------------------------------------------------
	// 캐시 업데이트 -> DB Flush -> 재연결 -> 상태 조회 (업데이트 된 상태 저장되었는지 확인) 
	// ---------------------------------------------------------------
	inline static int Test_DB_Flush() {
		{
			MockMessageQueue mockMQ;
			Cache::Initializer init;
			init.Initialize();
			init.InjectDependencies(&mockMQ);
			auto* recvMQ = init.GetMessageQueue();

			Core::MsgInventoryUpdateBody body{};
			body.characterID = TEST_CHAR_ID; 
			body.itemID = 33;
			body.op = 1;
			body.change = 34;
			recvMQ->EnqueueMessage(MakeMsg(Core::MSG_INVENTORY_UPDATE, 1, body));

			auto* res = mockMQ.WaitFor();
			assert(res != nullptr && "Timeout: cache miss update");
			auto* rb = Core::parseMsgBody<Core::MsgInventoryUpdateResBody>(res->GetBuffer());
			assert(rb->resStatus == 1 && "UPDATE failed");
			assert(rb->itemQuantity == 34 && "Expected item quantity = 34");
		}
		Block();
		// init 소멸자에서 DB flush 
		{
			MockMessageQueue mockMQ;
			Cache::Initializer init;
			init.Initialize();
			init.InjectDependencies(&mockMQ);
			auto* recvMQ = init.GetMessageQueue();
			{
				Core::MsgInventoryReqBody body{};
				body.characterID = TEST_CHAR_ID;
				recvMQ->EnqueueMessage(MakeMsg(Core::MSG_INVENTORY_REQ, 2, body));

				auto* res = mockMQ.WaitFor();
				assert(res != nullptr && "Timeout: cache miss update");
				auto* rb = Core::parseMsgBody<Core::MsgInventoryResBody>(res->GetBuffer());
				assert(rb->resStatus == 1 && "Data not exist");
				assert(rb->items[0].itemID == 33 && "Expected item id = 33");
				assert(rb->items[0].quantity == 34 && "Expected item quantity = 34");

			}
			Block();

			{
				Core::MsgInventoryUpdateBody body{};
				body.characterID = TEST_CHAR_ID;
				body.itemID = 33;
				body.op = 2;
				body.change = -34;
				recvMQ->EnqueueMessage(MakeMsg(Core::MSG_INVENTORY_UPDATE, 2, body));

				auto* res = mockMQ.WaitFor();
				assert(res != nullptr && "Timeout: cache miss update");
				auto* rb = Core::parseMsgBody<Core::MsgInventoryUpdateResBody>(res->GetBuffer());
				assert(rb->resStatus == 1 && "Delete Succeed");
			}
		}
		Block();
		std::cout << "[PASS] Test_DB_Flush\n\n";
		return 0;

	}

	// ---------------------------------------------------------------
	// 캐시 set -> lru 리스트 초과 -> lru evict 발생
	// ---------------------------------------------------------------
	inline static int Test_LRU_Eviction() {
		MockMessageQueue mockMQ;
		Cache::Initializer init;
		init.Initialize();
		init.InjectDependencies(&mockMQ);
		auto* recvMQ = init.GetMessageQueue();

		auto fetch = [&](uint64_t charID) {
			Core::MsgInventoryReqBody body{};
			body.characterID = charID;
			recvMQ->EnqueueMessage(MakeMsg(Core::MSG_INVENTORY_REQ, charID, body));
			auto* res = mockMQ.WaitFor();
			assert(res != nullptr);
			return Core::parseMsgBody<Core::MsgInventoryResBody>(res->GetBuffer())->resStatus;
			};

		Block();
		// shard 1 (charID & 0xF == 1): 1, 17, 33
		// LRU 크기를 2로 설정, shard는 16
		std::cout << "fetch 1\n";  fetch(1);   // shard1: {1}
		Block();
		std::cout << "fetch 17\n"; fetch(17);  // shard1: {17,1} 17=MRU 1=LRU
		Block();
		std::cout << "fetch 1\n";  fetch(1);   // shard1: {1,17} 1=MRU 17=LRU (갱신)
		Block();
		std::cout << "fetch 33\n"; fetch(33);  // shard1 full → evict 17 → {33,1}
		// ↑ 여기서 [LRU] evict 출력 기대
		Block();

		std::cout << "fetch 17 (cache miss expected)\n";
		fetch(17); // miss → DB 재조회
		// ↑ 여기서도 [LRU] evict 출력

		Block();
		std::cout << "[PASS] Test_LRU_Eviction\n\n";
		return 0;
	}

	// ---------------------------------------------------------------
	// 전체 실행
	// ---------------------------------------------------------------
	inline static int RunAllCacheTests() {
		std::cout << "=== CacheLib Integration Tests ===\n";
		Test_Inventory_DBFetch();
		Test_Inventory_CacheHit();
		Test_InventoryUpdate_Sequence();
		Test_DB_Flush();
		Test_LRU_Eviction();
		std::cout << "=== All Tests PASSED ===\n";
		return 0;
		
	}
}