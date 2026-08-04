use bazaar;
DELIMITER //

CREATE PROCEDURE sp_bazaar_buy(
    IN p_listing_id     BIGINT UNSIGNED,
    IN p_buyer_id       BIGINT UNSIGNED
)
BEGIN
    DECLARE v_seller_id BIGINT UNSIGNED DEFAULT 0;
    DECLARE v_item_id   BIGINT UNSIGNED DEFAULT 0;
    DECLARE v_item_type INT    UNSIGNED DEFAULT 0;
    DECLARE v_quantity  INT    UNSIGNED DEFAULT 0;
    DECLARE v_price     BIGINT UNSIGNED DEFAULT 0;
    DECLARE v_found     TINYINT        DEFAULT 0;

    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        ROLLBACK;
        SELECT 3 AS result, 0 AS seller_id, 0 AS item_id, 0 AS quantity, 0 AS price;
    END;

    START TRANSACTION;

    -- 리스팅 행 잠금 (동시 구매 직렬화)
    SELECT seller_id, item_id, item_type, quantity, price, 1
    INTO   v_seller_id, v_item_id, v_item_type, v_quantity, v_price, v_found
    FROM   bazaar
    WHERE  listing_id = p_listing_id AND status = 'TRADING' 
    FOR UPDATE;

    IF v_found = 0 THEN
        ROLLBACK;
        SELECT 0 AS result, 0 AS seller_id, 0 AS item_id, 0 AS quantity, 0 AS price;
    ELSE
        -- 다이아 차감 (부족/행없음이면 ROW_COUNT=0)
        UPDATE characters_diamond
        SET    diamond     = diamond - v_price,
               total_spent = total_spent + v_price,
               updated_at  = NOW()
        WHERE  char_id = p_buyer_id AND diamond >= v_price;

        IF ROW_COUNT() = 0 THEN
            ROLLBACK;
            SELECT 4 AS result, 0 AS seller_id, 0 AS item_id, 0 AS quantity, 0 AS price;
        ELSE
            UPDATE bazaar
            SET    status   = 'SOLD',
                   buyer_id = p_buyer_id
            WHERE  listing_id = p_listing_id AND status = 'TRADING';

            IF ROW_COUNT() = 0 THEN
                ROLLBACK;
                SELECT 0 AS result, 0 AS seller_id, 0 AS item_id, 0 AS quantity, 0 AS price;
            ELSE
                INSERT INTO bazaar_log
                    (listing_id, seller_id, buyer_id, item_id, item_type, quantity, price, sold_at)
                VALUES
                    (p_listing_id, v_seller_id, p_buyer_id, v_item_id, v_item_type, v_quantity, v_price, NOW());

                -- 판매자 정산 대기
                INSERT INTO bazaar_claim
                    (listing_id, seller_id, claim_status)
                VALUES
                    (p_listing_id, v_seller_id, 'READY');

                -- 구매자 우편함 배송
                INSERT INTO buyer_outbox
                    (char_id, listing_id, item_id, item_type, quantity, price, purchased_at, delivery_status)
                VALUES
                    (p_buyer_id, p_listing_id, v_item_id, v_item_type, v_quantity, v_price, NOW(), 'READY');

                COMMIT;
                SELECT 1 AS result, v_seller_id AS seller_id,
                       v_item_id AS item_id, v_quantity AS quantity, v_price AS price;
            END IF;
        END IF;
    END IF;
END //

CREATE PROCEDURE sp_bazaar_claim(
    IN p_listing_id BIGINT UNSIGNED,
    IN p_seller_id  BIGINT UNSIGNED
)
BEGIN
    DECLARE v_price BIGINT UNSIGNED DEFAULT 0;
    DECLARE v_found TINYINT         DEFAULT 0;

    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        ROLLBACK;
        SELECT 3 AS result, 0 AS diamond;
    END;

    START TRANSACTION;

    SELECT price, 1
    INTO   v_price, v_found
    FROM   bazaar
    WHERE  listing_id = p_listing_id
      AND  seller_id  = p_seller_id
      AND  status     = 'SOLD';

    IF v_found = 0 THEN
        ROLLBACK;
        SELECT 2 AS result, 0 AS diamond;
    ELSE
        -- 이중 정산 방지 (CAS)
        UPDATE bazaar_claim
        SET    claim_status = 'CLAIMED',
               claimed_at   = NOW()
        WHERE  listing_id   = p_listing_id
          AND  claim_status = 'READY';

        IF ROW_COUNT() = 0 THEN
            ROLLBACK;
            SELECT 4 AS result, 0 AS diamond;
        ELSE
            -- 판매자 다이아 지급: 행 없으면 생성 (정산금 손실 방지)
            INSERT INTO characters_diamond (char_id, diamond, total_earned, updated_at)
            VALUES (p_seller_id, v_price, v_price, NOW())
            ON DUPLICATE KEY UPDATE
                diamond      = diamond + v_price,
                total_earned = total_earned + v_price,
                updated_at   = NOW();

            COMMIT;
            SELECT 1 AS result, v_price AS diamond;
        END IF;
    END IF;
END //

DELIMITER ;