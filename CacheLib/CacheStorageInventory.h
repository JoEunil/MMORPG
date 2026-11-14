#pragma once
#include <tuple>
#include <memory>
#include <functional>

#include <CoreLib/MessageTypes.h>
#include <CoreLib/Message.h>
#include <CoreLib/LoggerGlobal.h>
#include <mysqlconn/include/mysql/jdbc.h>

#include "CacheStorage.h"
#include <CoreLib/IInventoryOperations.h>
#include <array>
#include <vector>


namespace Cache {
    struct FlushCommand;
    constexpr size_t RING_SIZE = 32;
    constexpr size_t RING_SIZE_MASK = 31;
    inline constexpr uint32_t INVENTORY_EQUIPMENT_TAG = 0x32514549; // IEQ2
    struct InventoryData {
        uint16_t count;
        Core::MsgInventoryItem items[MAX_INVENTORY];
        // blob 내부 필드라서 다른 자료구조 적용이 어려움.
        uint64_t recentEventIds[RING_SIZE];   // dedup ring (자동 truncate)
        uint8_t  head; // outbox CLAIMED 시 갱신 
        uint8_t  tail;
        bool lastOp; // 1: push, 0: pop
        uint64_t lastLsn;
        uint32_t equipmentFormatTag = INVENTORY_EQUIPMENT_TAG;
        uint32_t equippedWeaponItemID = 0;
        uint32_t equippedShieldItemID = 0;
        uint32_t equippedBodyItemID = 0;
    };

    constexpr Core::MsgInventoryItem EMPTY_SLOT = { 0, 0, 0 };
    constexpr InventoryData EMPTY_INVENTORY = {
        0,
        {} // 모든 원소 0으로 초기화
    };

    struct InventoryKey {
        uint64_t characterID;
        bool operator==(const InventoryKey& other) const noexcept {
            return characterID == other.characterID;
        }
        bool operator<(const InventoryKey& other) const noexcept {
            return characterID < other.characterID;
        }
    };
    struct InventoryKeyHash {
        std::size_t operator()(const InventoryKey& key) const noexcept {
            return std::hash<uint64_t>()(key.characterID);
        }
    };

    using InventoryCacheItem = CacheItem<InventoryData>;

    class CacheStorageInventory : public CacheStorage<InventoryKey, InventoryCacheItem, InventoryKeyHash>, public Core::IInventoryOperations {
        using Key = InventoryKey;
        using KeyHash = InventoryKeyHash;
        using Result = InventoryCacheItem;
        std::function<uint64_t(uint64_t, const InventoryData&)>  m_walFn;

        struct PendingReward {
            uint32_t itemID;
            uint16_t quantity;
            std::function<void(Core::ItemOperationResult)> completed;
        };
        std::array<std::unordered_map<uint64_t, std::vector<PendingReward>>, SHARD_SIZE> pendingRewards;
        std::array<std::unordered_map<uint64_t, std::vector<std::function<void(Core::EquipmentStateResult)>>>, SHARD_SIZE> pendingEquipmentLoads;
        Core::ItemOperationResult ChangeItemLocked(uint16_t shardIndex, Key key, uint32_t itemID, int32_t quantity);
        void CompleteLoad(uint16_t shardIndex, Key key, const Result* loaded);
        CACHE_STATUS LoadFromDB(uint16_t shardIndex, Key& key);
        bool RestoreEntry(uint64_t characterID, InventoryData& rec);

        void SetWalFn(std::function<uint64_t(uint64_t, const InventoryData&)> f) {
            m_walFn = std::move(f);
        }
        friend class WALManager;
        friend class InventoryPotionFixture;
    public:
        Core::EquipmentStateResult GetEquipmentState(uint64_t characterID) override;
        Core::EquipmentStateResult EquipItem(uint64_t characterID, uint8_t slot, uint32_t itemID) override;
        void LoadEquipmentState(uint64_t characterID, std::function<void(Core::EquipmentStateResult)> completed) override;
        Core::ItemOperationResult ConsumeItem(uint64_t characterID, uint32_t itemID) override;
        void GrantItem(uint64_t characterID, uint32_t itemID, uint16_t quantity,
            std::function<void(Core::ItemOperationResult)> completed) override;
        InventoryCacheItem Getter(uint64_t characterID);
        std::tuple<CACHE_STATUS, uint32_t, uint16_t, uint16_t> PartialUpdate(uint64_t characterID, uint32_t itemID, uint8_t op, int32_t change);
        static std::unique_ptr<FlushCommand> GetFlushCommand(Key key, Result result);
        // outbox → inbox 배송. AVAILABLE: 지급됨 / DUPLICATED: 이미 지급(스킵) / BLOCKED: 인벤·링 가득
        CACHE_STATUS DeliverItem(uint64_t characterID, uint64_t event_id, uint32_t itemID, uint32_t quantity);
        bool AdvanceInboxHead(uint16_t shardIndex, Key key, uint8_t snapshotHead, uint8_t claimedCnt);
        std::string ResultToString(const InventoryKey& key, const InventoryCacheItem& result) override {
            std::ostringstream oss;
            oss << "char_id: " << key.characterID << ", inventory_hex: ";
            const uint8_t* raw = reinterpret_cast<const uint8_t*>(&result.data);
            for (size_t i = 0; i < sizeof(InventoryData); i++)
                oss << std::hex << std::setw(2) << std::setfill('0') << (int)raw[i];
            return oss.str();
        }
    };
}
