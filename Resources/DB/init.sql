CREATE DATABASE login;
CREATE DATABASE game;
use login;

CREATE TABLE users (
    user_id BIGINT NOT NULL AUTO_INCREMENT,
    username VARCHAR(255) NOT NULL UNIQUE,
    email VARCHAR(255) NOT NULL UNIQUE,
    password VARCHAR(255) NOT NULL,
    phone_number VARCHAR(15),
    PRIMARY KEY (user_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

use game;

CREATE TABLE IF NOT EXISTS Characters (
    char_id BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT NOT NULL,
    channel_id INT NOT NULL,
    name VARCHAR(32) NOT NULL,
    level INT NOT NULL DEFAULT 1,
    exp INT NOT NULL DEFAULT 0,
    hp INT NOT NULL DEFAULT 10,
    mp INT NOT NULL DEFAULT 10,
    maxHp INT NOT NULL DEFAULT 10,
    maxMp INT NOT NULL DEFAULT 10,
    dir TINYINT NOT NULL DEFAULT 0,
    zone_id TINYINT NOT NULL DEFAULT 0,
    last_pos_x float NOT NULL DEFAULT 0,
    last_pos_y float NOT NULL DEFAULT 0,
    inventory BLOB,
    deleted_at DATETIME DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE INDEX idx_user_channel ON Characters(user_id, channel_id);

CREATE OR REPLACE VIEW v_user_characters AS
SELECT 
    c.user_id,
    c.channel_id,
    c.char_id,
    c.name,
    c.level
FROM Characters c
WHERE c.deleted_at IS NULL;


