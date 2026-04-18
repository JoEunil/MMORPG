#include "CacheStorageInventory.h"

#include <mysqlconn/include/mysql/jdbc.h>
#include <CoreLib/Message.h>
#include "DBWorker.h"
#include "CacheFlush.h"

namespace Cache {
    CACHE_STATUS CacheStorageInventory::LoadFromDB(uint16_t shardIndex, Key& key) {
        // status 변경 -> 원자성 보장
        auto status = TrySetReading(shardIndex, key);
        if (status != CACHE_STATUS::EMPTY) // READING
            return status;

        dbWorker->Enqueue([=](DBConnectionGame* conn) {
            Result result;
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
            else {
                result.data = EMPTY_INVENTORY;
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
            if (LoadFromDB(shardIndex, key) != CACHE_STATUS::AVAILABLE) {
                result.status = CACHE_STATUS::EMPTY;
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
            if (LoadFromDB(shardIndex, key) != CACHE_STATUS::AVAILABLE)
                return std::make_tuple(CACHE_STATUS::EMPTY, 0, 0, 0);
            break;
        }
        }
        auto& shard = m_shards[shardIndex];
        std::unique_lock<std::mutex> lock(shard.mutex);
        auto& res = shard.cache_data[key];
        
        if(res.status != CACHE_STATUS::AVAILABLE)
            return std::make_tuple(res.status, 0, 0, 0);

        CACHE_STATUS resStatus = CACHE_STATUS::AVAILABLE;
        int slot = 0;
        int quantity = 0;

        switch (op)
        {
        case 1: { // ADD
            slot = FindEmptySlot(res.data);
            //std::cout << "slot " << slot << "quantity " << change << '\n';

            if (slot == -1) {
                Core::gameLogger->LogInfo("cache5", "inventory full", "char_id", characterID);
                return std::make_tuple(CACHE_STATUS::BLOCKED, 0, 0, 0);
            }

            auto& itemSlot = res.data.items[slot];
            itemSlot.itemID =  itemID;
            itemSlot.quantity = change;
            itemSlot.slot = static_cast<uint8_t>(slot);
            quantity = itemSlot.quantity;
            res.data.count++;
            break;
        }
        case 2: { // UPDATE
            auto it = std::find_if(std::begin(res.data.items), std::end(res.data.items),
                [&](const Core::MsgInventoryItem& item) {
                    return item.itemID == itemID;
                });

            if (it == std::end(res.data.items)) {
                Core::gameLogger->LogInfo("cache storage inventory", "item not found", "char_id", characterID, "item_id", itemID);
                return std::make_tuple(CACHE_STATUS::BLOCKED, 0, 0, 0);
            }
            if (it->quantity + change < 0) {
                Core::gameLogger->LogInfo("cache storage inventory", "not enough item", "char_id", characterID, "item_id", itemID);
                return std::make_tuple(CACHE_STATUS::BLOCKED, 0, 0, 0);
            }
            slot = it->slot;
            if ((int16_t)it->quantity + change <= 0) {
                *it = EMPTY_SLOT;
                res.data.count--;
                quantity = 0;
            } else {
                it->quantity += change;
                quantity = it->quantity;
            }
            break;
        }
        default:
            return std::make_tuple(CACHE_STATUS::BLOCKED, 0, 0, 0);
        }

        res.lastModified = CacheTimer::GetTimeMS();
        shard.dirty_list.insert(key);
        return std::make_tuple(resStatus, itemID, slot, quantity);
    }
    uint16_t CacheStorageInventory::GetItemCount(uint64_t characterID, uint32_t itemID) {
        Key key;
        Result result;
        key.characterID = characterID;
        uint16_t shardIndex = key.characterID & SHARD_SIZE_MASK;

        if (TryGet(shardIndex, key, result) != CACHE_STATUS::AVAILABLE)
            return 0;

        auto& shard = m_shards[shardIndex];
        std::unique_lock<std::mutex> lock(shard.mutex);
        auto& res = shard.cache_data[key];

        if (res.status != CACHE_STATUS::AVAILABLE)
            return 0;

        auto it = std::find_if(std::begin(res.data.items), std::end(res.data.items),
            [&](const Core::MsgInventoryItem& item) {
                return item.itemID == itemID;
            });

        if (it == std::end(res.data.items))
            return 0;

        return it->quantity;
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
}
