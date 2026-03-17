#include "CacheStorage5.h"

#include <mysqlconn/include/mysql/jdbc.h>
#include <CoreLib/Message.h>

#include "CacheFlush.h"

namespace Cache {
    CACHE_STATUS CacheStorage5::LoadFromDB(uint16_t shardIndex, Key& key, Result& result) {
        // status 변경 -> 원자성 보장
        auto status = TrySetReading(shardIndex, key);
        if (status != CACHE_STATUS::EMPTY) // READING
            return status;

        auto conn = connectionPool->Acquire();
        auto res = conn->ExecuteSelect(5, key.characterID);
        connectionPool->Return(conn);

        if (!res || !res->next()) {
            Core::gameLogger->LogInfo("cache storage 5", "inventory not found in DB", "char_id", key.characterID);
            SetEmpty(shardIndex, key);  // READING → EMPTY
            return CACHE_STATUS::EMPTY;
        }

        std::istream* blobStream = res->getBlob("inventory");
        if (blobStream) {
            blobStream->read(
                reinterpret_cast<char*>(&result.data),
                sizeof(InventoryStruct)
            );
        } else {
            result.data = EMPTY_INVENTORY;
        }
        result.rollbackCnt = 0;
        Insert(shardIndex, key, result);  // READING → AVAILABLE
        return CACHE_STATUS::AVAILABLE;
    }

    bool CacheStorage5::Getter(Core::Message* msg) {
        auto body = Core::parseMsgBody<Core::MsgInventoryReqBody>(msg->GetBuffer());
        Key key;
        Result result;
        key.characterID = body->characterID;
        auto shardIndex = body->characterID & SHARD_SIZE_MASK;

        switch (TryGet(key.characterID & SHARD_SIZE_MASK, key, result))
        {
        case CACHE_STATUS::AVAILABLE:
            break;
        case CACHE_STATUS::DB_READING:
            return false;
        case CACHE_STATUS::EVICTING:
            return false;
        case CACHE_STATUS::EMPTY: {
            if (LoadFromDB(shardIndex, key, result) != CACHE_STATUS::AVAILABLE)
                return false;
            break;
        }
        default: break;
        }
        auto st = reinterpret_cast<Core::MsgStruct<Core::MsgInventoryResBody>*>(msg->GetBuffer());
        st->header.messageType = Core::MSG_INVENTORY_RES;
        st->body.resStatus = 1;
        st->body.itemCount = result.data.count;
        for (int i = 0; i < result.data.count; i++)
        {
            st->body.items[i].itemID = result.data.items[i].itemID;
            st->body.items[i].quantity = result.data.items[i].quantity;
            st->body.items[i].slot = result.data.items[i].slot;
        }
        return true;
    }

    int FindEmptySlot(const InventoryStruct& inv) {
        for (int i = 0; i < MAX_INVENTORY; ++i) {
            if (inv.items[i].itemID == 0) {
                return i;
            }
        }
        return -1;
    }

    // status, itemID, slot, quantity
    std::tuple<CACHE_STATUS, uint32_t, uint16_t, uint16_t> CacheStorage5::PartialUpdate(Core::Message* msg) {
        Core::MsgStruct<Core::MsgInventoryUpdateBody>* st = reinterpret_cast<Core::MsgStruct<Core::MsgInventoryUpdateBody>*>(msg->GetBuffer());

        Key key;
        Result result;
        key.characterID = st->body.characterID;
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
            if (LoadFromDB(shardIndex, key, result) != CACHE_STATUS::AVAILABLE)
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
        uint32_t itemID = st->body.itemID;
        int slot = 0;
        int quantity = 0;

        switch (st->body.op)
        {
        case 1: { // ADD
            slot = FindEmptySlot(res.data);
            //std::cout << "slot " << slot << "quantity " << st->body.change << '\n';

            if (slot == -1) {
                Core::gameLogger->LogInfo("cache5", "inventory full", "char_id", st->body.characterID);
                return std::make_tuple(CACHE_STATUS::BLOCKED, 0, 0, 0);
            }

            auto& itemSlot = res.data.items[slot];
            itemSlot.itemID = st->body.itemID;
            itemSlot.quantity = st->body.change;
            itemSlot.slot = static_cast<uint8_t>(slot);
            quantity = itemSlot.quantity;
            res.data.count++;
            break;
        }
        case 2: { // UPDATE
            auto it = std::find_if(std::begin(res.data.items), std::end(res.data.items),
                [&](const Core::MsgInventoryItem& item) {
                    return item.itemID == st->body.itemID;
                });

            if (it == std::end(res.data.items)) {
                Core::gameLogger->LogInfo("cache storage 5", "item not found", "char_id", st->body.characterID, "item_id", st->body.itemID);
                return std::make_tuple(CACHE_STATUS::BLOCKED, 0, 0, 0);
            }
            slot = it->slot;
            if ((int16_t)it->quantity + st->body.change <= 0) {
                *it = EMPTY_SLOT;
                res.data.count--;
                quantity = 0;
            } else {
                it->quantity += st->body.change;
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

    std::unique_ptr<FlushCommand> CacheStorage5::GetFlushCommand(Key key, Result result) {
        auto command = std::make_unique<FlushCommand>();
        command->stmtID = 6;
        std::vector<uint8_t> blob(sizeof(InventoryStruct));
        std::memcpy(blob.data(), &result.data, sizeof(InventoryStruct));

        command->params.emplace_back(std::move(blob));
        command->params.push_back(key.characterID);
        return command;
    }
}
