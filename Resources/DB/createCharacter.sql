DELIMITER $$
DROP PROCEDURE IF EXISTS generate_characters $$
CREATE PROCEDURE generate_characters()
BEGIN
    DECLARE i INT DEFAULT 1;
    DECLARE zone INT;
    DECLARE x FLOAT;
    DECLARE y FLOAT;
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
        
        INSERT INTO characters (
            user_id, channel_id, name, zone_id,
            last_pos_x, last_pos_y,
            hp, maxHp, mp, maxMp,
            deleted_at
        ) VALUES (
            i, 1, CONCAT('char_', i), zone,
            x, y,
            10000, 10000, 10000, 10000,
            NULL
        );

        INSERT INTO characters_inventory (char_id, inventory)
        VALUES (LAST_INSERT_ID(), UNHEX(REPEAT('00', 2404)));

        SET i = i + 1;
    END WHILE;
END $$
DELIMITER ;
CALL generate_characters();