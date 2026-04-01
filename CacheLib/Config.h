#pragma once
#include <cstdint>

namespace Cache {
    inline constexpr uint8_t DB_CONN_POOL = 3;
    inline constexpr uint8_t FLUSH_THREADPOOL_SIZE = 2;
    inline constexpr const char* DB_HOST = "localhost";
    inline constexpr const char* DB_USER = "root";
    inline constexpr const char* DB_PASS = "1234";
    inline constexpr const char* DB_DB = "game";

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

    inline constexpr uint16_t TARGET_MSGPOOL_SIZE = 50;
    inline constexpr uint16_t MAX_MSGPOOL_SIZE = 100;
    inline constexpr uint16_t MIN_MSGPOOL_SIZE = 20;

    inline constexpr uint16_t MESSGAGE_LEN = 1024;
    inline constexpr uint16_t THREADPOOL_SIZE = 100; 

    inline constexpr uint16_t MAX_CHARACTER_CNT = 10;
    inline constexpr uint16_t MAX_INVENTORY = 10;

    inline constexpr uint8_t MQ_THREADPOOL_SIZE = 3;

    inline constexpr size_t MQ_SIZE = 8192;
    

    template <typename T>
    constexpr bool IsPowerOfTwo(T x) {
        return x != 0 && (x & (x - 1)) == 0;
    }
    static_assert(IsPowerOfTwo(SHARD_SIZE), "(cache)SHARD_SIZE must be a power of two");
}
