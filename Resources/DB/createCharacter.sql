USE game;

DELIMITER $$
DROP PROCEDURE IF EXISTS generate_characters $$
CREATE PROCEDURE generate_characters()
BEGIN
    DECLARE i INT DEFAULT 1;
    DECLARE zone INT;
    DECLARE x FLOAT;
    DECLARE y FLOAT;
    DECLARE new_profile_id INT UNSIGNED;
    DECLARE new_char_id BIGINT UNSIGNED;
    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        ROLLBACK;
        RESIGNAL;
    END;

    START TRANSACTION;

    WHILE i <= 5000 DO
        SET zone = ((i - 1) DIV 50) MOD 4 + 1;
        CASE zone
            WHEN 1 THEN 
                SET x = 0 + RAND() * 100;
                SET y = 0 + RAND() * 100;
            WHEN 2 THEN 
                SET x = 100 + RAND() * 100;
                SET y = 0 + RAND() * 100;
            WHEN 3 THEN 
                SET x = 200 + RAND() * 100;
                SET y = 0 + RAND() * 100;
            WHEN 4 THEN 
                SET x = 0 + RAND() * 100;
                SET y = 100 + RAND() * 100;
        END CASE;
        
        INSERT INTO profile (name) VALUES (CONCAT('char_', i));
        SET new_profile_id = LAST_INSERT_ID();

        INSERT INTO characters (
            user_id, channel_id, profile_id, zone_id,
            last_pos_x, last_pos_y,
            hp, max_hp, mp, max_mp,
            deleted_at
        ) VALUES (
            i, 1, new_profile_id, zone,
            x, y,
            10000, 10000, 10000, 10000,
            NULL
        );

        SET new_char_id = LAST_INSERT_ID();
        INSERT INTO characters_inventory (char_id, inventory)
        VALUES (new_char_id, UNHEX(REPEAT('00', 2404)));

        INSERT INTO characters_currency (char_id, gold)
        VALUES (new_char_id, 100000);

        INSERT INTO bazaar.characters_diamond (char_id, diamond)
        VALUES (new_char_id, 1000);

        SET i = i + 1;
    END WHILE;

    COMMIT;
END $$
DELIMITER ;
CALL generate_characters();
