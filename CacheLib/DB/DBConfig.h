#pragma once

#include <cstddef>
#include <cstdint>

#include "DBConnection.h"
#include "Queries.h"

namespace Cache {
    inline constexpr uint8_t DB_CONN_POOL = 10;
    inline constexpr uint16_t GAME_DB_WORKER_THREADPOOL_SIZE = 2;
    inline constexpr uint16_t BAZAAR_DB_WORKER_THREADPOOL_SIZE = 2;
    inline constexpr std::size_t DB_WORK_QUEUE_SIZE = 4096;
    static_assert(DB_WORK_QUEUE_SIZE != 0 && (DB_WORK_QUEUE_SIZE & (DB_WORK_QUEUE_SIZE - 1)) == 0,
        "(cache) DB_WORK_QUEUE_SIZE must be a power of two");

    inline constexpr const char* DB_HOST_GAME = "localhost";
    inline constexpr const char* DB_USER_GAME = "root";
    inline constexpr const char* DB_PASS_GAME = "1234";
    inline constexpr const char* DB_DB_GAME = "game";

    inline constexpr const char* DB_HOST_BAZAAR = "localhost";
    inline constexpr const char* DB_USER_BAZAAR = "root";
    inline constexpr const char* DB_PASS_BAZAAR = "1234";
    inline constexpr const char* DB_DB_BAZAAR = "bazaar";

    struct GameDbConfig {
        static constexpr auto host = DB_HOST_GAME;
        static constexpr auto user = DB_USER_GAME;
        static constexpr auto password = DB_PASS_GAME;
        static constexpr auto database = DB_DB_GAME;
        static constexpr auto& queries = GAME_QUERIES;
        static constexpr auto queryType = StatementType::GameQuery;
        static constexpr auto updateType = StatementType::GameUpdate;
    };

    struct BazaarDbConfig {
        static constexpr auto host = DB_HOST_BAZAAR;
        static constexpr auto user = DB_USER_BAZAAR;
        static constexpr auto password = DB_PASS_BAZAAR;
        static constexpr auto database = DB_DB_BAZAAR;
        static constexpr auto& queries = BAZAAR_QUERIES;
        static constexpr auto queryType = StatementType::BazaarQuery;
        static constexpr auto updateType = StatementType::BazaarUpdate;
    };

    using DBConnectionGame = DBConnection<GameDbConfig>;
    using DBConnectionBazaar = DBConnection<BazaarDbConfig>;
}
