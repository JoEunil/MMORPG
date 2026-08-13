#include "CacheStorageInventory.h"

#include <mysqlconn/include/mysql/jdbc.h>
#include <CoreLib/Message.h>
#include "DBWorker.h"
#include "CacheFlush.h"

namespace Cache {
    bool CacheStorageInventory::RestoreEntry(uint64_t characterID, InventoryData& rec) {
        Key key;
        key.characterID = characterID;
        auto shardIndex = characterID & SHARD_SIZE_MASK;
        auto& shard = m_shards[shardIndex];
        std::lock_guard<std::mutex> lock(shard.mutex);

        auto it = shard.cache_data.find(key);
        if (it != shard.cache_data.end()) {
            return false;
        }

        shard.lru_list.push_front(key);
        shard.lru_pos[key] = shard.lru_list.begin();

        auto& item = shard.cache_data[key];
        item.data = rec;
        item.lastModified = CacheTimer::GetTimeMS();
        item.status = CACHE_STATUS::AVAILABLE;
        shard.dirty_list.insert(key);

        if (shard.lru_list.size() >= MAX_CACHE_SIZE) {
            const auto oldKey = shard.lru_list.back();
            shard.lru_pos.erase(oldKey);
            shard.lru_list.pop_back();

            auto it = shard.dirty_list.find(oldKey);
            if (it != shard.dirty_list.end()) {
                shard.cache_data[oldKey].status = CACHE_STATUS::EVICTING;
                m_flushFn(oldKey, shard.cache_data[oldKey]);
                shard.dirty_list.erase(oldKey);
            }
            else {
                shard.cache_data.erase(oldKey);
            }
        }
        return true;

    }
    CACHE_STATUS CacheStorageInventory::LoadFromDB(uint16_t shardIndex, Key& key) {
        auto status = TrySetReading(shardIndex, key);
        if (status != CACHE_STATUS::EMPTY) // READING
            return status;

        dbWorker->Enqueue([=](DBConnectionGame* conn) {
            Result result;
            // blob이 구버전(ring 필드 이전)이라 짧게 읽혀도 나머지가 쓰레기로 남지 않도록 선초기화
            result.data = EMPTY_INVENTORY;
            auto res = conn->ExecuteSelect(5, key.characterID);
            if (!res || !res->next()) {
                Core::gameLogger->LogInfo("cache storage inventory", "inventory not found in DB", "char_id", key.characterID);
                SetEmpty(shardIndex, key);  // READING → EMPTY
                return CACHE_STATUS::EMPTY;
            }

            std::istream* blobStream = res->getBlob("inventory");
            if (blobStream) {
                blobStream->read(
                    reinterpret_cast<char*>(&result.data),
                    sizeof(InventoryData)
                );
            }
            // blob 손상·구버전 방어: ring 인덱스가 범위를 벗어나면 ring만 초기화
            // (tail이 RING_SIZE 이상이면 push 시 recentEventIds[tail] 범위 밖 쓰기가 발생)
            if (result.data.head >= RING_SIZE || result.data.tail >= RING_SIZE) {
                Core::errorLogger->LogError("cache storage inventory", "invalid ring index in blob, reset ring", "char_id", key.characterID);
                std::memset(result.data.recentEventIds, 0, sizeof(result.data.recentEventIds));
                result.data.head = 0;
                result.data.tail = 0;
                result.data.lastOp = false;
                result.data.lastLsn = 0;
            }
            result.rollbackCnt = 0;
            Insert(shardIndex, key, result);  // READING → AVAILABLE
        });
        return CACHE_STATUS::DB_READING;
    }

    Result5 CacheStorageInventory::Getter(uint64_t characterID) {
        Key key;
        Result result;
        key.characterID = characterID;
        auto shardIndex = characterID & SHARD_SIZE_MASK;
        switch (TryGet(key.characterID & SHARD_SIZE_MASK, key, result))
        {
        case CACHE_STATUS::AVAILABLE:
            break;
        case CACHE_STATUS::DB_READING:
            result.status = CACHE_STATUS::DB_READING;
            return result;
        case CACHE_STATUS::EVICTING:
            result.status = CACHE_STATUS::EVICTING;
            return result;
        case CACHE_STATUS::EMPTY: {
            result.status = LoadFromDB(shardIndex, key);
            if (result.status != CACHE_STATUS::AVAILABLE) {
                return result;
            }
            break;
        }
        default: break;
        }
        return result;
    }

    int FindEmptySlot(const InventoryData& inv) {
        for (int i = 0; i < MAX_INVENTORY; ++i) {
            if (inv.items[i].itemID == 0) {
                return i;
            }
        }
        return -1;
    }    
    
    // itemID가 이미 있으면 해당 슬롯(스택), 없으면 첫 빈 슬롯, 둘 다 없으면 -1
    int FindStackOrEmptySlot(const InventoryData& inv, uint32_t itemID) {
        int firstEmpty = -1;
        for (int i = 0; i < MAX_INVENTORY; ++i) {
            if (inv.items[i].itemID == itemID)
                return i;
            if (firstEmpty == -1 && inv.items[i].itemID == 0)
                firstEmpty = i;
        }
        return firstEmpty;
    }

    // status, itemID, slot, quantity
    std::tuple<CACHE_STATUS, uint32_t, uint16_t, uint16_t> CacheStorageInventory::PartialUpdate(uint64_t characterID, uint32_t itemID, uint8_t op, int16_t change) {
        Key key;
        Result result;
        key.characterID = characterID;
        uint16_t shardIndex = key.characterID & SHARD_SIZE_MASK;


        switch (TryGet(key.characterID & SHARD_SIZE_MASK, key, result))
        {
        case CACHE_STATUS::AVAILABLE:
            break;
        case CACHE_STATUS::DB_READING:
            return std::make_tuple(CACHE_STATUS::DB_READING, 0, 0, 0);
        case CACHE_STATUS::EVICTING:
            return std::make_tuple(CACHE_STATUS::EVICTING, 0, 0, 0);
        case CACHE_STATUS::EMPTY: {
            CACHE_STATUS status = LoadFromDB(shardIndex, key);
            if (status != CACHE_STATUS::AVAILABLE)
                return std::make_tuple(status, 0, 0, 0);
            break;
        }
        }
        auto& shard = m_shards[shardIndex];
        std::unique_lock<std::mutex> lock(shard.mutex);

        auto it = shard.cache_data.find(key);
        if (it == shard.cache_data.end())
            return { CACHE_STATUS::EMPTY, 0, 0, 0 };
        auto& res = it->second;
        
        if(res.status != CACHE_STATUS::AVAILABLE)
            return std::make_tuple(res.status, 0, 0, 0);

        CACHE_STATUS resStatus = CACHE_STATUS::AVAILABLE;
        int slot = 0;
        int quantity = 0;

        switch (op)
        {
        case 1: { // ADD
            slot = FindStackOrEmptySlot(res.data, itemID);
            //std::cout << "slot " << slot << "quantity " << change << '\n';

            if (slot == -1) {
                Core::gameLogger->LogInfo("cache5", "inventory full", "char_id", characterID);
                return std::make_tuple(CACHE_STATUS::BLOCKED, 0, 0, 0);
            }
            auto& itemSlot = res.data.items[slot];
            int32_t cur = (itemSlot.itemID == itemID) ? itemSlot.quantity : 0;
            int32_t newQuantity = cur + change;
            if (newQuantity > UINT16_MAX) {
                Core::gameLogger->LogInfo("cache5", "quantity overflow", "char_id", characterID, "item_id", itemID);
                return std::make_tuple(CACHE_STATUS::BLOCKED, 0, 0, 0);
            }

            if (itemSlot.itemID != itemID) {      // 새 슬롯일 때만
                itemSlot.itemID = itemID;
                itemSlot.slot = static_cast<uint8_t>(slot);
                res.data.count++;
            }
            itemSlot.quantity = static_cast<uint16_t>(newQuantity);
            quantity = itemSlot.quantity;
            res.data.count++;
            break;
        }
        case 2: { // UPDATE

            slot = FindStackOrEmptySlot(res.data, itemID);
            if (slot == -1) {
                Core::gameLogger->LogInfo("cache5", "inventory full", "char_id", characterID);
                return std::make_tuple(CACHE_STATUS::BLOCKED, 0, 0, 0);
            }

            auto& itemSlot = res.data.items[slot];
            bool exists = (itemSlot.itemID == itemID);
            int32_t cur = exists ? itemSlot.quantity : 0;
            int32_t newQuantity = cur + change;

            if (newQuantity < 0) {
                Core::gameLogger->LogInfo("cache storage inventory", "not enough item", "char_id", characterID, "item_id", itemID);
                return std::make_tuple(CACHE_STATUS::BLOCKED, 0, 0, 0);
            }
            if (newQuantity > UINT16_MAX) {
                Core::gameLogger->LogInfo("cache5", "quantity overflow", "char_id", characterID, "item_id", itemID);
                return std::make_tuple(CACHE_STATUS::BLOCKED, 0, 0, 0);
            }

            if (newQuantity == 0) {
                if (exists) {                     // 실제로 있던 것만 제거 (count 언더플로우 방지)
                    itemSlot = EMPTY_SLOT;
                    res.data.count--;
                }
                quantity = 0;
            }
            else {
                if (!exists) {                    // 빈 슬롯에 복원
                    itemSlot.itemID = itemID;
                    itemSlot.slot = static_cast<uint8_t>(slot);
                    res.data.count++;
                }
                itemSlot.quantity = static_cast<uint16_t>(newQuantity);
                quantity = itemSlot.quantity;
            }
            break;
        }
        default:
            return std::make_tuple(CACHE_STATUS::BLOCKED, 0, 0, 0);
        }

        if (m_walFn) {
            uint64_t lsn = m_walFn(key.characterID, res.data);
            if (lsn != 0)
                res.data.lastLsn = lsn;  // 실패 시 기존 값 유지 
        }

        res.lastModified = CacheTimer::GetTimeMS();
        shard.dirty_list.insert(key);
        return std::make_tuple(resStatus, itemID, slot, quantity);
    }

    std::unique_ptr<FlushCommand> CacheStorageInventory::GetFlushCommand(Key key, Result result) {
        auto command = std::make_unique<FlushCommand>();
        command->stmtID = 6;
        std::vector<uint8_t> blob(sizeof(InventoryData));
        std::memcpy(blob.data(), &result.data, sizeof(InventoryData));

        command->params.emplace_back(std::move(blob));
        command->params.push_back(key.characterID);
        return command;
    }

    CACHE_STATUS CacheStorageInventory::DeliverItem(uint64_t characterID, uint64_t event_id, uint32_t itemID, uint32_t quantity) {
        if (event_id == 0) // ring의 미사용 슬롯(0)과 충돌 방지
            return CACHE_STATUS::BLOCKED;
        Key key;
        Result result;
        key.characterID = characterID;
        uint16_t shardIndex = key.characterID & SHARD_SIZE_MASK;
        switch (TryGet(key.characterID & SHARD_SIZE_MASK, key, result))
        {
        case CACHE_STATUS::AVAILABLE:
            break;
        case CACHE_STATUS::DB_READING:
            return CACHE_STATUS::DB_READING;
        case CACHE_STATUS::EVICTING:
            return CACHE_STATUS::EVICTING;
        case CACHE_STATUS::EMPTY: {
            CACHE_STATUS status = LoadFromDB(shardIndex, key);
            if (status != CACHE_STATUS::AVAILABLE) {
                return status;
            }
            break;
        }
        default: break;
        }
        auto& shard = m_shards[shardIndex];
        std::unique_lock<std::mutex> lock(shard.mutex);

        auto it = shard.cache_data.find(key);
        if (it == shard.cache_data.end())
            return CACHE_STATUS::EMPTY;
        auto& res = it->second;
        if (res.status != CACHE_STATUS::AVAILABLE)
            return res.status;

        // 중복 방어. (head - tail) 구간만 탐색하기에는 outbox read -> deliver 처리를 원자적으로 수행할 수 없음.
        //(CLAIMED 후 head 전진으로 truncate된 event_id도 재배송될 수 있어 ring 전체를 검사)
        for (uint8_t i = 0; i < RING_SIZE; i++) {
            if (event_id == res.data.recentEventIds[i]) {
                Core::gameLogger->LogInfo("cache5", "event duplicated", "char_id", characterID, "event_id", event_id);
                // 배송은 이미 반영됨. CLAIM은 flush 파이프라인 담당이므로 재flush만 유도
                res.lastModified = CacheTimer::GetTimeMS();
                shard.dirty_list.insert(key);
                return CACHE_STATUS::DUPLICATED;
            }
        }

        if (res.data.head == res.data.tail && res.data.lastOp) {
            // event ring full — CLAIM 되지 않은 event가 밀려나지 않도록 배송 차단 (backpressure)
            Core::gameLogger->LogInfo("cache5", "event ring full", "char_id", characterID);
            return CACHE_STATUS::BLOCKED;
        }


        int slot = FindStackOrEmptySlot(res.data, itemID);
        if (slot == -1) {
            Core::gameLogger->LogInfo("cache5", "inventory full", "char_id", characterID);
            return CACHE_STATUS::BLOCKED;
        }

        auto& itemSlot = res.data.items[slot];
        uint32_t newQuantity = (itemSlot.itemID == itemID ? itemSlot.quantity : 0) + quantity;
        if (newQuantity > UINT16_MAX) {
            Core::gameLogger->LogInfo("cache5", "quantity overflow", "char_id", characterID, "item_id", itemID);
            return CACHE_STATUS::BLOCKED;
        }

        if (itemSlot.itemID != itemID) { // 새 슬롯
            itemSlot.itemID = itemID;
            itemSlot.slot = static_cast<uint8_t>(slot);
            res.data.count++;
        }
        itemSlot.quantity = static_cast<uint16_t>(newQuantity);

        // dedup ring에 event_id 기록 — 아이템과 같은 blob이라 flush 시 원자적으로 영속됨
        res.data.recentEventIds[res.data.tail] = event_id;
        res.data.tail = (res.data.tail + 1) & RING_SIZE_MASK;
        res.data.lastOp = true; // push

        if (m_walFn) {
            uint64_t lsn = m_walFn(key.characterID, res.data);
            if (lsn != 0)
                res.data.lastLsn = lsn;  // 실패 시 기존 값 유지 
        }

        res.lastModified = CacheTimer::GetTimeMS();
        shard.dirty_list.insert(key);
        return CACHE_STATUS::AVAILABLE;
    }

    bool CacheStorageInventory::AdvanceInboxHead(uint16_t shardIndex, Key key, uint8_t snapshotHead, uint8_t claimedCnt) {
        auto& shard = m_shards[shardIndex];
        std::lock_guard<std::mutex> lock(shard.mutex);
        auto it = shard.cache_data.find(key);
        if (it == shard.cache_data.end())
            return false;
        auto& res = it->second;
        if (res.status != CACHE_STATUS::AVAILABLE)  // EVICTING 등 — 건드리지 않음
            return false;
        if (res.data.head != snapshotHead) // 다른 claim pass가 이미 전진 — 스테일 적용(이중 전진) 방지
            return false;
        res.data.head = (snapshotHead + claimedCnt) & RING_SIZE_MASK;
        res.data.lastOp = false;

        if (m_walFn) {
            uint64_t lsn = m_walFn(key.characterID, res.data);
            if (lsn != 0)
                res.data.lastLsn = lsn;  // 실패 시 기존 값 유지 
        }

        res.lastModified = CacheTimer::GetTimeMS();
        shard.dirty_list.insert(key);
        return true;
    }
}
