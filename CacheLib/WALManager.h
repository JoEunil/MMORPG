#pragma once

#include <thread>
#include <atomic>
#include <map>

#include <BaseLib/WAL.h>
#include <CoreLib/LoggerGlobal.h>
#include <CacheLib/CacheStorageInventory.h>
#include <CacheLib/DBConnectionPool.h>
#include <CacheLib/DBConnectionGame.h>

namespace Cache {
	struct WalInventoryRecord {
		uint64_t      characterID;
		InventoryData data;   // lastLSN 포함
	};
	enum WalType : uint8_t {
		INVENTORY = 1
	};
	class WALManager {
		// cache flush, flush dispatcher 뒤에 초기화.
		std::optional<Base::WAL> m_wal; // 생성 지연
		std::thread m_fsyncThread;

		std::unordered_map<uint32_t, uint32_t> m_segRefCnt;    // seg -> 보존 필요 레코드 수, 전역
		std::unordered_map<uint64_t, uint64_t> m_unflushedInventory; // inventory 전용 (key = charID)
		std::mutex m_mutex;

		std::atomic<bool> m_blocked = false;
		// file write 실패한 경우 일정 기간동안 Write를 block 하기 위함.
		// 실패 시 drop, wal 없이 작업을 통과 시키도록.
		std::atomic<bool> m_running = false;

		const int RESUME_TRIGGER = 200; // 10초+

		std::unordered_map<uint64_t, std::pair<uint64_t, WalInventoryRecord>> m_lastImageInventory;
		CacheStorageInventory* cache_inventory;
		DBConnectionPool<DBConnectionGame>* connectionPoolGame;

		void CollectInvenotry(const Base::WALHeader& header, const uint8_t* payload) {
			if (header.length != sizeof(WalInventoryRecord))
				return;
			WalInventoryRecord rec;
			std::memcpy(&rec, payload, sizeof(WalInventoryRecord));
			m_lastImageInventory[rec.characterID] = { header.lsn, rec };
		}
		void Collect(const Base::WALHeader& header, const uint8_t* payload) {
			switch (header.type) {
			case WalType::INVENTORY: CollectInvenotry(header, payload); break;
			}
		}

		void RestoreInventory() {
			if (m_lastImageInventory.empty())
				return;
			auto* conn = connectionPoolGame->Acquire();   // 부팅 시점 동기 접근 — 블로킹 OK
			if (conn == nullptr)
				return;
			for (auto& [key, val] : m_lastImageInventory) {
				// DB blob 로드 → lastLSN 비교 (기존 QUERY_5 재사용)
				uint64_t lsn = val.first;

				WalInventoryRecord rec = val.second;
				auto res = conn->ExecuteSelect(5, key);
				uint64_t dbLSN = 0;
				if (res && res->next()) {
					InventoryData tmp = EMPTY_INVENTORY;             
					std::istream* blob = res->getBlob("inventory");
					if (blob) blob->read(reinterpret_cast<char*>(&tmp), sizeof(InventoryData));
					dbLSN = tmp.lastLsn;
				}
				// 레코드는 자기 lsn을 품을 수 없어(발급 전 복사) payload의 lastLsn은 한 세대 전 값.
				// header lsn으로 보정해야 flush 정산과 재부팅 비교가 여기서 수렴한다.
				rec.data.lastLsn = lsn;
				if (lsn > dbLSN) {
					cache_inventory->RestoreEntry(key, rec.data);  // 캐시 Insert + dirty 마킹
				}
			}
			connectionPoolGame->Return(conn);
			m_lastImageInventory.clear();
		}
		void Restore() {
			RestoreInventory();
		}

		void ThreadFunc() {
			auto tid = std::this_thread::get_id();
			std::stringstream ss;
			ss << tid;
			Core::sysLogger->LogInfo("WAL manager", "WAL manager thread started", "threadID", ss.str());

			int resumeCounter = 0;
			while (m_running.load(std::memory_order_relaxed)) {
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				m_wal->Fsync();
				if (m_blocked.load(std::memory_order_relaxed)) {
					if (++resumeCounter > RESUME_TRIGGER) {
						m_blocked.store(false, std::memory_order_relaxed);
						resumeCounter = 0;
					}
				}
			}
			m_wal->Fsync();
			Core::sysLogger->LogInfo("WAL manager", "WAL manager thread stopped", "threadID", ss.str());
		}

		void Initialize(CacheStorageInventory* ci, DBConnectionPool<DBConnectionGame>* p) {
			cache_inventory = ci;
			connectionPoolGame = p;
			m_wal.emplace("redo", 1000000, [this](const Base::WALHeader& h, const uint8_t* p) { Collect(h, p); });
			Restore();   // 수집된 last-image를 캐시에 적용
			m_running.store(true, std::memory_order_relaxed);
			m_fsyncThread = std::thread(&WALManager::ThreadFunc, this);
			cache_inventory->SetWalFn([this](uint64_t id, const InventoryData& d) {
				return this->WriteInventory(id, d);  
				});
		}

		bool IsReady() {
			if (!cache_inventory) {
				Core::sysLogger->LogError("WAL manager", "cache_inventory not Initialized");
				return false;
			}
			return true;
		}
		uint64_t Write(const uint8_t* payload, size_t len, uint8_t type) {
			if (m_blocked.load(std::memory_order_relaxed))
				return 0;
			uint64_t lsn = m_wal->Write(payload, len, type);
			if (lsn == 0)
				m_blocked.store(true, std::memory_order_relaxed);
			return lsn;
		}
		friend class Initializer;
	public:
		WALManager() {
		}
		~WALManager()
		{
			m_running.store(false, std::memory_order_relaxed);

			if (m_fsyncThread.joinable())
				m_fsyncThread.join();
		}
		uint64_t WriteInventory(uint64_t characterID, const InventoryData& data) {
			WalInventoryRecord record;
			record.characterID = characterID;
			record.data = data;
			// WAL::Write()에서 바로 복사해서 쓰기 때문에 수명 문제 없음
			uint64_t lsn  = Write(reinterpret_cast<const uint8_t*>(&record), sizeof(WalInventoryRecord), WalType::INVENTORY);


			if (lsn != 0) {
				std::lock_guard<std::mutex> lock(m_mutex);
				auto it = m_unflushedInventory.find(characterID);
				if (it != m_unflushedInventory.end())
					m_segRefCnt[static_cast<uint32_t>(it->second >> 32)]--;
				m_unflushedInventory[characterID] = lsn;
				m_segRefCnt[static_cast<uint32_t>(lsn >> 32)]++;
			}
			return lsn;
		}
		void OnInventoryFlushed(uint64_t characterID, uint64_t flushedLsn) {
			std::lock_guard<std::mutex> lock(m_mutex);
			auto it = m_unflushedInventory.find(characterID);
			if (it == m_unflushedInventory.end())
				return;                    // 이미 정산됨
			if (it->second > flushedLsn) 
				return;					// flush 중 재수정
			m_segRefCnt[static_cast<uint32_t>(it->second >> 32)]--;
			m_unflushedInventory.erase(it);
		}
	};
}