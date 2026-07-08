#pragma once
#include <cstdint>

namespace Cache {
    inline constexpr uint8_t DB_CONN_POOL = 3;
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

    inline constexpr const char* QUERY_1 = "SELECT  * FROM v_user_characters WHERE user_id = ? and channel_id = ?";
    inline constexpr const char* QUERY_2 = "INSERT INTO characters (user_id, channel_id, name, zone_id, deleted_at) VALUES (?, ?, ?, ?, NULL);";
    inline constexpr const char* QUERY_3 = "SELECT char_id, name, attack, level, exp, hp, mp, max_hp, max_mp, dir, zone_id, last_pos_x, last_pos_y FROM characters WHERE char_id = ?";
    inline constexpr const char* QUERY_4 = "UPDATE characters SET attack = ?, level = ?, exp = ?, hp = ?, mp = ?, max_hp = ?, max_mp = ?, dir =?, last_pos_x = ?, last_pos_y = ?, zone_id = ? WHERE char_id = ?;";
    inline constexpr const char* QUERY_5 = "SELECT char_id, inventory FROM characters_inventory WHERE char_id = ?";
    inline constexpr const char* QUERY_6 = "UPDATE characters_inventory SET inventory = ? WHERE char_id = ?;";

    inline constexpr const char* QUERY_7 = "SELECT gold FROM characters_currency WHERE char_id=?";
    inline constexpr const char* QUERY_8 = "UPDATE characters_currency SET gold=? WHERE char_id=?";

    inline constexpr const char* QUERY_9 = "SELECT diamond, total_earned, total_spent FROM characters_diamond WHERE char_id = ?";
    inline constexpr const char* QUERY_10 = "UPDATE characters_diamond SET diamond = diamond + ?, total_earned = total_earned + ?, updated_at = NOW() WHERE char_id = ?";
    inline constexpr const char* QUERY_11 =
        "SELECT listing_id, item_id, seller_id, quantity, price, "
        "UNIX_TIMESTAMP(listed_at) AS listed_at, "
        "CASE status "
        "WHEN 'TRADING' THEN 0 "
        "WHEN 'SOLD'    THEN 1 "
        "ELSE 0 "
        "END AS status "
        "FROM bazaar " 
        "WHERE seller_id = ? AND status != 'CANCELLED'" 
        "LIMIT ?";
    inline constexpr const char* QUERY_12 =
        "SELECT listing_id, item_id, seller_id, quantity, price, " 
        "UNIX_TIMESTAMP(listed_at) AS listed_at " 
        "FROM bazaar " 
        "WHERE item_type = ? AND status = 'TRADING' " 
        "ORDER BY listed_at DESC " 
        "LIMIT ? OFFSET ?";

    inline constexpr const char* QUERY_13 = 
        "INSERT INTO bazaar (item_id, seller_id, item_type, quantity, price, status, listed_at) " 
        "VALUES(?, ?, ?, ?, ?, 'TRADING', NOW())";
    inline constexpr const char* QUERY_14 = 
        "UPDATE bazaar " 
        "SET status = 'CANCELLED' WHERE status = 'TRADING' AND listing_id = ? AND seller_id = ? ";
    inline constexpr const char* QUERY_15 = "SELECT item_id, quantity FROM bazaar WHERE listing_id = ?";
    inline constexpr const char* QUERY_16 = "CALL sp_bazaar_buy(?, ?, ?)";
    inline constexpr const char* QUERY_17 = "CALL sp_bazaar_claim(?, ?)";

    inline constexpr uint16_t MSGPOOL_SIZE = 100;

    inline constexpr uint16_t MESSAGE_LEN = 1024;
    inline constexpr uint16_t THREADPOOL_SIZE = 100; 

    inline constexpr uint16_t MAX_CHARACTER_CNT = 10;
    inline constexpr uint16_t MAX_INVENTORY = 10;

    inline constexpr uint8_t MQ_THREADPOOL_SIZE = 3;

    inline constexpr size_t MQ_SIZE = 8192;
    inline constexpr uint16_t GAME_DB_WORKER_THREADPOOL_SIZE = 2;
    inline constexpr uint16_t BAZAAR_DB_WORKER_THREADPOOL_SIZE = 1;
    

    template <typename T>
    constexpr bool IsPowerOfTwo(T x) {
        return x != 0 && (x & (x - 1)) == 0;
    }
    static_assert(IsPowerOfTwo(SHARD_SIZE), "(cache)SHARD_SIZE must be a power of two");
}
