#pragma once
#include <tuple>
#include <memory>
#include <functional>

#include <CoreLib/MessageTypes.h>
#include <CoreLib/Message.h>
#include <CoreLib/LoggerGlobal.h>
#include <mysqlconn/include/mysql/jdbc.h>

#include "CacheStorage.h"


namespace Cache {
    struct FlushCommand;
    constexpr size_t RING_SIZE = 32;
    constexpr size_t RING_SIZE_MASK = 31;
    struct InventoryData {
        uint16_t count;
        Core::MsgInventoryItem items[MAX_INVENTORY];
        // blob 내부 필드라서 다른 자료구조 적용이 어려움.
        uint64_t recentEventIds[RING_SIZE];   // dedup ring (자동 truncate)
        uint8_t  head; // outbox CLAIMED 시 갱신 
        uint8_t  tail;
        bool lastOp; // 1: push, 0: pop
        uint64_t lastLsn; 
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
        std::function<uint64_t(uint64_t, const InventoryData&)>  m_walFn;

        CACHE_STATUS LoadFromDB(uint16_t shardIndex, Key& key);
        bool RestoreEntry(uint64_t characterID, InventoryData& rec);

        void SetWalFn(std::function<uint64_t(uint64_t, const InventoryData&)> f) {
            m_walFn = std::move(f);
        }
        friend class WALManager;
    public:
        Result5 Getter(uint64_t characterID);
        std::tuple<CACHE_STATUS, uint32_t, uint16_t, uint16_t> PartialUpdate(uint64_t characterID, uint32_t itemID, uint8_t op, int16_t change);
        static std::unique_ptr<FlushCommand> GetFlushCommand(Key key, Result result);
        // outbox → inbox 배송. AVAILABLE: 지급됨 / DUPLICATED: 이미 지급(스킵) / BLOCKED: 인벤·링 가득
        CACHE_STATUS DeliverItem(uint64_t characterID, uint64_t event_id, uint32_t itemID, uint32_t quantity);
        bool AdvanceInboxHead(uint16_t shardIndex, Key key, uint8_t snapshotHead, uint8_t claimedCnt);
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
