#pragma once
#include "CacheStorage.h"
#include <CoreLib/MessageTypes.h>
#include <CoreLib/Message.h>
#include <tuple>

#include <mysqlconn/include/mysql/jdbc.h>
namespace Cache {
    struct InventoryStruct {
        uint16_t count;
        Core::MsgInventoryItem items[MAX_INVENTORY];
    };

    constexpr Core::MsgInventoryItem EMPTY_SLOT = { 0, 0, 0 };
    constexpr InventoryStruct EMPTY_INVENTORY = {
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

    using Result5 = CacheItem<InventoryStruct>;

    class CacheStorage5 : public CacheStorage<Key5, Result5, KeyHash5> {
        using Key = Key5;
        using KeyHash = KeyHash5;
        using Result = Result5;
    public:
        bool Getter(Core::Message* msg);
        bool Setter(std::unique_ptr<sql::ResultSet> r);
        std::tuple<uint8_t, uint32_t, uint16_t, uint16_t> PartialUpdate(Core::Message* msg);
    };
}
