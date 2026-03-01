DELIMITER $$

DROP PROCEDURE IF EXISTS generate_characters $$
CREATE PROCEDURE generate_characters()
BEGIN
    DECLARE i INT DEFAULT 1;
    DECLARE zone INT;
    DECLARE x FLOAT;
    DECLARE y FLOAT;

    WHILE i <= 5000 DO

        -- 50명 단위로 Zone 1~4 반복
        SET zone = ((i - 1) DIV 50) MOD 4 + 1;

        -- Zone별 좌표 생성
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
				    inventory, deleted_at
				) VALUES (
				    i,
				    1,
				    CONCAT('char_', i),
				    zone,
				    x,
				    y,
				    10000, 10000, 10000, 10000, 
				    UNHEX(REPEAT('00', 2404)),
				    NULL
				);

        SET i = i + 1;
    END WHILE;

END $$

DELIMITER ;

CALL generate_characters();

