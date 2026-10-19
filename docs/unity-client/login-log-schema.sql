-- Applied with user approval; retained as a record. Do not rerun against an existing table.
USE login;
CREATE TABLE login_log (
    log_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    login_id VARCHAR(255) NOT NULL,
    succeed BOOL NOT NULL,
    ip_addr    VARCHAR(45) NOT NULL,
    user_agent VARCHAR(512),
    logged_at  DATETIME DEFAULT NOW(),
    PRIMARY KEY (log_id),
    INDEX idx_login (login_id, logged_at DESC)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
