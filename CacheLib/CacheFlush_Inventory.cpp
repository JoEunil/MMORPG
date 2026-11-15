#include "CacheFlush.h"
#include "InventoryCodec.h"

namespace Cache {
    void CacheFlush::FlushInventory(FlushCommand* command) {
        const InventoryKey key{ std::any_cast<uint64_t>(command->params[1]) };
        const auto shardIndex = static_cast<uint16_t>(key.characterID & SHARD_SIZE_MASK);
        auto& blob = std::any_cast<std::vector<uint8_t>&>(command->params[0]);
        const auto inventory = DecodeInventory(blob.data(), blob.size());
        if (!TryWriteStorage(*command, *cache_inventory, key, shardIndex, blob))
            return;

        CrashPoint("CLAIM"); // 인벤토리 blob durable 이후, outbox CLAIM 전

        const bool retryClaim = ClaimPendingOutboxEvents(key, shardIndex, inventory);
        cache_inventory->WriteDone(shardIndex, key, command->generation, retryClaim);
        if (m_invFlushedFn)
            m_invFlushedFn(key.characterID, inventory.lastLsn);
    }

    bool CacheFlush::ClaimPendingOutboxEvents(const InventoryKey& key, uint16_t shardIndex, const InventoryData& inventory) {
        uint8_t pendingCnt = (inventory.tail - inventory.head) & RING_SIZE_MASK;
        if (pendingCnt == 0 && inventory.lastOp)
            pendingCnt = RING_SIZE; // full
        if (pendingCnt == 0)
            return false;

        auto bazaarConn = connectionPoolBazaar->AcquireScoped();
        if (!bazaarConn) {
            // 인벤토리 저장 결과는 유지하고 남은 claim은 다음 flush에서 재시도한다.
            Core::errorLogger->LogError("cache flush", "bazaar connection acquire failed case 6", "char_id", key.characterID);
            return true;
        }

        uint8_t claimedCnt = 0;
        try {
            uint8_t idx = inventory.head;
            for (; claimedCnt < pendingCnt; ++claimedCnt, idx = (idx + 1) & RING_SIZE_MASK) {
                // 0 rows(이미 CLAIMED)도 성공으로 진행한다.
                bazaarConn->ExecuteUpdate(StatementId::ClaimBuyerOutboxEvent, inventory.recentEventIds[idx], key.characterID);
            }
        }
        catch (const sql::SQLException& e) {
            Core::errorLogger->LogError("cache flush", "outbox claim exception case 6",
                "code", e.getErrorCode(), "msg", e.what(), "char_id", key.characterID);
        }
        bazaarConn.reset();

        // WriteDone 전에 성공한 만큼 head를 전진시켜 다음 flush에서 저장되도록 한다.
        if (claimedCnt > 0 && !cache_inventory->AdvanceInboxHead(shardIndex, key, inventory.head, claimedCnt))
            Core::gameLogger->LogInfo("cache flush", "inbox advance failed", "char_id", key.characterID);
        return claimedCnt < pendingCnt;
    }
}
