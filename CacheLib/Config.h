#pragma once
#include <cstdint>

namespace Cache {
    inline constexpr const uint8_t DB_CONN_POOL = 3;
    inline constexpr const uint8_t FLUSH_THREADPOOL_SIZE = 2;
    inline constexpr const char* DB_HOST = "localhost";
    inline constexpr const char* DB_USER = "root";
    inline constexpr const char* DB_PASS = "1234";
    inline constexpr const char* DB_DB = "game";

    inline constexpr const size_t MAX_CACHE_SIZE = 1024;
    inline constexpr const uint16_t SHARD_SIZE = 10;
    inline constexpr const char* QUERY_1 = "SELECT  * FROM v_user_characters WHERE user_id = ? and channel_id = ?";
    inline constexpr const char* QUERY_2 = "INSERT INTO characters (user_id, channel_id, name, zone_id, inventory, deleted_at) \
VALUES (?, ?, ?, ?, ?, NULL);";
    //INSERT INTO characters(user_id, channel_id, name, zone_id, inventory, deleted_at)
    //    VALUES(3, 1, 'KnightArthur', 1001, UNHEX(REPEAT('00', 2404)), NULL);
    // 현재 inventory struct 기준으로 300개 item으로 초기화
    inline constexpr const char* QUERY_3 = "SELECT char_id, name, level, exp, hp, mp, dir, zone_id, inventory, last_pos_x, last_pos_y FROM characters WHERE char_id = ?";
    inline constexpr const char* QUERY_4 = "UPDATE characters SET level = ?, exp = ?, hp = ?, mp = ?, dir =?, last_pos_x = ?, last_pos_y = ?, zone_id = ? WHERE char_id = ?;";
    inline constexpr const char* QUERY_5 = "SELECT inventory FROM characters WHERE char_id = ?";
    inline constexpr const char* QUERY_6 = "UPDATE characters SET inventory = ? WHERE char_id = ?;";

    inline constexpr const uint16_t TARGET_MSGPOOL_SIZE = 50;
    inline constexpr const uint16_t MAX_MSGPOOL_SIZE = 100;
    inline constexpr const uint16_t MIN_MSGPOOL_SIZE = 20;

    inline constexpr const uint16_t MESSGAGE_LEN = 1024;
    inline constexpr const uint16_t THREADPOOL_SIZE = 100; 

    inline constexpr const uint16_t MAX_CHARACTER_CNT = 10;
    inline constexpr const uint16_t MAX_INVENTORY = 10;

    inline constexpr const uint8_t MQ_THREADPOOL_SIZE = 3;
    
        
}
