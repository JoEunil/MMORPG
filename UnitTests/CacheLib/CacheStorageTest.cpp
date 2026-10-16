#include <gtest/gtest.h>

#include <CacheLib/CacheFlush.h>
#include <CacheLib/CacheStorageInventory.h>
#include <CacheLib/CacheStorageCurrency.h>
#include <BaseLib/WAL.h>

#include <chrono>
#include <filesystem>
#include <vector>

namespace {
    constexpr uint64_t Character = 16;
    constexpr uint16_t Shard = Character & Cache::SHARD_SIZE_MASK;

    class InventoryHarness : public Cache::CacheStorageInventory {
    public:
        std::vector<std::unique_ptr<Cache::FlushCommand>> writes;
        bool acceptWrites = true;

        InventoryHarness() {
            m_shards.resize(Cache::SHARD_SIZE);
            SetFlushFn([this](const Cache::Key5& key, Cache::Result5& result) {
                if (!acceptWrites) return Cache::FlushEnqueueResult::Failed;
                writes.push_back(GetFlushCommand(key, result));
                return Cache::FlushEnqueueResult::Queued;
            });
        }
        void Seed(uint64_t id = Character) {
            Cache::Result5 result{};
            result.data = Cache::EMPTY_INVENTORY;
            Insert(id & Cache::SHARD_SIZE_MASK, Cache::Key5{ id }, result);
        }
        void Dispatch() {
            ForEachDirty([this](const Cache::Key5& key, Cache::Result5& result) {
                if (!acceptWrites) return false;
                writes.push_back(GetFlushCommand(key, result));
                return true;
            });
        }
        Cache::InventoryData Image(size_t write) const {
            Cache::InventoryData result{};
            const auto& blob = std::any_cast<const std::vector<uint8_t>&>(writes[write]->params[0]);
            std::memcpy(&result, blob.data(), sizeof(result));
            return result;
        }
    };

    class CurrencyHarness : public Cache::CacheStorageCurrency {
    public:
        CurrencyHarness(uint64_t gold) {
            m_shards.resize(Cache::SHARD_SIZE);
            Cache::Result7 result{};
            result.data.gold = gold;
            Insert(Shard, Cache::Key7{ Character }, result);
        }
    };
}

TEST(CacheStorageRegression, StackCountAndUnsignedQuantityRange) {
    InventoryHarness cache;
    cache.Seed();
    EXPECT_EQ(std::get<0>(cache.PartialUpdate(Character, 1, 1, 40000)), Cache::CACHE_STATUS::AVAILABLE);
    EXPECT_EQ(std::get<0>(cache.PartialUpdate(Character, 1, 1, 25535)), Cache::CACHE_STATUS::AVAILABLE);
    auto result = cache.Getter(Character);
    ASSERT_EQ(result.data.count, 1);
    ASSERT_EQ(result.data.items[0].quantity, UINT16_MAX);

    EXPECT_EQ(std::get<0>(cache.PartialUpdate(Character, 1, 2, -40000)), Cache::CACHE_STATUS::AVAILABLE);
    EXPECT_EQ(cache.Getter(Character).data.items[0].quantity, 25535);
    EXPECT_EQ(std::get<0>(cache.PartialUpdate(Character, 1, 2, -25535)), Cache::CACHE_STATUS::AVAILABLE);
    EXPECT_EQ(cache.Getter(Character).data.count, 0);
}

TEST(CacheStorageRegression, RejectInvalidAmountsWithoutMutation) {
    InventoryHarness cache;
    cache.Seed();
    EXPECT_EQ(std::get<0>(cache.PartialUpdate(Character, 1, 1, 0)), Cache::CACHE_STATUS::BLOCKED);
    EXPECT_EQ(std::get<0>(cache.PartialUpdate(Character, 1, 1, -1)), Cache::CACHE_STATUS::BLOCKED);
    EXPECT_EQ(std::get<0>(cache.PartialUpdate(Character, 1, 2, INT32_MAX)), Cache::CACHE_STATUS::BLOCKED);
    EXPECT_EQ(cache.DeliverItem(Character, 1, 1, UINT32_MAX), Cache::CACHE_STATUS::BLOCKED);
    EXPECT_EQ(cache.DeliverItem(Character, 1, 0, 1), Cache::CACHE_STATUS::BLOCKED);
    EXPECT_EQ(cache.Getter(Character).data.count, 0);
}

TEST(CacheStorageRegression, SameKeySnapshotsCannotOverlapAndStaleCallbacksDoNothing) {
    InventoryHarness cache;
    cache.Seed();
    cache.PartialUpdate(Character, 1, 1, 10);
    cache.Dispatch();
    ASSERT_EQ(cache.writes.size(), 1);
    auto oldGeneration = cache.writes[0]->generation;
    cache.PartialUpdate(Character, 1, 2, 7);
    cache.Dispatch();
    EXPECT_EQ(cache.writes.size(), 1); // no second write while first is in flight

    cache.WriteDone(Shard, Cache::Key5{ Character }, oldGeneration);
    cache.Dispatch();
    ASSERT_EQ(cache.writes.size(), 2);
    EXPECT_EQ(cache.Image(0).items[0].quantity, 10);
    EXPECT_EQ(cache.Image(1).items[0].quantity, 17);
    EXPECT_NE(cache.writes[1]->generation, oldGeneration);

    cache.WriteDone(Shard, Cache::Key5{ Character }, oldGeneration);
    cache.Rollback(Shard, Cache::Key5{ Character }, oldGeneration);
    EXPECT_TRUE(cache.Getter(Character).flushInFlight);
    EXPECT_EQ(cache.writes.size(), 2);
    cache.WriteDone(Shard, Cache::Key5{ Character }, cache.writes[1]->generation);
    EXPECT_FALSE(cache.Getter(Character).flushInFlight);
}

TEST(CacheStorageRegression, RetryUsesLatestImageAndRejectsEarlierCompletion) {
    InventoryHarness cache;
    cache.Seed();
    cache.PartialUpdate(Character, 1, 1, 10);
    cache.Dispatch();
    auto original = cache.writes[0]->generation;
    cache.PartialUpdate(Character, 1, 2, 5);
    cache.Rollback(Shard, Cache::Key5{ Character }, original);
    ASSERT_EQ(cache.writes.size(), 2);
    EXPECT_EQ(cache.Image(1).items[0].quantity, 15);
    cache.WriteDone(Shard, Cache::Key5{ Character }, original);
    EXPECT_TRUE(cache.Getter(Character).flushInFlight);
    cache.WriteDone(Shard, Cache::Key5{ Character }, cache.writes[1]->generation);
    EXPECT_FALSE(cache.Getter(Character).flushInFlight);
}

TEST(CacheStorageRegression, QueueRejectionKeepsDirtyEntryRetryable) {
    InventoryHarness cache;
    cache.Seed();
    cache.PartialUpdate(Character, 1, 1, 1);
    cache.acceptWrites = false;
    cache.Dispatch();
    EXPECT_FALSE(cache.Getter(Character).flushInFlight);
    cache.acceptWrites = true;
    cache.Dispatch();
    EXPECT_EQ(cache.writes.size(), 1);
}

TEST(CacheStorageRegression, InFlightEntryCannotBeEvictedOrReloaded) {
    InventoryHarness cache;
    cache.Seed();
    cache.PartialUpdate(Character, 1, 1, 10);
    cache.Dispatch();
    auto generation = cache.writes[0]->generation;
    for (uint64_t i = 1; i <= Cache::MAX_CACHE_SIZE; ++i)
        cache.Seed(Character + i * Cache::SHARD_SIZE);
    auto result = cache.Getter(Character);
    EXPECT_EQ(result.status, Cache::CACHE_STATUS::AVAILABLE);
    EXPECT_EQ(result.data.items[0].quantity, 10);
    EXPECT_TRUE(result.flushInFlight);
    cache.WriteDone(Shard, Cache::Key5{ Character }, generation);
    EXPECT_EQ(cache.Getter(Character).status, Cache::CACHE_STATUS::AVAILABLE);
}

TEST(CacheStorageRegression, CurrencyDepositCannotWrap) {
    CurrencyHarness cache(UINT64_MAX - 3);
    EXPECT_EQ(std::get<0>(cache.DepositCurrency(Character, 4)), Cache::CACHE_STATUS::BLOCKED);
    EXPECT_EQ(cache.Getter(Character).data.gold, UINT64_MAX - 3);
    EXPECT_EQ(std::get<0>(cache.DepositCurrency(Character, 3)), Cache::CACHE_STATUS::AVAILABLE);
    EXPECT_EQ(cache.Getter(Character).data.gold, UINT64_MAX);
}

TEST(CacheWALRegression, NewNodeLsnExceedsTheLoadedDatabaseLsn) {
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto dir = std::filesystem::temp_directory_path() /
        ("game-wal-floor-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(stamp));
    auto prefix = (dir / "redo").string();
    const uint64_t databaseLsn = (uint64_t{ 42 } << 32) | 1000;
    uint64_t appended = 0;
    {
        Base::WAL wal(prefix, 1024, [](const Base::WALHeader&, const uint8_t*) {});
        const uint8_t payload = 1;
        appended = wal.Write(&payload, sizeof(payload), 1, databaseLsn);
        EXPECT_GT(appended, databaseLsn);
        EXPECT_EQ(appended >> 32, 43);
    }
    uint64_t replayed = 0;
    {
        Base::WAL wal(prefix, 1024, [&](const Base::WALHeader& header, const uint8_t*) { replayed = header.lsn; });
    }
    EXPECT_EQ(replayed, appended);
    std::filesystem::remove(dir / "redo.1");
    std::filesystem::remove(dir / "redo.43");
    std::filesystem::remove(dir);
}
