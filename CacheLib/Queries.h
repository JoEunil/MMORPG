#pragma once

namespace Cache {
    inline constexpr const char* GAME_QUERY_1 = "SELECT  * FROM v_user_characters WHERE user_id = ? and channel_id = ?";
    inline constexpr const char* GAME_QUERY_2 = "INSERT INTO characters (user_id, channel_id, profile_id, zone_id, deleted_at) VALUES (?, ?, ?, ?, NULL);";
    inline constexpr const char* GAME_QUERY_3 =
        "SELECT c.char_id, c.profile_id, p.version AS profile_version, p.name, "
        "c.attack, c.level, c.exp, c.hp, c.mp, c.max_hp, c.max_mp, c.dir, c.zone_id, c.last_pos_x, c.last_pos_y "
        "FROM characters c JOIN profile p ON p.profile_id = c.profile_id "
        "WHERE c.char_id = ? AND c.user_id = ? AND c.channel_id = ? AND c.deleted_at IS NULL";
    inline constexpr const char* GAME_QUERY_4 = "UPDATE characters SET attack = ?, level = ?, exp = ?, hp = ?, mp = ?, max_hp = ?, max_mp = ?, dir =?, last_pos_x = ?, last_pos_y = ?, zone_id = ? WHERE char_id = ?;";
    inline constexpr const char* GAME_QUERY_5 = "SELECT char_id, inventory FROM characters_inventory WHERE char_id = ?";
    inline constexpr const char* GAME_QUERY_6 = "UPDATE characters_inventory SET inventory = ? WHERE char_id = ?;";

    inline constexpr const char* GAME_QUERY_7 = "SELECT gold FROM characters_currency WHERE char_id=?";
    inline constexpr const char* GAME_QUERY_8 = "INSERT INTO characters_currency (gold,char_id) VALUES (?,?) ON DUPLICATE KEY UPDATE gold=VALUES(gold)";

    inline constexpr const char* GAME_QUERY_9 = "INSERT INTO profile (name) VALUES (?)";
    inline constexpr const char* GAME_QUERY_10 = "SELECT profile_id, version, name FROM profile WHERE profile_id = ?";
    inline constexpr const char* GAME_QUERY_11 = "UPDATE profile SET name = ?, version = version + 1 WHERE profile_id = ?";

    inline constexpr const char* BAZAAR_QUERY_1 = "SELECT diamond, total_earned, total_spent FROM characters_diamond WHERE char_id = ?";
    inline constexpr const char* BAZAAR_QUERY_2 = "UPDATE characters_diamond SET diamond = diamond + ?, total_earned = total_earned + ?, updated_at = NOW() WHERE char_id = ?";
    inline constexpr const char* BAZAAR_QUERY_3 =
        "SELECT listing_id, item_id, seller_id, quantity, price, "
        "UNIX_TIMESTAMP(listed_at) AS listed_at, "
        "CASE status "
        "WHEN 'TRADING' THEN 0 "
        "WHEN 'SOLD'    THEN 1 "
        "WHEN 'CLAIMED' THEN 2 "
        "ELSE 255 "
        "END AS status "
        "FROM bazaar "
        "WHERE seller_id = ? AND status != 'CANCELLED'"
        "LIMIT ?";
    inline constexpr const char* BAZAAR_QUERY_4 =
        "SELECT listing_id, item_id, seller_id, quantity, price, "
        "UNIX_TIMESTAMP(listed_at) AS listed_at "
        "FROM bazaar "
        "WHERE item_type = ? AND status = 'TRADING' "
        "ORDER BY listed_at DESC "
        "LIMIT ? OFFSET ?";

    inline constexpr const char* BAZAAR_QUERY_5 =
        "INSERT INTO bazaar (item_id, seller_id, item_type, quantity, price, status, listed_at) "
        "VALUES(?, ?, ?, ?, ?, 'TRADING', NOW())";
    inline constexpr const char* BAZAAR_QUERY_6 = "CALL sp_bazaar_cancel(?, ?)";
    inline constexpr const char* BAZAAR_QUERY_7 = "SELECT item_id, quantity FROM bazaar WHERE listing_id = ?";
    inline constexpr const char* BAZAAR_QUERY_8 = "CALL sp_bazaar_buy(?, ?)";
    inline constexpr const char* BAZAAR_QUERY_9 = "CALL sp_bazaar_claim(?, ?)";
    inline constexpr const char* BAZAAR_QUERY_10 =
        "SELECT event_id, listing_id, item_id, item_type, quantity, price, "
        "UNIX_TIMESTAMP(purchased_at) AS purchased_at "
        "FROM buyer_outbox "
        "WHERE char_id = ? AND delivery_status = 'READY' "
        "ORDER BY purchased_at ASC, event_id ASC "
        "LIMIT 30";
    inline constexpr const char* BAZAAR_QUERY_11 =
        "UPDATE buyer_outbox "
        "SET delivery_status = 'CLAIMED', delivery_claimed_at = NOW() "
        "WHERE event_id = ? AND char_id = ? AND delivery_status = 'READY'";
}
