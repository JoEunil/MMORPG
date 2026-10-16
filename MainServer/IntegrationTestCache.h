#pragma once
#include <stdexcept>
#include <string>

 #include <queue>
 #include <mutex>
 #include <condition_variable>
 #include <chrono>
 #include <cstring>
 #include <memory>
 #include <vector>
 #include <algorithm>
 #include <thread>

#include <CoreLib/IMessageQueue.h>
#include <CoreLib/Message.h>
#include <CoreLib/MessageTypes.h>
#include <CacheLib/Initializer.h>
#include <CacheLib/Config.h>

// 전제 조건:
//   - CacheLib/Config.h의 DB 환경변수에 해당하는 테스트 DB 실행 중
//   - characters 테이블에 TEST_CHAR_ID 및 LRU 테스트용 캐릭터 존재

namespace Cache {
	void Block() {
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
    // Handler 응답을 캡처하는 Mock 큐
	class MockMessageQueue : public Core::IMessageQueue {
		std::mutex m_mutex;
		std::condition_variable m_cv;
		std::queue<Core::Message*> m_received;
		std::vector<std::unique_ptr<Core::Message>> m_ownedResponses;
	public:
		bool EnqueueMessage(Core::Message * msg) override {
			// The handler returns its message to the pool immediately after enqueue.
			auto copy = std::make_unique<Core::Message>(msg->GetLength());
			std::memcpy(copy->GetBuffer(), msg->GetBuffer(), msg->GetLength());
			copy->SetLength(msg->GetLength());
			std::lock_guard lock(m_mutex);
			m_received.push(copy.get());
			m_ownedResponses.push_back(std::move(copy));
			m_cv.notify_one();
			return true;
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
		// InMemoryQueue copies the request before EnqueueMessage returns.
		thread_local Core::Message request(MESSAGE_LEN);
		auto* msg = &request;
		auto* st = reinterpret_cast<Core::MsgStruct<BodyType>*>(msg->GetBuffer());
		st->header.messageType = msgType;
		st->header.sessionID = sessionID;
		st->body = body;
		msg->SetLength(sizeof(Core::MsgStruct<BodyType>));
		return msg;
	}

	template<typename BodyType>
	static Core::Message* WaitForResponse(MockMessageQueue& responses, uint16_t messageType,
		uint64_t sessionID, int timeoutMs = 5000) {
		auto* response = responses.WaitFor(timeoutMs);
		if (!(response != nullptr))
		    throw std::runtime_error("Timeout waiting for cache response");
		if (!(response->GetLength() >= sizeof(Core::MsgStruct<BodyType>)))
		    throw std::runtime_error("Cache response too short");
		const auto* header = Core::parseMsgHeader(response->GetBuffer());
		if (!(header->messageType == messageType))
		    throw std::runtime_error("Unexpected cache response type");
		if (!(header->sessionID == sessionID))
		    throw std::runtime_error("Unexpected cache response session");
		return response;
	}

	// A cold Getter starts an asynchronous DB read and immediately reports status=0.
	// Consume each response before retrying; never replay a mutation to warm the cache.
	static Core::Message* PollInventoryUntilAvailable(Core::IMessageQueue* requests,
		MockMessageQueue& responses, uint64_t characterID, uint64_t sessionID,
		std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
		auto deadline = std::chrono::steady_clock::now() + timeout;
		Core::MsgInventoryReqBody request{};
		request.characterID = characterID;
		while (std::chrono::steady_clock::now() < deadline) {
			if (!(requests->EnqueueMessage(MakeMsg(Core::MSG_INVENTORY_REQ, sessionID, request))))
			    throw std::runtime_error("Cache request enqueue failed");
			auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
				deadline - std::chrono::steady_clock::now()).count();
			if (!(remaining > 0))
			    throw std::runtime_error("Inventory polling deadline expired");
			auto* response = WaitForResponse<Core::MsgInventoryResBody>(responses,
				Core::MSG_INVENTORY_RES, sessionID, static_cast<int>(remaining));
			const auto* body = Core::parseMsgBody<Core::MsgInventoryResBody>(response->GetBuffer());
			if (!(body->resStatus <= 1))
			    throw std::runtime_error("Unexpected inventory response status");
			if (body->resStatus == 1) {
				if (!(body->itemCount <= Core::MAX_INVENTORY_ITEMS))
				    throw std::runtime_error("Inventory item count out of range");
				return response;
			}
			std::this_thread::sleep_until((std::min)(deadline,
				std::chrono::steady_clock::now() + std::chrono::milliseconds(10)));
		}
		throw std::runtime_error("Inventory remained unavailable for character " + std::to_string(characterID));
	}
	
	constexpr uint64_t TEST_CHAR_ID = 16;    // DB에 존재하는 char_id
	constexpr uint32_t TEST_ITEM_ID = 99; // 인벤토리에 없는 아이템 (테스트용)
	
	// ---------------------------------------------------------------
	// 캐시 미스 → DB 조회 → 인벤토리 응답
	// ---------------------------------------------------------------
	inline static int Test_Inventory_DBFetch() {
		MockMessageQueue mockMQ;
		Cache::Initializer init{ Cache::DEFAULT_WAL_FSYNC_INTERVAL, "wal/cache-integration/redo" };
		init.Initialize();
		init.InjectDependencies(&mockMQ);
		auto* recvMQ = init.GetMessageQueue();

		auto* res = PollInventoryUntilAvailable(recvMQ, mockMQ, TEST_CHAR_ID, 1);
		auto* rb = Core::parseMsgBody<Core::MsgInventoryResBody>(res->GetBuffer());
		if (!(rb->resStatus == 1))
		    throw std::runtime_error("DB fetch failed");

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
		Cache::Initializer init{ Cache::DEFAULT_WAL_FSYNC_INTERVAL, "wal/cache-integration/redo" };
		init.Initialize();
		init.InjectDependencies(&mockMQ);
		auto* recvMQ = init.GetMessageQueue();
		
		Core::MsgInventoryReqBody body{};
		body.characterID = TEST_CHAR_ID;
		
			        // 1차: DB fetch로 캐시 채우기
		PollInventoryUntilAvailable(recvMQ, mockMQ, TEST_CHAR_ID, 1);
		Block();
			        // 2차: 캐시 히트
		if (!(recvMQ->EnqueueMessage(MakeMsg(Core::MSG_INVENTORY_REQ, 1, body))))
		    throw std::runtime_error("Cache request enqueue failed");
		auto* res = WaitForResponse<Core::MsgInventoryResBody>(mockMQ, Core::MSG_INVENTORY_RES, 1);
		auto* rb = Core::parseMsgBody<Core::MsgInventoryResBody>(res->GetBuffer());
		if (!(rb->resStatus == 1))
		    throw std::runtime_error("Data not exist");
		Block();
		
		std::cout << "[PASS] Test_Inventory_CacheHit\n\n";
		return 0;
		
	}
	
	// ---------------------------------------------------------------
	// ADD → UPDATE → DELETE (상태 복원, 소멸자에서 DB flush)
	// ---------------------------------------------------------------
	inline static int Test_InventoryUpdate_Sequence() {
		MockMessageQueue mockMQ;
		Cache::Initializer init{ Cache::DEFAULT_WAL_FSYNC_INTERVAL, "wal/cache-integration/redo" };
		init.Initialize();
		init.InjectDependencies(&mockMQ);
		auto* recvMQ = init.GetMessageQueue();
		// 캐시 채우기
		PollInventoryUntilAvailable(recvMQ, mockMQ, TEST_CHAR_ID, 1);
		Block();
		// ADD item TEST_ITEM_ID, qty=5
		{
			Core::MsgInventoryUpdateBody body{};
			body.characterID = TEST_CHAR_ID;
			body.itemID = TEST_ITEM_ID;
			body.op = 1;
			body.change = 5;
			if (!(recvMQ->EnqueueMessage(MakeMsg(Core::MSG_INVENTORY_UPDATE, 2, body))))
			    throw std::runtime_error("Cache request enqueue failed");
			
			auto* res = WaitForResponse<Core::MsgInventoryUpdateResBody>(mockMQ, Core::MSG_INVENTORY_UPDATE_RES, 2);
			auto* rb = Core::parseMsgBody<Core::MsgInventoryUpdateResBody>(res->GetBuffer());
			if (!(rb->resStatus == 1))
			    throw std::runtime_error("ADD failed");
			if (!(rb->itemID == TEST_ITEM_ID))
			    throw std::runtime_error("Unexpected inventory item");
			if (!(rb->itemQuantity == 5))
			    throw std::runtime_error("ADD quantity mismatch");
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
			if (!(recvMQ->EnqueueMessage(MakeMsg(Core::MSG_INVENTORY_UPDATE, 3, body))))
			    throw std::runtime_error("Cache request enqueue failed");
			
			auto* res = WaitForResponse<Core::MsgInventoryUpdateResBody>(mockMQ, Core::MSG_INVENTORY_UPDATE_RES, 3);
			auto* rb = Core::parseMsgBody<Core::MsgInventoryUpdateResBody>(res->GetBuffer());
			if (!(rb->resStatus == 1))
			    throw std::runtime_error("UPDATE failed");
			if (!(rb->itemQuantity == 8))
			    throw std::runtime_error("UPDATE quantity mismatch");
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
			if (!(recvMQ->EnqueueMessage(MakeMsg(Core::MSG_INVENTORY_UPDATE, 4, body))))
			    throw std::runtime_error("Cache request enqueue failed");
			
			auto* res = WaitForResponse<Core::MsgInventoryUpdateResBody>(mockMQ, Core::MSG_INVENTORY_UPDATE_RES, 4);
			auto* rb = Core::parseMsgBody<Core::MsgInventoryUpdateResBody>(res->GetBuffer());
			if (!(rb->resStatus == 1))
			    throw std::runtime_error("DELETE failed");
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
			Cache::Initializer init{ Cache::DEFAULT_WAL_FSYNC_INTERVAL, "wal/cache-integration/redo" };
			init.Initialize();
			init.InjectDependencies(&mockMQ);
			auto* recvMQ = init.GetMessageQueue();

			PollInventoryUntilAvailable(recvMQ, mockMQ, TEST_CHAR_ID, 1);
			Core::MsgInventoryUpdateBody body{};
			body.characterID = TEST_CHAR_ID; 
			body.itemID = 33;
			body.op = 1;
			body.change = 34;
			if (!(recvMQ->EnqueueMessage(MakeMsg(Core::MSG_INVENTORY_UPDATE, 1, body))))
			    throw std::runtime_error("Cache request enqueue failed");

			auto* res = WaitForResponse<Core::MsgInventoryUpdateResBody>(mockMQ, Core::MSG_INVENTORY_UPDATE_RES, 1);
			auto* rb = Core::parseMsgBody<Core::MsgInventoryUpdateResBody>(res->GetBuffer());
			if (!(rb->resStatus == 1))
			    throw std::runtime_error("UPDATE failed");
			if (!(rb->itemQuantity == 34))
			    throw std::runtime_error("Expected item quantity = 34");
		}
		Block();
		// init 소멸자에서 DB flush 
		{
			MockMessageQueue mockMQ;
			Cache::Initializer init{ Cache::DEFAULT_WAL_FSYNC_INTERVAL, "wal/cache-integration/redo" };
			init.Initialize();
			init.InjectDependencies(&mockMQ);
			auto* recvMQ = init.GetMessageQueue();
			{
				auto* res = PollInventoryUntilAvailable(recvMQ, mockMQ, TEST_CHAR_ID, 2);
				auto* rb = Core::parseMsgBody<Core::MsgInventoryResBody>(res->GetBuffer());
				if (!(rb->resStatus == 1))
				    throw std::runtime_error("Data not exist");
				bool found = false;
				for (int i = 0; i < rb->itemCount; ++i) {
					if (rb->items[i].itemID == 33) {
						if (!(rb->items[i].quantity == 34))
						    throw std::runtime_error("Expected item quantity = 34");
						found = true;
					}
				}
				if (!(found))
				    throw std::runtime_error("Expected flushed item id = 33");

			}
			Block();

			{
				Core::MsgInventoryUpdateBody body{};
				body.characterID = TEST_CHAR_ID;
				body.itemID = 33;
				body.op = 2;
				body.change = -34;
				if (!(recvMQ->EnqueueMessage(MakeMsg(Core::MSG_INVENTORY_UPDATE, 2, body))))
				    throw std::runtime_error("Cache request enqueue failed");

				auto* res = WaitForResponse<Core::MsgInventoryUpdateResBody>(mockMQ, Core::MSG_INVENTORY_UPDATE_RES, 2);
				auto* rb = Core::parseMsgBody<Core::MsgInventoryUpdateResBody>(res->GetBuffer());
				if (!(rb->resStatus == 1))
				    throw std::runtime_error("Delete Succeed");
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
		if (MAX_CACHE_SIZE != 2) {
			std::cout << "[SKIP] LRU fetch scenario requires MAX_CACHE_SIZE=2; use CacheStorageTest for eviction assertions.\n";
			return 0;
		}
		MockMessageQueue mockMQ;
		Cache::Initializer init{ Cache::DEFAULT_WAL_FSYNC_INTERVAL, "wal/cache-integration/redo" };
		init.Initialize();
		init.InjectDependencies(&mockMQ);
		auto* recvMQ = init.GetMessageQueue();

		auto fetch = [&](uint64_t charID) {
			auto* res = PollInventoryUntilAvailable(recvMQ, mockMQ, charID, charID);
			auto status = Core::parseMsgBody<Core::MsgInventoryResBody>(res->GetBuffer())->resStatus;
			if (!(status == 1))
			    throw std::runtime_error("LRU scenario inventory fetch failed");
			return status;
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
		std::cout << "[INFO] LRU fetch sequence completed; confirm eviction in logs.\n\n";
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
