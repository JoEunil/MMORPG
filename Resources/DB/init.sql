CREATE DATABASE login;
CREATE DATABASE game;
CREATE DATABASE billing;
use login;

CREATE TABLE users (
    user_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    username VARCHAR(255) NOT NULL UNIQUE,
    email VARCHAR(255) NOT NULL UNIQUE,
    password VARCHAR(255) NOT NULL,
    phone_number VARCHAR(15),
    PRIMARY KEY (user_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

use game;

CREATE TABLE IF NOT EXISTS characters (
    char_id BIGINT UNSIGNED PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT UNSIGNED NOT NULL,
    channel_id INT UNSIGNED NOT NULL,
    name VARCHAR(32) NOT NULL,
    level INT UNSIGNED  NOT NULL DEFAULT 1,
    exp INT UNSIGNEDNOT NULL DEFAULT 0,
    hp INT NOT NULL DEFAULT 10000,
    mp INT NOT NULL DEFAULT 10000,
    maxHp INT NOT NULL DEFAULT 10000,
    maxMp INT NOT NULL DEFAULT 10000,
    dir TINYINT UNSIGNED NOT NULL DEFAULT 0,
    zone_id TINYINT UNSIGNED NOT NULL DEFAULT 0,
    last_pos_x float NOT NULL DEFAULT 0,
    last_pos_y float NOT NULL DEFAULT 0,

    deleted_at DATETIME DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE characters_inventory (
    char_id   BIGINT UNSIGNED NOT NULL,
    inventory BLOB,
    PRIMARY KEY (char_id)
)  ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE INDEX idx_user_channel ON characters(user_id, channel_id);

CREATE OR REPLACE VIEW v_user_characters AS
SELECT 
    c.user_id,
    c.channel_id,
    c.char_id,
    c.name,
    c.level
FROM characters c
WHERE c.deleted_at IS NULL;
ET=utf8mb4;

CREATE TABLE characters_currency (
    char_id BIGINT UNSIGNED PRIMARY KEY,
    gold    BIGINT UNSIGNED NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

use billing;

CREATE TABLE characters_diamond (
    char_id         BIGINT UNSIGNED PRIMARY KEY,
    diamond         INT    UNSIGNED NOT NULL DEFAULT 0,
    total_earned  BIGINT UNSIGNED NOT NULL DEFAULT 0,
    total_spent     BIGINT UNSIGNED NOT NULL DEFAULT 0,
    updated_at      DATETIME        NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE bazaar (
    listing_id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    item_id    BIGINT UNSIGNED NOT NULL,       
    seller_id  BIGINT UNSIGNED NOT NULL,
    buyer_id   BIGINT UNSIGNED,
    item_type  INT UNSIGNED NOT NULL,
    quantity   INT UNSIGNED NOT NULL DEFAULT 1, -- 수량 없음, 추가 필요
    price      BIGINT UNSIGNED NOT NULL,        -- 음수 불가
    status     ENUM('TRADING', 'SOLD', 'CANCELLED', 'CLAIMED') DEFAULT 'TRADING',
    listed_at  DATETIME NOT NULL,
    INDEX idx_status_listed (status, listed_at DESC),
    INDEX idx_seller        (seller_id, status),
    INDEX idx_item_type     (item_type, status, listed_at DESC) -- 정렬 조건은 뒤에 배치
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE bazaar_log (
    log_id     BIGINT AUTO_INCREMENT PRIMARY KEY,
    listing_id BIGINT UNSIGNED NOT NULL,
    seller_id  BIGINT UNSIGNED NOT NULL,
    buyer_id   BIGINT UNSIGNED NOT NULL,
    claim_status ENUM('READY', 'CLAIMED') NOT NULL DEFAULT 'READY',
    claimed_at DATETIME DEFAULT NULL,
    buyer_prev_quantity INT UNSIGNED DEFAULT NULL,
    item_type  TINYINT UNSIGNED NOT NULL,
quantity INT UNSIGNED NOT NULL DEFAULT 0,
    price      BIGINT UNSIGNED NOT NULL,
    sold_at    DATETIME NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
