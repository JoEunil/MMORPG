#include <gtest/gtest.h>
#include <algorithm>

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
#include <CacheLib/BazaarRegisterResult.h>
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
        bool accept = true;
        bool EnqueueMessage(Core::Message* msg) override {
            if (!accept) return false;
            received = std::make_unique<Core::Message>(msg->GetLength());
            std::memcpy(received->GetBuffer(), msg->GetBuffer(), msg->GetLength());
            received->SetLength(msg->GetLength());
            return true;
        }
    };
    class RegisterResponseQueue : public Core::IMessageQueue {
    public:
        int attempts = 0;
        uint8_t status = 255;
        bool accept = true;
        bool throws = false;
        bool EnqueueMessage(Core::Message* msg) override {
            ++attempts;
            const auto* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarRegisterResBody>*>(msg->GetBuffer());
            EXPECT_EQ(msg->GetLength(), sizeof(*st));
            EXPECT_EQ(st->header.sessionID, 42);
            EXPECT_EQ(st->header.messageType, Core::MSG_BAZAAR_REGISTER_RES);
            status = st->body.resStatus;
            EXPECT_EQ(st->body.requestID, 0x12345678);
            EXPECT_EQ(st->body.characterID, Character);
            if (throws) throw std::runtime_error("fixture response queue threw");
            return accept;
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
        void RequestBuy(int delta = 0) {
            auto* msg = messages->Acquire();
            auto* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarBuyBody>*>(msg->GetBuffer());
            st->header = {42, Core::MSG_BAZAAR_BUY}; st->body = {5011, UINT64_MAX, 123};
            msg->SetLength(sizeof(*st) + delta); handler.Process(msg);
        }
        void RequestOutbox(int delta = 0) {
            auto* msg = messages->Acquire();
            auto* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarCheckOutboxBody>*>(msg->GetBuffer());
            st->header = {42, Core::MSG_BAZAAR_CHECK_OUTBOX}; st->body = {5011, 124};
            msg->SetLength(sizeof(*st) + delta); handler.Process(msg);
        }
        void RequestClaim(int delta = 0) {
            auto* msg = messages->Acquire();
            auto* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarClaimBody>*>(msg->GetBuffer());
            st->header = {42, Core::MSG_BAZAAR_CLAIM};
            st->body = {5011, UINT64_MAX, 0x12345678};
            msg->SetLength(sizeof(*st) + delta);
            handler.Process(msg);
        }
        void RequestRegister(InventoryHarness& inventory, CurrencyHarness& currency, RegisterResponseQueue& queue, uint16_t quantity = 3, int lengthDelta = 0) {
            handler.cache_inventory = &inventory;
            handler.cache_currency = &currency;
            handler.messageQ = &queue;
            auto* msg = messages->Acquire();
            auto* st = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarRegisterBody>*>(msg->GetBuffer());
            st->header = {42, Core::MSG_BAZAAR_REGISTER};
            st->body = {Character, 1, quantity, 100, 0x12345678, 0};
            msg->SetLength(sizeof(*st) + lengthDelta);
            if (lengthDelta != 0) handler.Process(msg);
            else {
                handler.BazaarRegister(msg, 42, &st->body);
                EXPECT_EQ(msg, nullptr);
            }
            handler.messageQ = &responses;
        }
        void ExpectPoolRestored() {
            std::vector<Core::Message*> held;
            for (size_t i = 0; i < MSGPOOL_SIZE; ++i) {
                auto* msg = messages->Acquire();
                ASSERT_NE(msg, nullptr);
                EXPECT_EQ(std::find(held.begin(), held.end(), msg), held.end());
                held.push_back(msg);
            }
            EXPECT_EQ(messages->Acquire(), nullptr);
            for (auto* msg : held) messages->Return(msg);
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

#include <CacheLib/BazaarClaimResult.h>
namespace {
    struct ClaimRows {
        int64_t status = 1;
        uint64_t amount = UINT64_MAX;
        bool empty = false, throws = false;
        bool next() { if (throws) throw std::runtime_error("fixture SQL read failure"); return !empty; }
        bool isNull(const char*) { return false; }
        int64_t getInt64(const char*) { return status; }
        uint64_t getUInt64(const char*) { return amount; }
    };
}
namespace Cache {
    TEST_F(BazaarMyListFixture, BazaarClaimSqlFixtureAndQueueRejectionKeepCorrelationAndSingleReturn) {
        ClaimRows rows;
        Core::MsgBazaarClaimResBody body{};
        ParseBazaarClaimResult(&rows, body);
        EXPECT_EQ(body.resStatus, 1); EXPECT_EQ(body.diamondClaimed, UINT64_MAX);
        rows.amount = 0; rows.status = 4;
        ParseBazaarClaimResult(&rows, body);
        EXPECT_EQ(body.resStatus, 4); EXPECT_EQ(body.diamondClaimed, 0);
        rows.amount = UINT64_MAX;
        EXPECT_THROW(ParseBazaarClaimResult(&rows, body), std::runtime_error);
        EXPECT_EQ(body.resStatus, 0); EXPECT_EQ(body.diamondClaimed, 0);
        rows.throws = true;
        EXPECT_THROW(ParseBazaarClaimResult(&rows, body), std::runtime_error);
        rows.throws = false; rows.empty = true;
        ParseBazaarClaimResult(&rows, body);
        EXPECT_EQ(body.resStatus, 0); EXPECT_EQ(body.diamondClaimed, 0);
        RequestClaim();
        ASSERT_NE(responses.received, nullptr);
        const auto* result = reinterpret_cast<Core::MsgStruct<Core::MsgBazaarClaimResBody>*>(responses.received->GetBuffer());
        EXPECT_EQ(result->header.sessionID, 42); EXPECT_EQ(result->header.messageType, Core::MSG_BAZAAR_CLAIM_RES);
        EXPECT_EQ(result->body.requestID, 0x12345678); EXPECT_EQ(result->body.characterID, 5011);
        EXPECT_EQ(result->body.listingID, UINT64_MAX);
        EXPECT_EQ(result->body.resStatus, 0); EXPECT_EQ(result->body.diamondClaimed, 0);
        ExpectPoolRestored();
        responses.received.reset(); responses.accept = false;
        RequestClaim(); EXPECT_EQ(responses.received, nullptr); ExpectPoolRestored();
        responses.accept = true;
        RequestClaim(-1); EXPECT_EQ(responses.received, nullptr); ExpectPoolRestored();
    }
}

#include <CacheLib/BazaarPurchaseResult.h>
TEST(BazaarRegisterSafety, SqlOutcomeNeverRefundsAnUnknownCommitAndRestoresAtMostOnce) {
    // The product helper owns this decision boundary; no DB connection or durable writes are used.
    for (int scenario = 0; scenario < 7; ++scenario) {
        SCOPED_TRACE(scenario);
        int inserts = 0, inventoryRestores = 0, goldRestores = 0, unknownReports = 0;
        const auto status = Cache::ResolveBazaarRegister([&] {
            ++inserts;
            if (scenario == 1) throw sql::SQLException("fixture result lost after commit");
            if (scenario == 2) throw 7; // Non-SQL worker failure is also uncertain.
            if (scenario == 0) return 1;
            if (scenario == 3) return -1;
            return 0;
        }, [&] {
            ++inventoryRestores;
            if (scenario == 6) throw std::runtime_error("fixture partially applied restore");
            ++goldRestores;
            return scenario != 5;
        }, [&] { ++unknownReports; });
        EXPECT_EQ(status, scenario == 0 ? 1 : scenario == 4 ? 0 : 5);
        EXPECT_EQ(inserts, 1);
        EXPECT_EQ(inventoryRestores, scenario >= 4 ? 1 : 0);
        EXPECT_EQ(goldRestores, scenario >= 4 && scenario != 6 ? 1 : 0);
        EXPECT_EQ(unknownReports, scenario == 0 || scenario == 4 ? 0 : 1);
    }
}

namespace Cache {
    TEST_F(BazaarMyListFixture, BazaarRegisterRejectionAndResponseFailureReturnOneMessageAndRestoreOnce) {
        // Actual Handler + stopped DBWorker: no query can have run before rejection.
        for (int scenario = 0; scenario < 8; ++scenario) {
            SCOPED_TRACE(scenario);
            InventoryHarness inventory;
            inventory.Seed();
            ASSERT_EQ(std::get<0>(inventory.PartialUpdate(Character, 1, 1, 3)), CACHE_STATUS::AVAILABLE);
            const uint64_t initialGold = scenario == 3 ? 0 : 100;
            CurrencyHarness currency(initialGold);
            RegisterResponseQueue queue;
            queue.accept = scenario != 1;
            queue.throws = scenario == 2 || scenario == 7;
            const uint16_t quantity = scenario == 4 ? 4 : scenario == 5 || scenario == 7 ? 0 : 3;
            EXPECT_NO_THROW(RequestRegister(inventory, currency, queue, quantity, scenario == 6 ? -1 : 0));
            EXPECT_EQ(queue.attempts, scenario == 6 ? 0 : 1);
            if (scenario != 6) EXPECT_EQ(queue.status, scenario == 3 ? 3 : scenario == 4 ? 2 : scenario == 5 || scenario == 7 ? 4 : 0);
            EXPECT_EQ(inventory.Getter(Character).data.items[0].quantity, 3);
            EXPECT_EQ(currency.Getter(Character).data.gold, initialGold);
            ExpectPoolRestored(); // Pool exhaustion + distinct addresses detect both leaks and double Return.
        }
    }
}

namespace {
    struct BuyRows {
        int64_t status = 1;
        uint64_t itemID = UINT32_MAX, quantity = UINT32_MAX, price = UINT64_MAX;
        bool empty = false, throws = false;
        bool next() { if (throws) throw std::runtime_error("fixture SQL result failure"); return !empty; }
        bool isNull(const char*) { return false; }
        int64_t getInt64(const char*) { return status; }
        uint64_t getUInt64(const char* column) {
            if (std::string_view(column) == "item_id") return itemID;
            if (std::string_view(column) == "quantity") return quantity;
            return price;
        }
    };
    struct OutboxRows {
        int count = 3, index = -1, throwAt = -1;
        bool next() { ++index; if (index == throwAt) throw std::runtime_error("fixture partial read failure"); return index < count; }
        bool isNull(const char*) { return false; }
        uint64_t getUInt64(const char* column) {
            if (std::string_view(column) == "event_id") return 100 + index;
            if (std::string_view(column) == "item_id") return 1;
            return index == 2 ? UINT32_MAX : 1;
        }
    };
}
namespace Cache {
    TEST_F(BazaarMyListFixture, BazaarPurchaseSqlAndDeliveryPrefixKeepDedupAndSingleReturn) {
        BuyRows rows;
        Core::MsgBazaarBuyResBody bought{};
        ParseBazaarBuyResult(&rows, bought);
        EXPECT_EQ(bought.resStatus, 1); EXPECT_EQ(bought.itemID, UINT32_MAX);
        EXPECT_EQ(bought.quantity, UINT32_MAX); EXPECT_EQ(bought.diamondSpent, UINT64_MAX);
        rows.status = 4; rows.itemID = rows.quantity = rows.price = 0;
        ParseBazaarBuyResult(&rows, bought); EXPECT_EQ(bought.resStatus, 4);
        rows.status = 3;
        ParseBazaarBuyResult(&rows, bought); EXPECT_EQ(bought.resStatus, 3);
        rows.status = 2;
        EXPECT_THROW(ParseBazaarBuyResult(&rows, bought), std::runtime_error); EXPECT_EQ(bought.resStatus, 5);
        rows.status = 1; rows.itemID = uint64_t{UINT32_MAX} + 1;
        EXPECT_THROW(ParseBazaarBuyResult(&rows, bought), std::runtime_error); EXPECT_EQ(bought.resStatus, 5);
        rows.throws = true;
        EXPECT_THROW(ParseBazaarBuyResult(&rows, bought), std::runtime_error); EXPECT_EQ(bought.resStatus, 5);
        ParseBazaarBuyResult<BuyRows>(nullptr, bought); EXPECT_EQ(bought.resStatus, 5);
        InventoryHarness inventory; inventory.Seed();
        const auto deliver = [&](uint64_t eventID, uint32_t itemID, uint32_t quantity) { return inventory.DeliverItem(Character, eventID, itemID, quantity); };
        OutboxRows events;
        Core::MsgBazaarCheckOutboxResBody delivered{};
        DeliverBazaarOutboxResult(&events, deliver, delivered);
        EXPECT_EQ(delivered.resStatus, 1); EXPECT_EQ(delivered.deliveredCount, 2); EXPECT_EQ(delivered.blockedCount, 1);
        EXPECT_EQ(inventory.Getter(Character).data.items[0].quantity, 2);
        events.index = -1;
        DeliverBazaarOutboxResult(&events, deliver, delivered);
        EXPECT_EQ(delivered.deliveredCount, 0); EXPECT_EQ(delivered.duplicatedCount, 2); EXPECT_EQ(delivered.blockedCount, 1);
        EXPECT_EQ(inventory.Getter(Character).data.items[0].quantity, 2);
        events.index = -1; events.throwAt = 1;
        DeliverBazaarOutboxResult(&events, deliver, delivered);
        EXPECT_EQ(delivered.resStatus, 0); EXPECT_EQ(delivered.duplicatedCount, 1);
        EXPECT_EQ(inventory.Getter(Character).data.items[0].quantity, 2);
        RequestBuy(); ASSERT_NE(responses.received, nullptr);
        const auto* buy = Core::parseMsgBody<Core::MsgBazaarBuyResBody>(responses.received->GetBuffer());
        EXPECT_EQ(buy->resStatus, 5); EXPECT_EQ(buy->requestID, 123); EXPECT_EQ(buy->characterID, 5011); EXPECT_EQ(buy->listingID, UINT64_MAX);
        EXPECT_EQ(buy->itemID, 0); EXPECT_EQ(buy->quantity, 0); EXPECT_EQ(buy->diamondSpent, 0); ExpectPoolRestored();
        RequestOutbox(); ASSERT_NE(responses.received, nullptr);
        const auto* outbox = Core::parseMsgBody<Core::MsgBazaarCheckOutboxResBody>(responses.received->GetBuffer());
        EXPECT_EQ(outbox->resStatus, 0); EXPECT_EQ(outbox->requestID, 124); EXPECT_EQ(outbox->characterID, 5011);
        EXPECT_EQ(outbox->deliveredCount, 0); EXPECT_EQ(outbox->duplicatedCount, 0); EXPECT_EQ(outbox->blockedCount, 0); ExpectPoolRestored();
        responses.received.reset(); responses.accept = false;
        RequestBuy(); RequestOutbox(); EXPECT_EQ(responses.received, nullptr); ExpectPoolRestored();
        responses.accept = true;
        RequestBuy(-1); RequestOutbox(1); EXPECT_EQ(responses.received, nullptr); ExpectPoolRestored();
    }
}
