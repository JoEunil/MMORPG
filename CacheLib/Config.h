#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <string>

namespace Core {
    template<std::size_t PoolSize, uint16_t MessageLength>
    class MessagePool;
}

namespace Cache {
    inline std::string GetEnvVar(const char* name) {
#ifdef _MSC_VER
        char* buf = nullptr;
        size_t len = 0;
        if (_dupenv_s(&buf, &len, name) != 0 || buf == nullptr)
            return {};
        std::string value(buf);
        std::free(buf);
        return value;
#else
        const char* v = std::getenv(name);
        return v ? std::string(v) : std::string();
#endif
    }

    // set CRASH_POINT=DELIVER  (배송 반영 후 flush 전 abort)
    // set CRASH_POINT=CLAIM    (인벤토리 flush 후 outbox CLAIM 전 abort)
    inline void CrashPoint(const char* name) {
        static const std::string target = GetEnvVar("CRASH_POINT");
        if (!target.empty() && target == name) {
            std::printf("[CRASH_POINT] %s - abort\n", name);
            std::fflush(stdout);
            std::abort();
        }
    }

    inline constexpr uint8_t DB_CONN_POOL = 10;
    inline constexpr uint8_t FLUSH_THREADPOOL_SIZE = 2;
    inline constexpr const char* DB_HOST_GAME = "localhost";
    inline constexpr const char* DB_USER_GAME = "root";
    inline constexpr const char* DB_PASS_GAME = "1234";
    inline constexpr const char* DB_DB_GAME = "game";

    inline constexpr const char* DB_HOST_BAZAAR = "localhost";
    inline constexpr const char* DB_USER_BAZAAR = "root";
    inline constexpr const char* DB_PASS_BAZAAR = "1234";
    inline constexpr const char* DB_DB_BAZAAR = "bazaar";

    //inline constexpr size_t MAX_CACHE_SIZE = 2; // unit test
    inline constexpr size_t MAX_CACHE_SIZE = 1024;
    inline constexpr uint16_t SHARD_SIZE = 16;
    inline constexpr uint16_t SHARD_SIZE_MASK = SHARD_SIZE - 1;

    enum class StatementId : uint16_t {
        SelectCharacterList = 1,
        InsertCharacter = 2,
        SelectCharacterState = 3,
        UpdateCharacterState = 4,
        SelectInventory = 5,
        UpdateInventory = 6,
        SelectCurrency = 7,
        UpsertCurrency = 8,
        SelectDiamond = 9,
        DepositDiamond = 10,
        SelectBazaarMyList = 11,
        SearchBazaar = 12,
        InsertBazaarListing = 13,
        CancelBazaarListing = 14,
        SelectBazaarListingItem = 15,
        BuyBazaarListing = 16,
        ClaimBazaarProceeds = 17,
        SelectBuyerOutbox = 18,
        ClaimBuyerOutboxEvent = 19,
        InsertProfile = 20,
        SelectProfile = 21,
        RenameProfile = 22,
    };

    enum class StatementType : uint8_t {
        Unknown,
        GameQuery,
        GameUpdate,
        BazaarQuery,
        BazaarUpdate,
    };

    constexpr StatementType GetStatementType(StatementId id) noexcept {
        switch (id) {
        case StatementId::SelectCharacterList:
        case StatementId::SelectCharacterState:
        case StatementId::SelectInventory:
        case StatementId::SelectCurrency:
        case StatementId::SelectProfile:
            return StatementType::GameQuery;
        case StatementId::InsertCharacter:
        case StatementId::UpdateCharacterState:
        case StatementId::UpdateInventory:
        case StatementId::UpsertCurrency:
        case StatementId::InsertProfile:
        case StatementId::RenameProfile:
            return StatementType::GameUpdate;
        case StatementId::SelectDiamond:
        case StatementId::SelectBazaarMyList:
        case StatementId::SearchBazaar:
        case StatementId::CancelBazaarListing:
        case StatementId::SelectBazaarListingItem:
        case StatementId::BuyBazaarListing:
        case StatementId::ClaimBazaarProceeds:
        case StatementId::SelectBuyerOutbox:
            return StatementType::BazaarQuery;
        case StatementId::DepositDiamond:
        case StatementId::InsertBazaarListing:
        case StatementId::ClaimBuyerOutboxEvent:
            return StatementType::BazaarUpdate;
        default:
            return StatementType::Unknown;
        }
    }

    inline constexpr uint16_t MSGPOOL_SIZE = 10000;
    inline constexpr uint16_t MESSAGE_LEN = 2048;
    using CacheMessagePool = Core::MessagePool<MSGPOOL_SIZE, MESSAGE_LEN>;

    inline constexpr uint16_t THREADPOOL_SIZE = 100; 

    inline constexpr uint16_t MAX_CHARACTER_CNT = 10;
    inline constexpr uint16_t MAX_INVENTORY = 10;

    inline constexpr uint8_t MQ_THREADPOOL_SIZE = 3;

    inline constexpr size_t MQ_SIZE = 8192;
    inline constexpr size_t MQ_DEFER_SIZE = 2048;
    inline constexpr uint16_t GAME_DB_WORKER_THREADPOOL_SIZE = 2;
    inline constexpr uint16_t BAZAAR_DB_WORKER_THREADPOOL_SIZE = 2;
    inline constexpr size_t DB_WORK_QUEUE_SIZE = 4096;
    inline constexpr size_t FLUSH_QUEUE_SIZE = 4096;
    

    template <typename T>
    constexpr bool IsPowerOfTwo(T x) {
        return x != 0 && (x & (x - 1)) == 0;
    }
    static_assert(IsPowerOfTwo(SHARD_SIZE), "(cache)SHARD_SIZE must be a power of two");
    static_assert(IsPowerOfTwo(MQ_SIZE), "(cache) MQ_SIZE must be a power of two");
    static_assert(IsPowerOfTwo(MQ_DEFER_SIZE), "(cache) MQ_DEFER_SIZE must be a power of two");
    static_assert(IsPowerOfTwo(DB_WORK_QUEUE_SIZE), "(cache) DB_WORK_QUEUE_SIZE must be a power of two");
    static_assert(IsPowerOfTwo(FLUSH_QUEUE_SIZE), "(cache) FLUSH_QUEUE_SIZE must be a power of two");
}
