#include "CacheStorage5.h"

#include <mysqlconn/include/mysql/jdbc.h>
#include <CoreLib/Message.h>

namespace Cache {
    bool CacheStorage5::Getter(Core::Message* msg) {
        auto body = Core::parseMsgBody<Core::MsgInventoryReqBody>(msg->GetBuffer());
        Key key;
        Result res;
        key.characterID = body->characterID;
        if (!TryGet(key.characterID & SHARD_SIZE_MASK, key, res))
            return false;

        auto st = reinterpret_cast<Core::MsgStruct<Core::MsgInventoryResBody>*>(msg->GetBuffer());
        st->header.messageType = Core::MSG_INVENTORY_RES;
        st->body.resStatus = 1;
        st->body.itemCount = res.data.count;
        for (int i = 0; i < res.data.count; i++)
        {
            st->body.items[i].itemID = res.data.items[i].itemID;
            st->body.items[i].quantity = res.data.items[i].quantity;
            st->body.items[i].slot = res.data.items[i].slot;
        }
        return true;
    }

    bool CacheStorage5::Setter(std::unique_ptr<sql::ResultSet> r) {
        if (!r || !r->next()) {
            return false;
        }
        Key key;
        Result res;
        std::istream* blobStream = r->getBlob("inventory");
        if (!blobStream) {
            return false;
        }
        blobStream->read(reinterpret_cast<char*>(&res.data), sizeof(res.data));
        key.characterID = r->getUInt64("char_id");
        Insert(key.characterID & SHARD_SIZE_MASK, key, res);

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
    std::tuple<uint8_t, uint32_t, uint16_t, uint16_t> CacheStorage5::PartialUpdate(Core::Message* msg) {
        Core::MsgStruct<Core::MsgInventoryUpdateBody>* st = reinterpret_cast<Core::MsgStruct<Core::MsgInventoryUpdateBody>*>(msg->GetBuffer());

        Key key;
        key.characterID = st->body.characterID;
        uint16_t shardID = key.characterID & SHARD_SIZE_MASK;
        auto& shard = m_shards[shardID];
        std::unique_lock<std::shared_mutex> lock1(shard.dataMutex);
        auto iter = shard.cache_data.find(key);
        if (iter == shard.cache_data.end())
            return std::make_tuple(0, 0, 0, 0);
        auto& res = iter->second;
        
        uint8_t resStatus = 1;
        uint32_t itemID = st->body.itemID;
        int slot = 0;
        int quantity = 0;

        switch (st->body.op)
        {
        case 1: { // ADD
            slot = FindEmptySlot(res.data);
            if (slot == -1)
                return std::make_tuple(0, 0, 0, 0);

            auto& itemSlot = res.data.items[slot];
            itemSlot.itemID = st->body.itemID;
            itemSlot.quantity = st->body.change;
            itemSlot.slot = static_cast<uint8_t>(slot);
            quantity = itemSlot.quantity;
            res.data.count++;
            break;
        }
        case 2: { // DELETE
            auto it = std::find_if(std::begin(res.data.items), std::end(res.data.items),
                [&](const Core::MsgInventoryItem& item) {
                    return item.itemID == st->body.itemID;
                });

            if (it == std::end(res.data.items))
                return std::make_tuple(0, 0, 0, 0);
            *it = EMPTY_SLOT;
            res.data.count--;
            resStatus = 2;

            break;
        }
        case 3: { // UPDATE
            auto it = std::find_if(std::begin(res.data.items), std::end(res.data.items),
                [&](const Core::MsgInventoryItem& item) {
                    return item.itemID == st->body.itemID;
                });

            if (it == std::end(res.data.items))
                return std::make_tuple(0, 0, 0, 0);
            it->quantity += st->body.change;
            quantity = it->quantity;
            if (it->quantity <= 0) {
                *it = EMPTY_SLOT;
                res.data.count--;
            }
            quantity = it->quantity;
            break;
        }
        default:
            return std::make_tuple(0, 0, 0, 0);
        }

        res.lastModified = std::chrono::steady_clock::now();

        std::unique_lock<std::mutex> lock2(shard.dirtyMutex);
        shard.dirty_list.insert(key);
        return std::make_tuple(resStatus, itemID, slot, quantity);
    }
}
