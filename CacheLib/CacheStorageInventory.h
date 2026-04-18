#pragma once
#include <tuple>
#include <memory>

#include <CoreLib/MessageTypes.h>
#include <CoreLib/Message.h>
#include <CoreLib/LoggerGlobal.h>
#include <mysqlconn/include/mysql/jdbc.h>

#include "CacheStorage.h"


namespace Cache {
    struct FlushCommand;
    struct InventoryData {
        uint16_t count;
        Core::MsgInventoryItem items[MAX_INVENTORY];
    };

    constexpr Core::MsgInventoryItem EMPTY_SLOT = { 0, 0, 0 };
    constexpr InventoryData EMPTY_INVENTORY = {
        0,
        {} // 모든 원소 0으로 초기화
    };

    struct Key5 {
        uint64_t characterID;
        bool operator==(const Key5& other) const noexcept {
            return characterID == other.characterID;
        }
        bool operator<(const Key5& other) const noexcept {
            return characterID < other.characterID;
        }
    };
    struct KeyHash5 {
        std::size_t operator()(const Key5& key) const noexcept {
            return std::hash<uint64_t>()(key.characterID);
        }
    };

    using Result5 = CacheItem<InventoryData>;

    class CacheStorageInventory : public CacheStorage<Key5, Result5, KeyHash5> {
        using Key = Key5;
        using KeyHash = KeyHash5;
        using Result = Result5;
        CACHE_STATUS LoadFromDB(uint16_t shardIndex, Key& key);
    public:
        Result5 Getter(uint64_t characterID);

        uint16_t GetItemCount(uint64_t characterID, uint32_t itemID);
        std::tuple<CACHE_STATUS, uint32_t, uint16_t, uint16_t> PartialUpdate(uint64_t characterID, uint32_t itemID, uint8_t op, int16_t change);
        static std::unique_ptr<FlushCommand> GetFlushCommand(Key key, Result result);
        std::string ResultToString(const Key5& key, const Result5& result) override {
            std::ostringstream oss;
            oss << "char_id: " << key.characterID << ", inventory_hex: ";
            const uint8_t* raw = reinterpret_cast<const uint8_t*>(&result.data);
            for (size_t i = 0; i < sizeof(InventoryData); i++)
                oss << std::hex << std::setw(2) << std::setfill('0') << (int)raw[i];
            return oss.str();
        }
    };
}
