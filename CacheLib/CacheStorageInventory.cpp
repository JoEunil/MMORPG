#include "pch.h"
#include "CacheStorageInventory.h"

#include <algorithm>
#include <iterator>
#include <CoreLib/Interface/IMessage.h>
#include "DB/DBWorker.h"
#include "CacheFlush.h"
#include "InventoryCodec.h"
#include "CoreConfig.h"

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
        item.status = CACHE_STATUS::AVAILABLE;
        MarkDirtyLocked(shard, key, item);

        return TryEvictAfterInsertLocked(shard, key);
    }
    CACHE_STATUS CacheStorageInventory::LoadFromDB(uint16_t shardIndex, const Key& key) {
        auto status = TrySetReading(shardIndex, key);
        if (status != CACHE_STATUS::EMPTY) // READING
            return status;

        const auto enqueueResult = dbWorker->Enqueue([=](DBConnectionGame* conn) {
            Result result;
            auto res = conn->ExecuteSelect(StatementId::SelectInventory, key.characterID);
            if (!res) {
                CompleteLoad(shardIndex, key, nullptr);
                return;
            }
            if (!res->next()) {
                // A successful query with no row is a confirmed empty bag, not a read failure.
                result.data = EMPTY_INVENTORY;
            } else {
                std::unique_ptr<std::istream> blobStream(res->getBlob("inventory"));
                if (!blobStream) throw std::runtime_error("Inventory blob is null.");
                result.data = ReadInventory(*blobStream);
            }
            result.rollbackCnt = 0;
            CompleteLoad(shardIndex, key, &result);
        }, [this, shardIndex, key] { CompleteLoad(shardIndex, key, nullptr); });
        if (enqueueResult != DBEnqueueResult::Queued) {
            CompleteLoad(shardIndex, key, nullptr);
            return CACHE_STATUS::EMPTY;
        }
        return CACHE_STATUS::DB_READING;
    }

    CACHE_STATUS CacheStorageInventory::GetOrLoad(uint16_t shardIndex, const Key& key, Result& outResult) {
        const auto status = TryGet(shardIndex, key, outResult);
        if (status != CACHE_STATUS::EMPTY)
            return status;

        return LoadFromDB(shardIndex, key);
    }

    InventoryCacheItem CacheStorageInventory::Getter(uint64_t characterID) {
        Result result;
        const Key key{characterID};
        const uint16_t shardIndex = characterID & SHARD_SIZE_MASK;
        auto status = GetOrLoad(shardIndex, key, result);
        // A concurrent load can return AVAILABLE before we have copied its snapshot.
        if (status == CACHE_STATUS::AVAILABLE && result.status == CACHE_STATUS::EMPTY)
            status = TryGet(shardIndex, key, result);
        result.status = status;
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

    static bool IsEquipped(const InventoryData& inventory, uint32_t itemID) {
        return inventory.equippedWeaponItemID == itemID ||
            inventory.equippedShieldItemID == itemID ||
            inventory.equippedBodyItemID == itemID;
    }

    bool CacheStorageInventory::AppendInventoryWal(uint64_t characterID, InventoryData& image) {
        if (!m_walFn)
            return false;

        const auto lsn = m_walFn(characterID, image);
        if (lsn == 0)
            return false;

        image.lastLsn = lsn;
        return true;
    }

    bool CacheStorageInventory::TryCommitInventoryLocked(CacheShard<Key, Result, KeyHash>& shard, const Key& key, Result& entry, InventoryData& image) {
        try {
            if (!AppendInventoryWal(key.characterID, image))
                return false;
        } catch (...) {
            return false;
        }

        entry.data = image;
        MarkDirtyLocked(shard, key, entry);
        return true;
    }

    Core::ItemOperationResult CacheStorageInventory::ChangeItemLocked(uint16_t shardIndex, Key key, uint32_t itemID, int32_t quantity) {
        auto& shard = m_shards[shardIndex];
        auto found = shard.cache_data.find(key);
        if (found == shard.cache_data.end() || found->second.status != CACHE_STATUS::AVAILABLE)
            return {Core::ItemOperationStatus::Busy};
        auto& result = found->second;
        if (quantity < 0 && IsEquipped(result.data, itemID))
            return {Core::ItemOperationStatus::Missing};
        auto image = result.data;
        const int slot = FindStackOrEmptySlot(image, itemID);
        if (slot < 0) return {quantity < 0 ? Core::ItemOperationStatus::Missing : Core::ItemOperationStatus::Full};
        auto& item = image.items[slot];
        const int current = item.itemID == itemID ? item.quantity : 0;
        const int next = current + quantity;
        if (next < 0) return {Core::ItemOperationStatus::Missing, static_cast<uint16_t>(current)};
        if (next > UINT16_MAX) return {Core::ItemOperationStatus::Full, static_cast<uint16_t>(current)};
        if (next == 0) {
            if (item.itemID != 0) { item = EMPTY_SLOT; --image.count; }
        } else {
            if (item.itemID == 0) { item.itemID = itemID; item.slot = static_cast<uint8_t>(slot); ++image.count; }
            item.quantity = static_cast<uint16_t>(next);
        }
        if (!TryCommitInventoryLocked(shard, key, result, image))
            return {Core::ItemOperationStatus::StorageFailure, static_cast<uint16_t>(current)};
        return {Core::ItemOperationStatus::Success, static_cast<uint16_t>(next)};
    }

    Core::EquipmentStateResult CacheStorageInventory::GetEquipmentState(uint64_t characterID) {
        if (characterID == 0) return {Core::EquipmentOperationStatus::Unsupported};
        const auto result = Getter(characterID);
        if (result.status == CACHE_STATUS::AVAILABLE)
            return {Core::EquipmentOperationStatus::Success, result.data.equippedWeaponItemID, result.data.equippedShieldItemID, result.data.equippedBodyItemID};
        return {result.status == CACHE_STATUS::EMPTY ? Core::EquipmentOperationStatus::StorageFailure : Core::EquipmentOperationStatus::Busy};
    }

    void CacheStorageInventory::LoadEquipmentState(uint64_t characterID, std::function<void(Core::EquipmentStateResult)> completed) {
        if (!completed) return;
        if (characterID == 0) { completed({Core::EquipmentOperationStatus::Unsupported}); return; }
        const uint16_t index = characterID & SHARD_SIZE_MASK;
        auto& shard = m_shards[index];
        bool load = false;
        Core::EquipmentStateResult result{Core::EquipmentOperationStatus::Busy};
        {
            std::lock_guard lock(shard.mutex);
            const auto found = shard.cache_data.find(Key{characterID});
            if (found != shard.cache_data.end() && found->second.status == CACHE_STATUS::AVAILABLE)
                result = {Core::EquipmentOperationStatus::Success, found->second.data.equippedWeaponItemID, found->second.data.equippedShieldItemID, found->second.data.equippedBodyItemID};
            else if (found == shard.cache_data.end() || found->second.status == CACHE_STATUS::DB_READING) {
                pendingEquipmentLoads[index][characterID].push_back(std::move(completed));
                load = found == shard.cache_data.end();
                if (!load) return;
            }
        }
        if (!load) { completed(result); return; }
        Key key{characterID};
        const auto status = LoadFromDB(index, key);
        if (status == CACHE_STATUS::DB_READING) return;
        // A concurrent loader normally drained the waiters already. Handle a directly available
        // insertion or synchronous enqueue rejection without retaining a caller indefinitely.
        std::vector<std::function<void(Core::EquipmentStateResult)>> callbacks;
        result = {Core::EquipmentOperationStatus::StorageFailure};
        {
            std::lock_guard lock(shard.mutex);
            const auto found = shard.cache_data.find(key);
            if (found != shard.cache_data.end() && found->second.status == CACHE_STATUS::AVAILABLE)
                result = {Core::EquipmentOperationStatus::Success, found->second.data.equippedWeaponItemID, found->second.data.equippedShieldItemID, found->second.data.equippedBodyItemID};
            if (auto waiting = pendingEquipmentLoads[index].extract(characterID); waiting) {
                callbacks = std::move(waiting.mapped());
            }
        }
        for (auto& callback : callbacks) callback(result);
    }

    Core::EquipmentStateResult CacheStorageInventory::EquipItem(uint64_t characterID, uint8_t slot, uint32_t itemID) {
        if (!CanEquipItem(slot, itemID))
            return {Core::EquipmentOperationStatus::Unsupported};
        const auto ready = GetEquipmentState(characterID);
        if (ready.status != Core::EquipmentOperationStatus::Success) return ready;
        const uint16_t index = characterID & SHARD_SIZE_MASK;
        auto& shard = m_shards[index];
        std::lock_guard lock(shard.mutex);
        const auto found = shard.cache_data.find(Key{characterID});
        if (found == shard.cache_data.end() || found->second.status != CACHE_STATUS::AVAILABLE)
            return {Core::EquipmentOperationStatus::Busy};
        auto& result = found->second;
        if (itemID != 0) {
            bool owned = false;
            for (const auto& item : result.data.items)
                if (item.itemID == itemID && item.quantity > 0) { owned = true; break; }
            if (!owned) return {Core::EquipmentOperationStatus::Missing};
        }
        auto image = result.data;
        uint32_t& equipped = slot == 0 ? image.equippedWeaponItemID : slot == 1 ? image.equippedShieldItemID : image.equippedBodyItemID;
        if (equipped == itemID)
            return {Core::EquipmentOperationStatus::Success, image.equippedWeaponItemID, image.equippedShieldItemID, image.equippedBodyItemID};
        equipped = itemID;
        if (!TryCommitInventoryLocked(shard, found->first, result, image))
            return {Core::EquipmentOperationStatus::StorageFailure};
        return {Core::EquipmentOperationStatus::Success, image.equippedWeaponItemID, image.equippedShieldItemID, image.equippedBodyItemID};
    }

    Core::ItemOperationResult CacheStorageInventory::ConsumeItem(uint64_t characterID, uint32_t itemID) {
        const uint16_t index = characterID & SHARD_SIZE_MASK;
        std::lock_guard lock(m_shards[index].mutex);
        return ChangeItemLocked(index, Key{characterID}, itemID, -1);
    }

    void CacheStorageInventory::GrantItem(uint64_t characterID, uint32_t itemID, uint16_t quantity,
        std::function<void(Core::ItemOperationResult)> completed) {
        const uint16_t index = characterID & SHARD_SIZE_MASK;
        auto& shard = m_shards[index];
        bool load = false;
        Core::ItemOperationResult result{Core::ItemOperationStatus::StorageFailure};
        {
            std::lock_guard lock(shard.mutex);
            if (characterID == 0 || itemID == 0 || quantity == 0) { }
            else {
                auto found = shard.cache_data.find(Key{characterID});
                if (found == shard.cache_data.end() || found->second.status == CACHE_STATUS::DB_READING) {
                    pendingRewards[index][characterID].push_back({itemID, quantity, std::move(completed)});
                    load = found == shard.cache_data.end();
                    if (!load) return;
                } else {
                    // Retain the accepted flush generation until its snapshot completes.
                    RestoreEvictingEntryLocked(shard, found->first, found->second);
                    result = ChangeItemLocked(index, Key{characterID}, itemID, quantity);
                }
            }
        }
        if (load) {
            Key key{characterID};
            LoadFromDB(index, key);
            return;
        }
        if (completed) completed(result);
    }

    void CacheStorageInventory::CompleteLoad(uint16_t index, Key key, const Result* loaded) {
        std::vector<PendingReward> rewards;
        std::vector<Core::ItemOperationResult> results;
        std::vector<std::function<void(Core::EquipmentStateResult)>> equipmentLoads;
        Core::EquipmentStateResult equipmentState{Core::EquipmentOperationStatus::StorageFailure};
        auto& shard = m_shards[index];
        {
            std::lock_guard lock(shard.mutex);
            ApplyLoadResultLocked(shard, key, loaded);
            const auto current = shard.cache_data.find(key);
            if (loaded && current != shard.cache_data.end() && current->second.status == CACHE_STATUS::AVAILABLE)
                equipmentState = {Core::EquipmentOperationStatus::Success, current->second.data.equippedWeaponItemID, current->second.data.equippedShieldItemID, current->second.data.equippedBodyItemID};
            if (auto equipment = pendingEquipmentLoads[index].extract(key.characterID); equipment) {
                equipmentLoads = std::move(equipment.mapped());
            }
            if (auto pending = pendingRewards[index].extract(key.characterID); pending) {
                rewards = std::move(pending.mapped());
                results.reserve(rewards.size());
                for (const auto& reward : rewards)
                    results.push_back(loaded ? ChangeItemLocked(index, key, reward.itemID, reward.quantity)
                        : Core::ItemOperationResult{Core::ItemOperationStatus::StorageFailure});
            }
        }
        for (size_t i = 0; i < rewards.size(); ++i)
            if (rewards[i].completed) rewards[i].completed(results[i]);
        for (auto& completed : equipmentLoads) completed(equipmentState);
    }

    static CACHE_STATUS ValidatePartialUpdate(const InventoryCacheItem& entry, uint32_t itemID, uint8_t op, int32_t change) {
        if (entry.status != CACHE_STATUS::AVAILABLE)
            return entry.status;

        if (change < 0 && IsEquipped(entry.data, itemID))
            return CACHE_STATUS::BLOCKED;
        if (op != 1 && op != 2)
            return CACHE_STATUS::BLOCKED;

        return CACHE_STATUS::AVAILABLE;
    }

    // status, itemID, slot, quantity
    std::tuple<CACHE_STATUS, uint32_t, uint16_t, uint16_t> CacheStorageInventory::PartialUpdate(uint64_t characterID, uint32_t itemID, uint8_t op, int32_t change) {
        if (itemID == 0 || change == 0 || change < -static_cast<int32_t>(UINT16_MAX) || change > UINT16_MAX || (op == 1 && change < 0))
            return { CACHE_STATUS::BLOCKED, 0, 0, 0 };
        const Key key{characterID};
        const uint16_t shardIndex = characterID & SHARD_SIZE_MASK;
        Result result;
        const auto status = GetOrLoad(shardIndex, key, result);
        if (status != CACHE_STATUS::AVAILABLE)
            return {status, 0, 0, 0};

        auto& shard = m_shards[shardIndex];
        std::unique_lock<std::mutex> lock(shard.mutex);

        auto it = shard.cache_data.find(key);
        if (it == shard.cache_data.end())
            return { CACHE_STATUS::EMPTY, 0, 0, 0 };
        auto& res = it->second;
        if (const auto validation = ValidatePartialUpdate(res, itemID, op, change); validation != CACHE_STATUS::AVAILABLE)
            return {validation, 0, 0, 0};

        const int slot = FindStackOrEmptySlot(res.data, itemID);
        if (slot == -1) {
            Core::gameLogger->LogInfo("cache5", "inventory full", {{"char_id", characterID}});
            return {CACHE_STATUS::BLOCKED, 0, 0, 0};
        }

        auto& itemSlot = res.data.items[slot];
        const bool exists = itemSlot.itemID == itemID;
        const int32_t currentQuantity = exists ? itemSlot.quantity : 0;
        const int32_t newQuantity = currentQuantity + change;
        if (newQuantity < 0) {
            Core::gameLogger->LogInfo("cache storage inventory", "not enough item", {{"char_id", characterID}, {"item_id", itemID}});
            return {CACHE_STATUS::BLOCKED, 0, 0, 0};
        }
        if (newQuantity > UINT16_MAX) {
            Core::gameLogger->LogInfo("cache5", "quantity overflow", {{"char_id", characterID}, {"item_id", itemID}});
            return {CACHE_STATUS::BLOCKED, 0, 0, 0};
        }

        if (newQuantity == 0) {
            if (exists) {
                itemSlot = EMPTY_SLOT;
                res.data.count--;
            }
        } else {
            if (!exists) {
                itemSlot.itemID = itemID;
                itemSlot.slot = static_cast<uint8_t>(slot);
                res.data.count++;
            }
            itemSlot.quantity = static_cast<uint16_t>(newQuantity);
        }

        AppendInventoryWal(key.characterID, res.data);
        MarkDirtyLocked(shard, key, res);
        return {CACHE_STATUS::AVAILABLE, itemID, static_cast<uint16_t>(slot), static_cast<uint16_t>(newQuantity)};
    }

    std::unique_ptr<FlushCommand> CacheStorageInventory::GetFlushCommand(Key key, Result result) {
        auto command = std::make_unique<FlushCommand>();
        command->stmtID = StatementId::UpdateInventory;
        command->generation = result.flushGeneration;
        auto blob = EncodeInventory(result.data);

        command->params.emplace_back(std::move(blob));
        command->params.emplace_back(key.characterID);
        return command;
    }

    CACHE_STATUS CacheStorageInventory::DeliverItem(uint64_t characterID, uint64_t event_id, uint32_t itemID, uint32_t quantity) {
        if (event_id == 0 || itemID == 0 || quantity == 0 || quantity > UINT16_MAX)
            return CACHE_STATUS::BLOCKED;
        const Key key{characterID};
        const uint16_t shardIndex = characterID & SHARD_SIZE_MASK;
        Result result;
        const auto status = GetOrLoad(shardIndex, key, result);
        if (status != CACHE_STATUS::AVAILABLE)
            return status;

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
        if (std::ranges::find(res.data.recentEventIds, event_id) != std::end(res.data.recentEventIds)) {
            Core::gameLogger->LogInfo("cache5", "event duplicated", {{"char_id", characterID}, {"event_id", event_id}});
            // 배송은 이미 반영됨. CLAIM은 flush 파이프라인 담당이므로 재flush만 유도
            MarkDirtyLocked(shard, key, res);
            return CACHE_STATUS::DUPLICATED;
        }

        if (res.data.head == res.data.tail && res.data.lastOp) {
            // event ring full — CLAIM 되지 않은 event가 밀려나지 않도록 배송 차단 (backpressure)
            Core::gameLogger->LogInfo("cache5", "event ring full", {{"char_id", characterID}});
            return CACHE_STATUS::BLOCKED;
        }


        int slot = FindStackOrEmptySlot(res.data, itemID);
        if (slot == -1) {
            Core::gameLogger->LogInfo("cache5", "inventory full", {{"char_id", characterID}});
            return CACHE_STATUS::BLOCKED;
        }

        auto& itemSlot = res.data.items[slot];
        uint32_t newQuantity = (itemSlot.itemID == itemID ? itemSlot.quantity : 0) + quantity;
        if (newQuantity > UINT16_MAX) {
            Core::gameLogger->LogInfo("cache5", "quantity overflow", {{"char_id", characterID}, {"item_id", itemID}});
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

        AppendInventoryWal(key.characterID, res.data);
        MarkDirtyLocked(shard, key, res);
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

        AppendInventoryWal(key.characterID, res.data);
        MarkDirtyLocked(shard, key, res);
        return true;
    }
}
