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


#include <CacheLib/BazaarSearchResult.h>
#include <CacheLib/Handler.h>
#include <CacheLib/DBWorker.h>
#include <CoreLib/IMessageQueue.h>
#include <stdexcept>
#include <limits>
#include <string_view>

namespace {
    struct SearchRows {
        int count = 0;
        int index = -1;
        int invalidAt = -1;
        int throwAt = -1;
        uint64_t itemID = 1;
        int64_t quantity = 1;
        bool next() {
            ++index;
            if (index == throwAt) throw std::runtime_error("fixture read failure");
            return index < count;
        }
        uint64_t getUInt64(const char* name) {
            const std::string_view field(name);
            if (field == "item_id") return index == invalidAt ? itemID : 1;
            if (field == "listing_id") return 100 + index;
            if (field == "seller_id") return 200 + index;
            return 0; // Price and registration time zero are valid, preserved values.
        }
        int64_t getInt64(const char*) { return index == invalidAt ? quantity : 1; }
    };
    class SearchResponseQueue : public Core::IMessageQueue {
    public:
        std::unique_ptr<Core::Message> received;
        bool EnqueueMessage(Core::Message* msg) override {
            received = std::make_unique<Core::Message>(msg->GetLength());
            std::memcpy(received->GetBuffer(), msg->GetBuffer(), msg->GetLength());
            received->SetLength(msg->GetLength());
            return true;
        }
    };
}

TEST(BazaarSearchParser, NullIsFailureButZeroRowsIsSuccessWithRequestIdAndNoResidue) {
    Core::MsgBazaarSearchResBody body;
    std::memset(&body, 0xCD, sizeof(body));
    Cache::ParseBazaarSearchResult<SearchRows>(nullptr, 123, body);
    EXPECT_EQ(body.requestID, 123);
    EXPECT_EQ(body.resStatus, 0);
    EXPECT_EQ(body.count, 0);
    EXPECT_EQ(body.listings[29].listingID, 0);
    SearchRows empty;
    Cache::ParseBazaarSearchResult(&empty, 124, body);
    EXPECT_EQ(body.requestID, 124);
    EXPECT_EQ(body.resStatus, 1);
    EXPECT_EQ(body.count, 0);
    EXPECT_EQ(body.listings[29].itemID, 0);
}

TEST(BazaarSearchParser, ThirtyRowsFitBothMessagePoolsAndTruncateOnlyToSqlLimit) {
    static_assert(sizeof(Core::MsgStruct<Core::MsgBazaarSearchResBody>) <= Core::MESSAGE_LEN);
    static_assert(sizeof(Core::MsgStruct<Core::MsgBazaarSearchResBody>) <= Cache::MESSAGE_LEN);
    SearchRows rows;
    rows.count = 31;
    Core::MsgBazaarSearchResBody body{};
    Cache::ParseBazaarSearchResult(&rows, 125, body);
    EXPECT_EQ(body.resStatus, 1);
    EXPECT_EQ(body.count, 30);
    EXPECT_EQ(body.listings[29].listingID, 129);
    EXPECT_EQ(body.listings[29].sellerCharacterID, 229);
    EXPECT_EQ(body.listings[29].registeredAt, 0);
    EXPECT_EQ(body.listings[29].price, 0);
    EXPECT_EQ(rows.index, 29);
}

TEST(BazaarSearchParser, NarrowingOverflowNegativeQuantityAndPartialReadFailureEraseAllRows) {
    for (int kind = 0; kind < 5; ++kind) {
        SearchRows rows;
        rows.count = 4;
        rows.invalidAt = 2;
        if (kind == 0) rows.itemID = uint64_t{1} << 32;
        if (kind == 1) rows.quantity = 65536;
        if (kind == 2) rows.quantity = -1;
        if (kind == 3) rows.itemID = 0;
        if (kind == 4) rows.throwAt = 2;
        Core::MsgBazaarSearchResBody body;
        std::memset(&body, 0xCD, sizeof(body));
        Cache::ParseBazaarSearchResult(&rows, 126, body);
        EXPECT_EQ(body.requestID, 126) << kind;
        EXPECT_EQ(body.resStatus, 0) << kind;
        EXPECT_EQ(body.count, 0) << kind;
        for (const auto& row : body.listings) {
            EXPECT_EQ(row.listingID, 0);
            EXPECT_EQ(row.itemID, 0);
            EXPECT_EQ(row.quantity, 0);
        }
    }
}

namespace Cache {
    class BazaarSearchFixture : public ::testing::Test {
    protected:
        std::unique_ptr<MessagePool> messages = std::make_unique<MessagePool>();
        std::unique_ptr<DBWorker<DBConnectionBazaar>> stoppedWorker = std::make_unique<DBWorker<DBConnectionBazaar>>();
        SearchResponseQueue responses;
        Handler handler;
        void SetUp() override {
            handler.messageQ = &responses;
            handler.messagePool = messages.get();
            handler.dbWorkerBazaar = stoppedWorker.get();
        }
        void RequestSearch() {
            auto* msg = messages->Acquire();
            auto* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarSearchBody>*>(msg->GetBuffer());
            st->header = {42, Core::MSG_BAZAAR_SEARCH};
            st->body = {0x12345678, 3, 65535};
            msg->SetLength(sizeof(*st));
            handler.Process(msg); // Stopped queue rejects without connecting to any DB.
        }
    };
    TEST_F(BazaarSearchFixture, StoppedDbWorkerReturnsExplicitCorrelatedFailure) {
        RequestSearch();
        ASSERT_NE(responses.received, nullptr);
        EXPECT_EQ(responses.received->GetLength(), sizeof(Core::MsgStruct<Core::MsgBazaarSearchResBody>));
        const auto* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarSearchResBody>*>(responses.received->GetBuffer());
        EXPECT_EQ(st->header.sessionID, 42);
        EXPECT_EQ(st->header.messageType, Core::MSG_BAZAAR_SEARCH_RES);
        EXPECT_EQ(st->body.requestID, 0x12345678);
        EXPECT_EQ(st->body.resStatus, 0);
        EXPECT_EQ(st->body.count, 0);
        EXPECT_EQ(st->body.listings[29].listingID, 0);
    }
}

#include <CacheLib/BazaarMyListResult.h>
namespace {
    struct MyListRows {
        int count = 0;
        int index = -1;
        int invalidAt = -1;
        int throwAt = -1;
        const char* nullColumn = nullptr;
        const char* throwColumn = nullptr;
        uint64_t itemID = 1;
        uint64_t owner = 5011;
        uint64_t listingID = 100;
        int64_t quantity = 1;
        int64_t status = 0;
        bool next() {
            ++index;
            if (index == throwAt) throw std::runtime_error("my list fixture next");
            return index < count;
        }
        bool isNull(const char* column) {
            return index == invalidAt && nullColumn && std::string_view(column) == nullColumn;
        }
        uint64_t getUInt64(const char* column) {
            const std::string_view field(column);
            if (index == invalidAt && throwColumn && field == throwColumn)
                throw std::runtime_error("my list fixture partial read");
            if (field == "item_id") return index == invalidAt ? itemID : 1;
            if (field == "listing_id") return index == invalidAt ? listingID : 100 + index;
            if (field == "seller_id") return index == invalidAt ? owner : 5011;
            if (field == "price") return (std::numeric_limits<uint64_t>::max)();
            return 0;
        }
        int64_t getInt64(const char* column) {
            if (std::string_view(column) == "quantity") return index == invalidAt ? quantity : 1;
            return index == invalidAt ? status : index % 3;
        }
    };
}

TEST(BazaarMyListParser, NullAndEmptyResultsKeepCorrelationAndClearAllRows) {
    Core::MsgBazaarMyListResBody body;
    std::memset(&body, 0xCD, sizeof(body));
    Cache::ParseBazaarMyListResult<MyListRows>(nullptr, 123, 5011, body);
    EXPECT_EQ(body.requestID, 123);
    EXPECT_EQ(body.characterID, 5011);
    EXPECT_EQ(body.resStatus, 0);
    EXPECT_EQ(body.count, 0);
    EXPECT_EQ(body.listings[29].listingID, 0);
    MyListRows rows;
    Cache::ParseBazaarMyListResult(&rows, 124, 5011, body);
    EXPECT_EQ(body.requestID, 124);
    EXPECT_EQ(body.characterID, 5011);
    EXPECT_EQ(body.resStatus, 1);
    EXPECT_EQ(body.count, 0);
    EXPECT_EQ(body.listings[29].quantity, 0);
}

TEST(BazaarMyListParser, OneAndThirtyRowsPreserveEveryStateAndFullWidthValues) {
    static_assert(sizeof(Core::MsgStruct<Core::MsgBazaarMyListResBody>) <= Core::MESSAGE_LEN);
    static_assert(sizeof(Core::MsgStruct<Core::MsgBazaarMyListResBody>) <= Cache::MESSAGE_LEN);
    for (int count : {1, 30}) {
        MyListRows rows;
        rows.count = count;
        Core::MsgBazaarMyListResBody body{};
        Cache::ParseBazaarMyListResult(&rows, 125, 5011, body);
        ASSERT_EQ(body.resStatus, 1);
        ASSERT_EQ(body.count, count);
        for (int i = 0; i < count; ++i) {
            EXPECT_EQ(body.listings[i].listingID, 100 + i);
            EXPECT_EQ(body.listings[i].sellerCharacterID, 5011);
            EXPECT_EQ(body.listings[i].status, i % 3);
            EXPECT_EQ(body.listings[i].price, (std::numeric_limits<uint64_t>::max)());
            EXPECT_EQ(body.listings[i].registeredAt, 0);
        }
    }
}

TEST(BazaarMyListParser, InvalidOrPartialDatabaseResultsNeverExposeAPrefix) {
    for (int kind = 0; kind < 12; ++kind) {
        MyListRows rows;
        rows.count = 4;
        rows.invalidAt = 2;
        if (kind == 0) rows.itemID = uint64_t{1} << 32;
        if (kind == 1) rows.quantity = 65536;
        if (kind == 2) rows.quantity = -1;
        if (kind == 3) rows.itemID = 0;
        if (kind == 4) rows.throwAt = 2;
        if (kind == 5) rows.status = 255;
        if (kind == 6) rows.status = -1;
        if (kind == 7) rows.owner = 5012;
        if (kind == 8) rows.listingID = 0;
        if (kind == 9) rows.nullColumn = "listed_at";
        if (kind == 10) rows.throwColumn = "price";
        if (kind == 11) rows.count = 31;
        Core::MsgBazaarMyListResBody body;
        std::memset(&body, 0xCD, sizeof(body));
        Cache::ParseBazaarMyListResult(&rows, 126, 5011, body);
        EXPECT_EQ(body.requestID, 126) << kind;
        EXPECT_EQ(body.characterID, 5011) << kind;
        EXPECT_EQ(body.resStatus, 0) << kind;
        EXPECT_EQ(body.count, 0) << kind;
        for (const auto& row : body.listings) {
            EXPECT_EQ(row.listingID, 0) << kind;
            EXPECT_EQ(row.sellerCharacterID, 0) << kind;
            EXPECT_EQ(row.itemID, 0) << kind;
            EXPECT_EQ(row.quantity, 0) << kind;
            EXPECT_EQ(row.price, 0) << kind;
            EXPECT_EQ(row.registeredAt, 0) << kind;
            EXPECT_EQ(row.status, 0) << kind;
        }
    }
}

namespace Cache {
    class BazaarMyListFixture : public ::testing::Test {
    protected:
        std::unique_ptr<MessagePool> messages = std::make_unique<MessagePool>();
        std::unique_ptr<DBWorker<DBConnectionBazaar>> stoppedWorker = std::make_unique<DBWorker<DBConnectionBazaar>>();
        SearchResponseQueue responses;
        Handler handler;
        void SetUp() override {
            handler.messageQ = &responses;
            handler.messagePool = messages.get();
            handler.dbWorkerBazaar = stoppedWorker.get();
        }
        void RequestMyList() {
            auto* msg = messages->Acquire();
            auto* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarMyListBody>*>(msg->GetBuffer());
            st->header = {42, Core::MSG_BAZAAR_MY_LIST};
            st->body = {0x12345678, 5011};
            msg->SetLength(sizeof(*st));
            handler.Process(msg); // Rejected without any DB connection.
        }
    };
    TEST_F(BazaarMyListFixture, StoppedWorkerReturnsZeroedFailureWithRequestAndOwner) {
        RequestMyList();
        ASSERT_NE(responses.received, nullptr);
        EXPECT_EQ(responses.received->GetLength(), sizeof(Core::MsgStruct<Core::MsgBazaarMyListResBody>));
        const auto* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarMyListResBody>*>(responses.received->GetBuffer());
        EXPECT_EQ(st->header.sessionID, 42);
        EXPECT_EQ(st->header.messageType, Core::MSG_BAZAAR_MY_LIST_RES);
        EXPECT_EQ(st->body.requestID, 0x12345678);
        EXPECT_EQ(st->body.characterID, 5011);
        EXPECT_EQ(st->body.resStatus, 0);
        EXPECT_EQ(st->body.count, 0);
        EXPECT_EQ(st->body.listings[29].listingID, 0);
    }
}
